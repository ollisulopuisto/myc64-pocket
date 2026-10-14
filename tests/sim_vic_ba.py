# Usage: .venv/bin/python tests/sim_vic_ba.py [myc64-rtl dir]   (env PAT=R / RRRWWW: CPU read/write cycle pattern)
# VIC-only sim: per raster line, which real-C64 cycles have BA low / CPU half stolen.
import sys
import os.path
sys.path.insert(0, sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "fpga", "core", "myc64-rtl"))
from amaranth import *
from amaranth.sim import Simulator
from vicii import VicII

dut = VicII()
has_ba = hasattr(dut, 'o_ba')
results = {}
import os
PAT = os.environ.get('PAT', 'RRRWWW')
cpu = dict(i=0, done=0, rdy_mask=1, wstall=0)

# Scenarios: (name, d015, sprite y for enabled sprites); target raster 0x40 for sprites,
# bad line at raster 0x33 (d011 yscroll=3), sprite lines chosen off bad lines.
SCEN = [
  ("bad line only", 0x00, 0x00),
  ("sprite 0", 0x01, 0x80),
  ("sprites 0-7", 0xff, 0x80),
  ("sprites 2+5", 0x24, 0x80),
  ("sprites 0+3", 0x09, 0x80),
]

async def bench(ctx):
  tk = 0
  async def tick():
    nonlocal tk
    # --- CPU model (before the edge) ---
    stun = ctx.get(dut.o_steal_bus); ba = ctx.get(dut.o_ba) if has_ba else 1
    if tk == 0:
      cur_w = PAT[cpu['i'] % len(PAT)] == 'W'
      if (not stun) and cpu['rdy_mask'] and (ba or cur_w):
        cpu['i'] += 1; cpu['done'] += 1
    if 0 <= tk < 4 and stun and PAT[cpu['i'] % len(PAT)] == 'W':
      cpu['wstall'] += 1
    if stun: cpu['rdy_mask'] = 0
    elif tk == 4: cpu['rdy_mask'] = 1
    ctx.set(dut.clk_1mhz_ph1_en, tk == 0)
    ctx.set(dut.clk_1mhz_ph2_en, tk == 4)
    await ctx.tick()
    tk = (tk + 1) % 8
  async def wreg(a, v):
    # wait for a ph2 tick and write
    while tk != 4:
      await tick()
    ctx.set(dut.i_reg_cs, 1); ctx.set(dut.i_reg_we, 1); ctx.set(dut.i_reg_addr, a); ctx.set(dut.i_reg_data, v)
    await tick()
    ctx.set(dut.i_reg_cs, 0); ctx.set(dut.i_reg_we, 0)
  ctx.set(dut.clk_8mhz_en, 1)
  ctx.set(dut.i_data, 0)
  await wreg(0x11, 0x1b)
  for name, d015, sy in SCEN:
    await wreg(0x15, 0)
    for n in range(8):
      await wreg(1 + 2 * n, sy)
    # run to a raster line well before target, then enable sprites
    # track raster via hsync count: run 2 frames worth is too slow; just watch line count.
    line = None
    # wait for a line start (hsync)
    while not ctx.get(dut.o_hsync):
      await tick()
    await wreg(0x15, d015)
    rows = {}
    cur = [0]
    cpudone = {}
    cpu['wstall'] = 0
    cyc = 0  # core cycle number (1..63), core cycle 1 starts at the hsync tick
    lines = 0
    target_lines = 312 + 2
    while lines < target_lines:
      if ctx.get(dut.o_hsync):
        cyc = 0; lines += 1
      if tk == 0:
        cyc += 1
      if tk == 2:  # middle of CPU half of core cycle cyc
        raster = ctx.get(dut.o_dbg_raster) if hasattr(dut, 'o_dbg_raster') else None
        steal = ctx.get(dut.o_steal_bus)
        ba = ctx.get(dut.o_ba) if has_ba else 1
        rc = 63 if cyc == 1 else cyc - 1
        if rc == 1: cur[0] += 1
        if rc == 1:
          cpudone.setdefault(cur[0]-1, cpu['done'] - cpu.get('mark', 0)); cpu['mark'] = cpu['done']
        rows.setdefault(cur[0], []).append((rc, steal, ba))
      await tick()
    results[name] = (rows, cpudone, cpu['wstall'])

sim = Simulator(dut)
sim.add_clock(1e-6 / 8)
sim.add_testbench(bench)
sim.run()

def real(c):  # core CPU half of cycle c == real cycle c-1 phase 2
  return 63 if c == 1 else c - 1

def ranges(lst):
  lst = sorted(lst); out = []; 
  for v in lst:
    if out and v == out[-1][1] + 1: out[-1][1] = v
    else: out.append([v, v])
  return ",".join(f"{a}-{b}" if a != b else f"{a}" for a, b in out) or "-"

for name, (rows, cpudone, wst) in results.items():
  print(f"== {name}   (CPU pattern {PAT}; write cycles that hit an AEC half: {wst})")
  pats = {}
  for ln, r in rows.items():
    if len(r) != 63: continue
    st = [c for c, s, b in r if s]
    bl = [c for c, s, b in r if not b]
    key = (ranges(bl), len(bl), ranges(st), len(st), cpudone.get(ln))
    pats[key] = pats.get(key, 0) + 1
  for (b, nb, st, ns, cd), cnt in pats.items():
    if nb or ns:
      print(f"  x{cnt:3d}: BA low {b:20s} ({nb:2d}) | AEC (CPU phase) {st:22s} ({ns:2d}) | CPU cycles done {cd}")
