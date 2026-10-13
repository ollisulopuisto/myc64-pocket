#!/bin/sh
# Menu readback (Icarus): the interact write/readback block and the bridge read mux, cut from core_top.v, against
# the Pocket's read-modify-write of every setting. Read data is sampled at the next read (as for savestates).
set -eu
H=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
F=$H/../../src/fpga/core/core_top.v
B=$(mktemp -d)
trap '/bin/rm -rf "$B"' EXIT HUP INT TERM
s=$(($(grep -n "// Values written by APF during core boot" "$F" | cut -d: -f1) - 1))
e=$(($(grep -n "^  reg \[8:0\] video_pos_x;" "$F" | cut -d: -f1) - 1))
sed -n "${s},${e}p" "$F" > "$B/ia_block.vh"
m=$(grep -n "^  always @(\*) begin" "$F" | head -1 | cut -d: -f1)
sed -n "${m},$((m + 15))p" "$F" > "$B/mux.vh"
iverilog -g2005 -I "$B" -o "$B/sim" "$H/tb.v"
vvp -N "$B/sim" | grep -v finish
