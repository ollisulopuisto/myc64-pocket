# Round-trip test for the savestate scan chain (src/fpga/core/myc64-rtl/savestate.py):
# saving (rotating the chain) must leave every register unchanged, and shifting
# saved words back in must restore the exact state, also with the clock enable low.
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "fpga", "core", "myc64-rtl"))

from amaranth import *
from amaranth.hdl import EnableInserter
from amaranth.hdl._ir import Fragment
from amaranth.sim import Simulator

from savestate import add_scan_chain, register_capture


class Counter(Elaboratable):
  def __init__(self, width, step):
    self.width, self.step = width, step
    self.value = Signal(width)
    self.extra = Signal(3)

  def elaborate(self, platform):
    m = Module()
    m.d.sync += self.value.eq(self.value + self.step)
    with m.If(self.value[0]):
      m.d.sync += self.extra.eq(self.extra + 1)
    return m


class Top(Elaboratable):
  def __init__(self):
    self.i_ce = Signal()
    self.i_ss_shift = Signal()
    self.i_ss_si = Signal(32)
    self.o_ss_so = Signal(32)
    self.i_ss_capture = Signal()
    self.o_ss_words = Signal(16)
    self.o_ss_hash = Signal(32)
    self.probe = Signal(13)
    self.shadow = Signal(13, name="ss_cpu_shadow")

  def elaborate(self, platform):
    m = Module()
    m.submodules.a = a = Counter(17, 3)
    m.submodules.b = b = Counter(40, 7)
    m.submodules.c = c = Counter(5, 1)
    m.d.sync += self.probe.eq(self.probe ^ a.value[0:13])
    m.d.sync += self.shadow.eq(self.shadow)
    register_capture(self.shadow, a.value[0:13])
    self.counters = [a, b, c]
    return m


def test_round_trip():
  top = Top()
  frag = Fragment.get(EnableInserter({"sync": top.i_ce})(top), None)
  words, _ = add_scan_chain(frag, top.i_ss_shift, top.i_ss_si, top.o_ss_so, top.i_ss_capture,
                            top.o_ss_words, top.o_ss_hash, os.devnull)
  regs = [top.probe, top.shadow] + [s for c in top.counters for s in (c.value, c.extra)]

  sim = Simulator(frag)
  sim.add_clock(1e-6)

  async def bench(ctx):
    ctx.set(top.i_ce, 1)
    for _ in range(37):
      await ctx.tick()
    ctx.set(top.i_ce, 0)
    ctx.set(top.i_ss_capture, 1)
    await ctx.tick()
    ctx.set(top.i_ss_capture, 0)
    before = [ctx.get(r) for r in regs]
    assert ctx.get(top.shadow) == ctx.get(top.counters[0].value) & 0x1fff

    # Save: rotate, feeding so back into si
    saved = []
    ctx.set(top.i_ss_shift, 1)
    for _ in range(words):
      w = ctx.get(top.o_ss_so)
      saved.append(w)
      ctx.set(top.i_ss_si, w)
      await ctx.tick()
    ctx.set(top.i_ss_shift, 0)
    await ctx.tick()
    assert [ctx.get(r) for r in regs] == before, "rotation changed the state"

    # Run on, then load the saved words back
    ctx.set(top.i_ce, 1)
    for _ in range(11):
      await ctx.tick()
    ctx.set(top.i_ce, 0)
    assert [ctx.get(r) for r in regs] != before
    ctx.set(top.i_ss_shift, 1)
    for w in saved:
      ctx.set(top.i_ss_si, w)
      await ctx.tick()
    ctx.set(top.i_ss_shift, 0)
    await ctx.tick()
    assert [ctx.get(r) for r in regs] == before, "load did not restore the state"

  sim.add_testbench(bench)
  sim.run()
  print("scan chain round trip OK (%d words)" % words)


if __name__ == "__main__":
  test_round_trip()
