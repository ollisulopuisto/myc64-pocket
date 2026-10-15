#!/usr/bin/env python3
"""Generate openFPGA instance JSONs for the MyC64 core.

usage: build_instances_c64.py <common_dir> <out_dir>
  common_dir = .../Assets/c64/common            (CRT/PRG/G64/D64 files, flat or in subfolders)
  out_dir    = .../Assets/c64/markus-zzz.MyC64   (instance JSONs, one folder per first letter)
Slots: 0 = CRT, 1 = PRG, 2 = G64, 3 = D64 (none core-specific: filenames are relative to common_dir).
"""
import json, os, re, sys

common, out = sys.argv[1:3]
SLOT = {".crt": 0, ".prg": 1, ".g64": 2, ".d64": 3}

def inst(slots):
    return {"instance": {"magic": "APF_VER_1", "data_path": "",
                         "data_slots": [{"id": i, "filename": f} for i, f in slots]}}

def write(rel, slots):
    p = os.path.join(out, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w") as fh:
        json.dump(inst(slots), fh, indent=1)
        fh.write("\n")

count = 0
seen = set()
for root, dirs, files in os.walk(common):
    dirs[:] = sorted(d for d in dirs if not d.startswith("."))
    for f in sorted(files):
        if f.startswith("."): continue
        stem, ext = os.path.splitext(f)
        if ext.lower() not in SLOT: continue
        rel = os.path.relpath(os.path.join(root, f), common).replace(os.sep, "/")
        letter = stem[0].upper() if stem[0].isalpha() else "#"
        name = re.sub(r'[\\/:*?"<>|]', "-", stem)[:90]
        key = (letter, name.lower())
        if key in seen: name += f" [{ext[1:].lower()}]"; key = (letter, name.lower())
        seen.add(key)
        write(f"{letter}/{name}.json", [(SLOT[ext.lower()], rel)])
        count += 1
write("00 Boot without disk.json", [])
print("instances:", count)
