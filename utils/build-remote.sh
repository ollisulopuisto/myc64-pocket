#!/bin/bash
#
# Build on a remote Windows host that has Quartus, over SSH.
#
# The BIOS (clang/lld with RISC-V) and the Amaranth Verilog are made locally,
# src/fpga is sent to the host with tar, Quartus compiles there, and the
# bitstream comes back and is packaged like CI (MyC64-Pocket.zip).
#
#   utils/build-remote.sh                 build and package
#   utils/build-remote.sh MyC64Snd        also install to the SD card as
#                                         Cores/markus-zzz.MyC64Snd
#
# Environment (defaults in brackets):
#   BUILD_HOST   ssh host                          [ristisana]
#   QUARTUS_SH   quartus_sh.exe on the host        [E:\altera\quartus\bin64\quartus_sh.exe]
#   REMOTE_DIR   build dir under the remote home   [myc64-build]
#   LLVM_BIN     clang/ld.lld/llvm-objcopy dir     [/opt/homebrew/opt/llvm/bin]
#   PYTHON       python with amaranth 0.5.10       [.venv/bin/python, else python3]
#                (python3 -m venv .venv && .venv/bin/pip install "amaranth==0.5.10"
#                 "amaranth-yosys==0.40.0.0.post103")
#   POCKET       SD card mount                     [/Volumes/Analogue]
#

set -euo pipefail

HOST=${BUILD_HOST:-ristisana}
QUARTUS=${QUARTUS_SH:-'E:\altera\quartus\bin64\quartus_sh.exe'}
REMOTE_DIR=${REMOTE_DIR:-myc64-build}
LLVM=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
POCKET=${POCKET:-/Volumes/Analogue}
INSTALL_AS=${1:-}

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
if [ -n "${PYTHON:-}" ]; then PY=$PYTHON
elif [ -x .venv/bin/python ]; then PY=$ROOT/.venv/bin/python
else PY=python3; fi
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
VERSION=$(git rev-parse --short HEAD)$(git diff --quiet HEAD -- src dist || echo "+")
START=$(date +%s)

echo "== BIOS"
cp -R src/bios "$WORK/bios"
PATH="$LLVM:$PATH" make -s -C "$WORK/bios" clean all >/dev/null

echo "== Verilog"
(cd src/fpga/core/myc64-rtl && "$PY" myc64.py)
(cd src/fpga/core/my1541-rtl && "$PY" my1541.py)

echo "== Quartus on $HOST"
# Shared host: jobs go through the shared launcher E:\fpga-shared\qrun.ps1
# (rules in E:\fpga-shared\RULES.txt, lead: the Pocket PSX session). It admits
# the job when there is room, runs it detached from ssh and writes the exit
# code to <log>.done; we poll that. Session name: pocket-c64.
mkdir -p "$WORK/src"
cp -R src/fpga "$WORK/src/fpga"
rm -rf "$WORK/src/fpga/output_files" "$WORK/src/fpga/db" "$WORK/src/fpga/incremental_db"
rm -f "$WORK/src/fpga/bios.vh"  # symlink to ../bios/bios.vh
cp "$WORK/bios/bios.vh" "$WORK/src/fpga/bios.vh"
ssh "$HOST" "if exist $REMOTE_DIR\\fpga\\output_files rmdir /s /q $REMOTE_DIR\\fpga\\output_files"
COPYFILE_DISABLE=1 tar -C "$WORK/src" -czf - fpga | ssh "$HOST" "(if not exist $REMOTE_DIR mkdir $REMOTE_DIR) & tar -xzf - -C $REMOTE_DIR"
QOUT=$(ssh "$HOST" "powershell -NoProfile -ExecutionPolicy Bypass -File E:\\fpga-shared\\qrun.ps1 -Session pocket-c64 -WorkDir %USERPROFILE%\\$REMOTE_DIR\\fpga -Command \"quartus_sh --flow compile ap_core.qpf\"" | tr -d '\r' | tail -1)
echo "$QOUT"
case "$QOUT" in STARTED*) ;; *) echo "qrun did not start the job"; exit 1 ;; esac
QLOG=$(printf "%s\n" "$QOUT" | sed -n 's/.* log=\([^ ]*\).*/\1/p')  # unique qrun-<timestamp>.log
until CODE=$(ssh "$HOST" "type $QLOG.done" 2>/dev/null | tr -dc 0-9) && [ -n "$CODE" ]; do sleep 20; done
ssh "$HOST" "type $QLOG" 2>/dev/null | tr -d '\r' > "$WORK/compile.log"
if [ "$CODE" != 0 ]; then
  ssh "$HOST" "findstr /b /c:Error $REMOTE_DIR\\fpga\\output_files\\ap_core.*.rpt" 2>/dev/null | tr -d '\r' | head -20
  exit 1
fi
mkdir -p src/fpga/output_files
for f in ap_core.rbf ap_core.fit.summary ap_core.sta.summary; do
  scp -q "$HOST:$REMOTE_DIR/fpga/output_files/$f" src/fpga/output_files/
done
cp "$WORK/compile.log" src/fpga/output_files/compile-remote.log
grep -E "Logic utilization|Total RAM Blocks" src/fpga/output_files/ap_core.fit.summary
grep -A1 "Setup '" src/fpga/output_files/ap_core.sta.summary | grep Slack | sort -t: -k2 -n | head -1

echo "== Package"
STAGING="$WORK/staging"
cp -R dist "$STAGING"
rm -f "$STAGING/Cores/markus-zzz.MyC64/bitstream.rbf_r"
"$PY" utils/reverse-bits.py src/fpga/output_files/ap_core.rbf "$STAGING/Cores/markus-zzz.MyC64/bitstream.rbf_r"
CORE_JSON="$STAGING/Cores/markus-zzz.MyC64/core.json"
sed -i '' -e "s/VERSION/$VERSION/" -e "s/DATE_RELEASE/$(date +%Y-%m-%d)/" "$CORE_JSON"
rm -f MyC64-Pocket.zip
(cd "$STAGING" && zip -qr "$ROOT/MyC64-Pocket.zip" .)
echo "MyC64-Pocket.zip ($VERSION)"

if [ -n "$INSTALL_AS" ]; then
  echo "== Install as markus-zzz.$INSTALL_AS"
  [ -d "$POCKET/Cores" ] || { echo "SD card not mounted at $POCKET"; exit 1; }
  CORE="$STAGING/Cores/markus-zzz.MyC64"
  sed -i '' "s/\"shortname\": \"MyC64\"/\"shortname\": \"$INSTALL_AS\"/" "$CORE/core.json"
  mkdir -p "$POCKET/Cores/markus-zzz.$INSTALL_AS"
  cp "$CORE"/* "$POCKET/Cores/markus-zzz.$INSTALL_AS/"
  cp -n "$STAGING/Platforms/c64.json" "$POCKET/Platforms/" 2>/dev/null || true
  sync
fi

echo "Done in $(( $(date +%s) - START )) s"
