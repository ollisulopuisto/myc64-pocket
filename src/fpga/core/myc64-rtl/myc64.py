#
# Copyright (C) 2020-2024 Markus Lavin (https://www.zzzconsulting.se/)
#
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# yapf --in-place --recursive --style="{indent_width: 2, column_limit: 120}"

from amaranth import *
from cpu6510 import Cpu6510
from cia import Cia
from vicii import VicII

class Cartridge(Elaboratable):
  def __init__(self):
    self.i_cart_type = Signal(3)

    self.i_addr = Signal(16)
    self.i_we = Signal()
    self.i_data = Signal(8)
    self.o_data = Signal(8)

    self.i_io1 = Signal()
    self.i_io2 = Signal()
    self.i_roml = Signal()
    self.i_romh = Signal()

    self.o_exrom = Signal()
    self.o_game = Signal()

    self.o_nmi = Signal()

    self.o_mem_addr = Signal(21)
    self.o_mem_we = Signal()
    self.o_mem_data = Signal(8)
    self.i_mem_data = Signal(8)
    self.o_mem_ram = Signal()  # o_mem_addr[0:15] is cartridge RAM (core_top BRAM, in savestates)

    self.i_freeze = Signal()  # freeze button (level)
    self.o_io1_drive = Signal()  # cartridge answers reads in IO1 ($DExx)
    self.o_active = Signal()  # cartridge maps something into the C64

  def elaborate(self, platform):
    m = Module()

    reg_de00 = Signal(8)
    reg_de02 = Signal(8)

    freeze_p = Signal()
    m.d.sync += freeze_p.eq(self.i_freeze)
    freeze_rise = self.i_freeze & ~freeze_p

    m.d.comb += [self.o_exrom.eq(1), self.o_game.eq(1)]
    m.d.comb += [self.o_mem_we.eq(0), self.o_mem_ram.eq(0)]
    m.d.comb += [self.o_data.eq(self.i_mem_data), self.o_mem_data.eq(self.i_data)]
    m.d.comb += [self.o_io1_drive.eq(0), self.o_nmi.eq(0), self.o_active.eq(self.i_cart_type != 0)]

    # Retro Replay state (see VICE src/c64/cart/retroreplay.c)
    rr_active = Signal(init=1)
    rr_frozen = Signal()
    rr_bank = Signal(3)
    rr_cmode = Signal(2)  # bit 0: GAME asserted, bit 1: EXROM deasserted
    rr_export_ram = Signal()
    rr_allow_bank = Signal()
    rr_no_freeze = Signal()
    rr_reu_map = Signal()
    rr_write_once = Signal()

    with m.Switch(self.i_cart_type):
      with m.Case(0): # No cartridge
        pass

      with m.Case(1): # Magic Desk
        with m.If(self.i_io1 & self.i_we & (self.i_addr[0:8] == 0x00)):
          m.d.sync += reg_de00.eq(self.i_data)
        with m.If(~reg_de00[7]):
          m.d.comb += [self.o_exrom.eq(0), self.o_game.eq(1)]
        m.d.comb += self.o_mem_addr.eq(Cat(self.i_addr[0:13], reg_de00[0:7]))

      with m.Case(2): # EasyFlash
        m.d.comb += [self.o_game.eq(Mux(reg_de02[2], ~reg_de02[0], 0b0)), self.o_exrom.eq(~reg_de02[1])]
        with m.If(self.i_io1 & self.i_we):
          with m.If(self.i_addr[0:8] == 0x00):
            m.d.sync += reg_de00.eq(self.i_data)
          with m.If(self.i_addr[0:8] == 0x02):
            m.d.sync += reg_de02.eq(self.i_data)

        m.d.comb += self.o_mem_addr.eq(Cat(self.i_addr[0:13], reg_de00[0:6], self.i_romh))
        with m.If(self.i_io2):
          m.d.comb += [self.o_mem_addr.eq(0x100000 + self.i_addr[0:8]), self.o_mem_we.eq(self.i_we),
                       self.o_mem_ram.eq(1)]

      with m.Case(3): # Action Replay
        with m.If(~reg_de00[2]):
          m.d.comb += [self.o_exrom.eq(reg_de00[1]), self.o_game.eq(~reg_de00[0])]
          with m.If(self.i_io1 & self.i_we & (self.i_addr[0:8] == 0x00)):
            m.d.sync += reg_de00.eq(self.i_data)
        m.d.comb += self.o_mem_addr.eq(Cat(self.i_addr[0:13], reg_de00[3:5]))

        with m.If(reg_de00[5] & self.i_roml):
          m.d.comb += [self.o_mem_addr.eq(0x10000 + self.i_addr[0:13]), self.o_mem_we.eq(self.i_we),
                       self.o_mem_ram.eq(1)]
        with m.If(self.i_io2): #reg_de00[5] &
          m.d.comb += [self.o_mem_addr.eq(0x11f00 + self.i_addr[0:8]), self.o_mem_we.eq(self.i_we),
                       self.o_mem_ram.eq(1)]

        # Freeze: NMI while the button is held, Ultimax mode with ROM bank 0
        m.d.comb += self.o_nmi.eq(self.i_freeze)
        with m.If(freeze_rise):
          m.d.sync += reg_de00.eq(0x03)

      with m.Case(4): # Retro Replay (64K flash, 32K RAM; no flash mode, no Nordic Replay)
        cmode = Signal(2)
        m.d.comb += cmode.eq(Mux(rr_frozen, 0b11, rr_cmode))
        ultimax = cmode == 0b11
        game16k = cmode == 0b01
        m.d.comb += self.o_active.eq(rr_active)
        with m.If(rr_active):
          m.d.comb += [self.o_game.eq(~cmode[0]), self.o_exrom.eq(cmode[1])]

        rom_bank_addr = Cat(self.i_addr[0:13], rr_bank)  # 64K ROM at 0x00000
        ram_bank = Mux(rr_allow_bank, rr_bank[0:2], 0)
        m.d.comb += self.o_mem_addr.eq(rom_bank_addr)

        # $8000-$9FFF (ROML): RAM or ROM; writes reach RAM in Ultimax mode
        with m.If(self.i_roml):
          with m.If(rr_export_ram):
            m.d.comb += [self.o_mem_addr.eq(0x10000 + Cat(self.i_addr[0:13], rr_bank[0:2])),
                         self.o_mem_we.eq(self.i_we & ultimax), self.o_mem_ram.eq(1)]
        # ROMH: with RAM selected and AllowBank clear, bank bits 0-1 are ignored
        with m.If(self.i_romh & rr_export_ram & ~rr_allow_bank):
          m.d.comb += [self.o_mem_addr.eq(Cat(self.i_addr[0:13], C(0, 2), rr_bank[2])), self.o_mem_ram.eq(0)]

        # IO2 ($DFxx; $DExx with the REU compatible map): last page of the bank
        io_ram = Signal()
        io_page = Signal(16)
        m.d.comb += [io_ram.eq(rr_active & ~rr_frozen & rr_export_ram),
                     io_page.eq(Mux(rr_reu_map, 0x1e00, 0x1f00) + self.i_addr[0:8])]
        io_sel = Mux(rr_reu_map, self.i_io1 & (self.i_addr[0:8] >= 2), self.i_io2)
        with m.If(io_sel):
          with m.If(io_ram):
            m.d.comb += [self.o_mem_addr.eq(0x10000 + Cat(io_page[0:13], ram_bank)),
                         self.o_mem_we.eq(self.i_we), self.o_mem_ram.eq(1)]
          with m.Else():
            m.d.comb += [self.o_mem_addr.eq(Cat(io_page[0:13], rr_bank)), self.o_mem_ram.eq(0)]
          with m.If(rr_reu_map & rr_active & ~rr_frozen & (io_ram | ~(ultimax | game16k))):
            m.d.comb += self.o_io1_drive.eq(1)

        # $DE00/$DE01 read: status
        with m.If(self.i_io1 & rr_active & (self.i_addr[1:8] == 0)):
          m.d.comb += [self.o_io1_drive.eq(1),
                       self.o_data.eq(Cat(C(0, 1), rr_allow_bank, self.i_freeze, rr_bank[0:2], C(0, 1),
                                          rr_reu_map, rr_bank[2]))]

        # Register writes
        with m.If(self.i_io1 & self.i_we & rr_active):
          with m.If(self.i_addr[0:8] == 0x00):
            m.d.sync += [rr_bank.eq(Cat(self.i_data[3:5], self.i_data[7])),
                         rr_cmode.eq(self.i_data[0:2]),
                         rr_export_ram.eq(self.i_data[5])]
            with m.If(self.i_data[6]):
              m.d.sync += rr_frozen.eq(0)
            with m.If(self.i_data[2]):
              m.d.sync += rr_active.eq(0)
          with m.If(self.i_addr[0:8] == 0x01):
            m.d.sync += rr_bank.eq(Cat(self.i_data[3:5], self.i_data[7]))
            with m.If(~rr_write_once):
              m.d.sync += [rr_allow_bank.eq(self.i_data[1]), rr_no_freeze.eq(self.i_data[2]),
                           rr_reu_map.eq(self.i_data[6]), rr_write_once.eq(1)]

        # Freeze: NMI while held, then Ultimax until $DE00 bit 6 is written
        with m.If(~rr_no_freeze):
          m.d.comb += self.o_nmi.eq(self.i_freeze)
          with m.If(freeze_rise):
            m.d.sync += [rr_active.eq(1), rr_frozen.eq(1)]

    return m



class MyC64(Elaboratable):
  def __init__(self):
    self.i_freeze = Signal()  # cartridge freeze button
    self.i_color_data = Signal(4)  # color RAM (1K x 4 at o_bus_addr[0:10])
    self.o_color_data = Signal(4)
    self.o_color_we = Signal()
    self.o_cart_active = Signal()
    self.i_clk_1mhz_ph1_en = Signal()
    self.i_clk_1mhz_ph2_en = Signal()

    self.o_vid_rgb = Signal(24)
    self.o_vid_hsync = Signal()
    self.o_vid_vsync = Signal()
    self.o_vid_en = Signal()
    self.i_sid_data = Signal(8)
    self.o_sid_cs = Signal()
    self.o_sid2_cs = Signal()
    self.o_sid_we = Signal()
    self.o_sid_addr = Signal(5)
    self.o_sid_data = Signal(8)
    self.i_keyboard_mask = Signal(64)
    self.i_joystick1 = Signal(7) # UP,DOWN,LEFT,RIGHT,FIRE1,FIRE2,FIRE3
    self.i_joystick2 = Signal(7)

    self.o_bus_addr = Signal(16)
    self.i_rom_char_data = Signal(8)
    self.i_rom_basic_data = Signal(8)
    self.i_rom_kernal_data = Signal(8)
    self.i_ram_main_data = Signal(8)
    self.o_ram_main_data = Signal(8)
    self.o_ram_main_we = Signal()

    self.o_iec_atn_out = Signal()
    self.i_iec_data_in = Signal()
    self.o_iec_data_out = Signal()
    self.i_iec_clock_in = Signal()
    self.o_iec_clock_out = Signal()

    self.i_cart_type = Signal(3)
    self.o_cart_addr = Signal(21)
    self.o_cart_we = Signal()
    self.i_cart_data = Signal(8)
    self.o_cart_data = Signal(8)
    self.o_cart_ram = Signal()

    self.o_debug_6510_valid = Signal()
    self.o_debug_6510_sync = Signal()
    self.o_debug_6510_addr = Signal(16)
    self.o_debug_6510_data = Signal(8)
    self.o_debug_6510_regs = Signal(64)

    # Clock enable for the whole design (EnableInserter in __main__). core_top
    # runs the system clock faster than the emulated 8 MHz and uses this to
    # step it at 8 MHz normally, or every cycle in warp mode.
    self.i_ce = Signal()

    # Savestate scan chain (driven by savestate.add_scan_chain in __main__)
    self.i_ss_shift = Signal()
    self.i_ss_si = Signal(32)
    self.o_ss_so = Signal(32)
    self.i_ss_capture = Signal()
    self.i_ss_cpu_load = Signal()
    self.o_ss_words = Signal(16)
    self.o_ss_hash = Signal(32)

    self.ports = [
        self.i_ce,
        self.i_ss_shift, self.i_ss_si, self.o_ss_so, self.i_ss_capture, self.i_ss_cpu_load, self.o_ss_words, self.o_ss_hash,
        self.i_clk_1mhz_ph1_en, self.i_clk_1mhz_ph2_en,
        self.o_vid_rgb, self.o_vid_hsync, self.o_vid_vsync, self.o_vid_en,
        self.i_sid_data, self.o_sid_cs, self.o_sid2_cs, self.o_sid_we, self.o_sid_addr, self.o_sid_data,
        self.i_keyboard_mask, self.i_joystick1, self.i_joystick2,
        self.o_bus_addr, self.i_rom_char_data, self.i_rom_basic_data, self.i_rom_kernal_data,
        self.i_ram_main_data, self.o_ram_main_data, self.o_ram_main_we,
        self.o_iec_atn_out, self.i_iec_data_in , self.o_iec_data_out, self.i_iec_clock_in, self.o_iec_clock_out,
        self.i_cart_type, self.i_freeze, self.o_cart_active, self.i_color_data, self.o_color_data, self.o_color_we, self.o_cart_addr, self.o_cart_we, self.i_cart_data, self.o_cart_data, self.o_cart_ram,
        self.o_debug_6510_valid, self.o_debug_6510_sync, self.o_debug_6510_addr, self.o_debug_6510_data, self.o_debug_6510_regs
    ]

  def elaborate(self, platform):
    m = Module()

    vic_cycle = Signal()
    with m.If(self.i_clk_1mhz_ph1_en):
      m.d.sync += vic_cycle.eq(0)
    with m.Elif(self.i_clk_1mhz_ph2_en):
      m.d.sync += vic_cycle.eq(1)

    # 6510 CPU.
    m.submodules.u_cpu = u_cpu = Cpu6510()
    m.d.comb += u_cpu.i_ss_load.eq(self.i_ss_cpu_load)

    # Vic-II.
    m.submodules.u_vic = u_vic = VicII()

    # CIA-1.
    m.submodules.u_cia1 = u_cia1 = Cia()

    # CIA-2.
    m.submodules.u_cia2 = u_cia2 = Cia()

    # Cartridge.
    m.submodules.u_cart = u_cart = Cartridge()

    # Color RAM lives in core_top (so savestates can reach it); like main RAM
    # its read data is registered one clock after the address.
    color_rdata = self.i_color_data

    cpu_di = Signal(8)
    ram_cs = Signal()
    sid_cs = Signal()
    sid2_cs = Signal()
    cia1_cs = Signal()
    cia2_cs = Signal()
    color_cs = Signal()

    cpu_addr = Signal(16)
    cpu_we = Signal()
    cpu_di = Signal(8)
    cpu_do = Signal(8)
    cpu_po = Signal(6)

    bus_addr = Signal(16)
    bus_we = Signal()
    bus_di = Signal(8)
    bus_do = Signal(8)

    m.d.comb += [self.o_bus_addr.eq(bus_addr)]
    m.d.comb += [
        self.o_color_data.eq(bus_do[0:4]),
        self.o_color_we.eq(color_cs & bus_we),
        self.o_ram_main_data.eq(bus_do),
        self.o_ram_main_we.eq(ram_cs & bus_we)
    ]

    m.d.comb += [
        u_cart.i_cart_type.eq(self.i_cart_type),
        u_cart.i_freeze.eq(self.i_freeze),
        self.o_cart_active.eq(u_cart.o_active),
        u_cart.i_addr.eq(bus_addr),
        u_cart.i_data.eq(bus_do),
        u_cart.i_we.eq(bus_we),
        self.o_cart_addr.eq(u_cart.o_mem_addr),
        u_cart.i_mem_data.eq(self.i_cart_data),
        self.o_cart_data.eq(u_cart.o_mem_data),
        self.o_cart_we.eq(u_cart.o_mem_we),
        self.o_cart_ram.eq(u_cart.o_mem_ram),
    ]

    m.d.comb += [
      self.o_debug_6510_valid.eq(u_cpu.o_debug_valid),
      self.o_debug_6510_sync.eq(u_cpu.o_debug_sync),
      self.o_debug_6510_addr.eq(u_cpu.o_debug_addr),
      self.o_debug_6510_data.eq(u_cpu.o_debug_data),
      self.o_debug_6510_regs.eq(u_cpu.o_debug_regs),
    ]

    # Bank switching - following the table from
    # https://www.c64-wiki.com/wiki/Bank_Switching
    mode_bits = Signal(5)
    m.d.comb += mode_bits.eq(Cat(cpu_po[0:3], u_cart.o_game, u_cart.o_exrom))
    m.d.comb += [
        cpu_di.eq(0),
        ram_cs.eq(0),
        u_vic.i_reg_cs.eq(0),
        sid_cs.eq(0),
        sid2_cs.eq(0),
        cia1_cs.eq(0),
        cia2_cs.eq(0),
        color_cs.eq(0)
    ]

    # RAM (which the system requires and must appear in each mode)
    with m.If(cpu_addr <= 0x0FFF):
      m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
    #RAM or is unmapped
    with m.Elif((0x1000 <= cpu_addr) & (cpu_addr <= 0x7FFF)):
      with m.If(mode_bits[3:5] != 0b10):
        m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
    # RAM or cartridge ROM
    with m.Elif((0x8000 <= cpu_addr) & (cpu_addr <= 0x9FFF)):
      with m.Switch(mode_bits):
        with m.Case('10---'):
          m.d.comb += [u_cart.i_roml.eq(1), cpu_di.eq(u_cart.o_data)]
        with m.Case('01111', '01011', '00111', '00011'):
          m.d.comb += [u_cart.i_roml.eq(1), cpu_di.eq(u_cart.o_data), ram_cs.eq(1)]
        with m.Default():
          m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
    # RAM, BASIC interpretor ROM, cartridge ROM or is unmapped
    with m.Elif((0xA000 <= cpu_addr) & (cpu_addr <= 0xBFFF)):
      with m.Switch(mode_bits):
        with m.Case('11111', '11011', '01111', '01011'):
          m.d.comb += [cpu_di.eq(self.i_rom_basic_data), ram_cs.eq(1)]
        with m.Case('10---'):
          pass
        with m.Case('00111', '00110', '00011', '00010'):
          m.d.comb += [u_cart.i_romh.eq(1), cpu_di.eq(u_cart.o_data), ram_cs.eq(1)]
        with m.Default():
          m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
    # RAM or is unmapped
    with m.Elif((0xC000 <= cpu_addr) & (cpu_addr <= 0xCFFF)):
      with m.If(mode_bits[3:5] != 0b10):
        m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
    # RAM, Character generator ROM, or I/O registers and Color RAM
    with m.Elif((0xD000 <= cpu_addr) & (cpu_addr <= 0xDFFF)):
      with m.Switch(mode_bits):
        with m.Case('11111', '-1110', '-1101', '10---', '01111', '00111', '00110', '00101'):
          # IO
          with m.If((0xD000 <= cpu_addr) & (cpu_addr <= 0xD3FF)):  # VIC-II
            m.d.comb += [cpu_di.eq(u_vic.o_reg_data), u_vic.i_reg_cs.eq(~vic_cycle & ~u_vic.o_steal_bus)]
          with m.Elif((0xD400 <= cpu_addr) & (cpu_addr <= 0xD7FF)):  # SID 1 & SID 2
            m.d.comb += [
                cpu_di.eq(self.i_sid_data),
                sid_cs.eq(cpu_addr[5] == 0),
                sid2_cs.eq(cpu_addr[5] == 1)
            ]
          with m.Elif((0xD800 <= cpu_addr) & (cpu_addr <= 0xDBFF)):  # COLOR-RAM
            m.d.comb += [cpu_di.eq(color_rdata), color_cs.eq(1)]
          with m.Elif((0xDC00 <= cpu_addr) & (cpu_addr <= 0xDCFF)):  # CIA1
            m.d.comb += [cpu_di.eq(u_cia1.o_data), cia1_cs.eq(1)]
          with m.Elif((0xDD00 <= cpu_addr) & (cpu_addr <= 0xDDFF)):  # CIA2
            m.d.comb += [cpu_di.eq(u_cia2.o_data), cia2_cs.eq(1)]

          # Handle cartridge IO1/IO2
          with m.If(cpu_addr[8:16] == 0xDE):
            m.d.comb += u_cart.i_io1.eq(1)
            with m.If(u_cart.o_io1_drive):
              m.d.comb += cpu_di.eq(u_cart.o_data)
          with m.If(cpu_addr[8:16] == 0xDF):
            m.d.comb += [u_cart.i_io2.eq(1), cpu_di.eq(u_cart.o_data)]

        with m.Case('11-00', '0--00', '00001'):
          m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]
        with m.Default():
          m.d.comb += [cpu_di.eq(self.i_rom_char_data), ram_cs.eq(1)]

    # RAM, KERNAL ROM or cartridge ROM
    with m.Elif(0xE000 <= cpu_addr):
      with m.Switch(mode_bits):
        with m.Case('11111', '-1110', '11011', '-1010', '01111', '01011', '00111', '00110', '00011', '00010'):
          m.d.comb += [cpu_di.eq(self.i_rom_kernal_data), ram_cs.eq(1)]
        with m.Case('10---'):
          m.d.comb += [u_cart.i_romh.eq(1), cpu_di.eq(u_cart.o_data)]
        with m.Default():
          m.d.comb += [cpu_di.eq(self.i_ram_main_data), ram_cs.eq(1)]

    #
    #
    #

    m.d.comb += [
        u_cpu.i_data.eq(cpu_di),
        u_cpu.i_irq.eq(u_cia1.o_irq | u_vic.o_irq),
        u_cpu.i_nmi.eq(u_cia2.o_irq | u_cart.o_nmi),
        cpu_addr.eq(u_cpu.o_addr),
        cpu_we.eq(u_cpu.o_we),
        cpu_do.eq(u_cpu.o_data),
        cpu_po.eq(u_cpu.o_port),
        u_cpu.clk_1mhz_ph1_en.eq(self.i_clk_1mhz_ph1_en),
        u_cpu.clk_1mhz_ph2_en.eq(self.i_clk_1mhz_ph2_en),
        u_cpu.i_stun.eq(u_vic.o_steal_bus),
        # VIC-II
        u_vic.clk_8mhz_en.eq(1),
        u_vic.clk_1mhz_ph1_en.eq(self.i_clk_1mhz_ph1_en),
        u_vic.clk_1mhz_ph2_en.eq(self.i_clk_1mhz_ph2_en),
        u_vic.i_reg_addr.eq(bus_addr),
        u_vic.i_reg_data.eq(bus_do),
        u_vic.i_reg_we.eq(bus_we),
        # SID
        self.o_sid_cs.eq(sid_cs),
        self.o_sid2_cs.eq(sid2_cs),
        self.o_sid_addr.eq(bus_addr[0:5]),
        self.o_sid_data.eq(bus_do),
        self.o_sid_we.eq(bus_we),
        # CIA-1
        u_cia1.clk_1mhz_ph_en.eq(self.i_clk_1mhz_ph2_en),
        u_cia1.i_cs.eq(cia1_cs),
        u_cia1.i_addr.eq(bus_addr),
        u_cia1.i_we.eq(bus_we),
        u_cia1.i_data.eq(bus_do),
        u_cia1.i_pa.eq(0xff),
        u_cia1.i_pb.eq(0xff),
        # CIA-2
        u_cia2.clk_1mhz_ph_en.eq(self.i_clk_1mhz_ph2_en),
        u_cia2.i_cs.eq(cia2_cs),
        u_cia2.i_addr.eq(bus_addr),
        u_cia2.i_we.eq(bus_we),
        u_cia2.i_data.eq(bus_do),
        u_cia2.i_pa.eq(0xff),
        u_cia2.i_pb.eq(0xff),
        u_cia2.i_pa[6].eq(self.i_iec_clock_in),
        u_cia2.i_pa[7].eq(self.i_iec_data_in),

        # Bus matrix
        bus_addr.eq(Mux(vic_cycle | u_vic.o_steal_bus, Cat(u_vic.o_addr, ~u_cia2.o_pa[0:2]), cpu_addr)),
        bus_we.eq(cpu_we & ~vic_cycle & ~u_vic.o_steal_bus),
        bus_do.eq(cpu_do),
        # IEC
        self.o_iec_atn_out.eq(~u_cia2.o_pa[3]),
        self.o_iec_clock_out.eq(~u_cia2.o_pa[4]),
        self.o_iec_data_out.eq(~u_cia2.o_pa[5]),
    ]

    with m.If((bus_addr[12:16] == 0b0001) | (bus_addr[12:16] == 0b1001)):
      m.d.comb += u_vic.i_data.eq(Cat(self.i_rom_char_data, color_rdata))
    with m.Else():
      m.d.comb += u_vic.i_data.eq(Cat(self.i_ram_main_data, color_rdata))

    # Keyboard matrix and joysticks.
    m.d.comb += u_cia1.i_pa.eq(Cat(~self.i_joystick2[0:5], C(0b111, 3)))
    m.d.comb += u_cia1.i_pb.eq(~(
        Cat(self.i_joystick1[0:5], C(0b000, 3)) |  #
        Mux(~u_cia1.o_pa[7], self.i_keyboard_mask[56:64], 0) |  #
        Mux(~u_cia1.o_pa[6], self.i_keyboard_mask[48:56], 0) |  #
        Mux(~u_cia1.o_pa[5], self.i_keyboard_mask[40:48], 0) |  #
        Mux(~u_cia1.o_pa[4], self.i_keyboard_mask[32:40], 0) |  #
        Mux(~u_cia1.o_pa[3], self.i_keyboard_mask[24:32], 0) |  #
        Mux(~u_cia1.o_pa[2], self.i_keyboard_mask[16:24], 0) |  #
        Mux(~u_cia1.o_pa[1], self.i_keyboard_mask[8:16], 0) |  #
        Mux(~u_cia1.o_pa[0], self.i_keyboard_mask[0:8], 0)))

    m.d.comb += [self.o_vid_hsync.eq(u_vic.o_hsync), self.o_vid_vsync.eq(u_vic.o_vsync), self.o_vid_en.eq(u_vic.o_visib)]

    with m.Switch(u_vic.o_color):
      with m.Case(0x0):
        m.d.comb += self.o_vid_rgb.eq(0x000000)
      with m.Case(0x1):
        m.d.comb += self.o_vid_rgb.eq(0xffffff)
      with m.Case(0x2):
        m.d.comb += self.o_vid_rgb.eq(0x880000)
      with m.Case(0x3):
        m.d.comb += self.o_vid_rgb.eq(0xaaffee)
      with m.Case(0x4):
        m.d.comb += self.o_vid_rgb.eq(0xcc44cc)
      with m.Case(0x5):
        m.d.comb += self.o_vid_rgb.eq(0x00cc55)
      with m.Case(0x6):
        m.d.comb += self.o_vid_rgb.eq(0x0000aa)
      with m.Case(0x7):
        m.d.comb += self.o_vid_rgb.eq(0xeeee77)
      with m.Case(0x8):
        m.d.comb += self.o_vid_rgb.eq(0xdd8855)
      with m.Case(0x9):
        m.d.comb += self.o_vid_rgb.eq(0x664400)
      with m.Case(0xa):
        m.d.comb += self.o_vid_rgb.eq(0xff7777)
      with m.Case(0xb):
        m.d.comb += self.o_vid_rgb.eq(0x333333)
      with m.Case(0xc):
        m.d.comb += self.o_vid_rgb.eq(0x777777)
      with m.Case(0xd):
        m.d.comb += self.o_vid_rgb.eq(0xaaff66)
      with m.Case(0xe):
        m.d.comb += self.o_vid_rgb.eq(0x0088ff)
      with m.Case(0xf):
        m.d.comb += self.o_vid_rgb.eq(0xbbbbbb)

    return m

#
# Generate verilog
#

from amaranth.back import verilog
from amaranth.hdl import EnableInserter
from amaranth.hdl._ir import Fragment
from savestate import add_scan_chain
import sys
import os

if __name__ == "__main__":

  myc64 = MyC64()

  with open("myc64.v", "w") as f:
    frag = Fragment.get(EnableInserter({'sync': myc64.i_ce})(myc64), None)
    words, digest = add_scan_chain(frag, myc64.i_ss_shift, myc64.i_ss_si, myc64.o_ss_so, myc64.i_ss_capture,
                                   myc64.o_ss_words, myc64.o_ss_hash, 'myc64_ss_map.json')
    print('savestate chain: %d words, layout hash %08x' % (words, digest))
    f.write(verilog.convert(elaboratable=frag, name='myc64_top', ports=myc64.ports))
