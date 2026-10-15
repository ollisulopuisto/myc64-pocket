/*
 * Copyright (C) 2024 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "bios.h"

static volatile int sel_row = 0;

uint8_t joystick1;
uint8_t joystick2;

// The BASIC prompt is up when "READY." (screen codes) starts a row of the
// default screen at $0400. The reset clears C64 RAM first, so a match cannot
// be left over from before the reset.
int c64_basic_ready(void) {
  static const uint8_t ready[] = {0x12, 0x05, 0x01, 0x04, 0x19, 0x2e};
  for (int row = 0; row < 25; row++) {
    unsigned i = 0;
    while (i < sizeof(ready) && C64_RAM_RD[0x0400 + row * 40 + i] == ready[i])
      i++;
    if (i == sizeof(ready))
      return 1;
  }
  return 0;
}

void misc_reset_core(uint8_t cart_type) {
  // Like a power cycle: hold the C64 and 1541 in reset and clear all their
  // RAM (main, colour, drive, cartridge), so nothing of the previous program
  // survives. The clear takes 0.5 ms. Also called from the IRQ handler.
  *C64_CTRL = bits_set(*C64_CTRL, 0, 1, 0);         // Assert reset for MyC64
  *MEM_WIPE = 1;
  while (*MEM_WIPE & 1)
    ;
  *C64_CTRL = bits_set(*C64_CTRL, 5, 3, cart_type); // Set cartridge type
  *C64_CTRL = bits_set(*C64_CTRL, 0, 1, 1);         // Release reset for MyC64
}

void misc_handle() {
  if (KEYB_POSEDGE(dpad_up)) {
    if (sel_row > 0) {
      sel_row--;
      osd_dirty = 1;
    }
  } else if (KEYB_POSEDGE(dpad_down)) {
    if (sel_row < 2) {
      sel_row++;
      osd_dirty = 1;
    }
  }

  if (KEYB_POSEDGE(face_a)) {
    switch (sel_row) {
    case 0:
      misc_reset_core(0); // Reset with no cartridge
      break;
    case 1:
      joystick1 = joystick1 < 2 ? joystick1 + 1 : 0;
      osd_dirty = 1;
      break;
    case 2:
      joystick2 = joystick2 < 2 ? joystick2 + 1 : 0;
      osd_dirty = 1;
      break;
    }
  }
}

void misc_draw() {
  IRQ_DISABLE();
  int sel_row_tmp = sel_row;
  IRQ_ENABLE();

  const char *inputs[] = {"N/C  ", "CONT1", "CONT2"};
  int offset;
  int x = 2, y = 12;
  osd_put_str(x, y, "RESET", sel_row_tmp == 0);
  y += 10;
  osd_put_str(x, y, "INPUTS", 0);
  x += 8;
  y += 10;
  offset = osd_put_str(x, y, "JOYSTICK1: ", 0);
  osd_put_str(offset, y, inputs[joystick1], sel_row_tmp == 1);
  y += 10;
  offset = osd_put_str(x, y, "JOYSTICK2: ", 0);
  osd_put_str(offset, y, inputs[joystick2], sel_row_tmp == 2);
}
