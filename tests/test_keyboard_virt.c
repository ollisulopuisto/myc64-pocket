#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#undef NULL
#define NULL ((void *)0)

#include "../src/bios/bios.h"

// Mock BIOS environment for keyboard-virt
uint32_t cont1_key_p = 0;
uint32_t cont1_key = 0;
uint64_t c64_keyb_mask = 0;
uint64_t c64_2nd_fire_keyb_mask = 0;
uint64_t c64_3rd_fire_keyb_mask = 0;
volatile uint8_t osd_dirty = 0;

void irq_mask(uint32_t mask) { (void)mask; }
unsigned osd_put_char(int x, int y, char c, int invert) {
  (void)x; (void)y; (void)c; (void)invert;
  return x + 8;
}

// Baseline runtime linear scan for comparison
struct test_entry {
  const char *str;
  uint8_t ports;
};

// Include keyboard-virt.c to directly test internal state and functions
#include "../src/bios/keyboard-virt.c"

static unsigned calculate_row_length_orig(unsigned row_idx) {
  const struct entry *p = rows[row_idx];
  unsigned len = 0;
  while (p->str) {
    len++;
    p++;
  }
  return len;
}

static void press_dpad(uint32_t dpad_bit) {
  cont1_key_p = 0;
  cont1_key = 1 << dpad_bit;
  keyboard_virt_handle();
  cont1_key_p = cont1_key;
}

int main(void) {
  printf("Running keyboard_virt optimization unit tests...\n");

  // Test 1: Verify row_lengths matches the actual number of entries in each row
  for (unsigned r = 0; r < N_ROWS; r++) {
    unsigned expected_len = calculate_row_length_orig(r);
    assert(row_lengths[r] == expected_len);
    printf("Row %u length: %u (expected %u) - MATCH\n", r, row_lengths[r], expected_len);
  }

  // Test 2: Verify D-pad Down navigation across all rows and clamp behavior
  sel_row = 0;
  sel_col = 0;
  for (int r = 0; r < (int)N_ROWS; r++) {
    assert(sel_row == r);
    // Press down
    press_dpad(KEYB_BIT_dpad_down);
  }
  // After wrapping around from bottom row, should be at row 0
  assert(sel_row == 0);
  printf("D-pad Down wrap-around verified - PASSED\n");

  // Test 3: Verify D-pad Up navigation and wrap-around
  sel_row = 0;
  sel_col = 0;
  press_dpad(KEYB_BIT_dpad_up);
  assert(sel_row == N_ROWS - 1);
  assert(sel_col == 0); // spacebar row has length 1, index 0
  printf("D-pad Up wrap-around to row %u verified - PASSED\n", sel_row);

  // Test 4: Verify D-pad Left and Right column clamping and wrapping
  // Row 0 has 17 items (0..16)
  sel_row = 0;
  sel_col = 0;
  press_dpad(KEYB_BIT_dpad_left);
  assert(sel_col == row_lengths[0] - 1); // should wrap to 16

  press_dpad(KEYB_BIT_dpad_right);
  assert(sel_col == 0); // should wrap back to 0

  // Move right 16 times to reach the end
  for (int c = 0; c < row_lengths[0] - 1; c++) {
    press_dpad(KEYB_BIT_dpad_right);
  }
  assert(sel_col == row_lengths[0] - 1);

  // Move down to row 1 (16 items, 0..15). Column 16 should clamp to 15.
  press_dpad(KEYB_BIT_dpad_down);
  assert(sel_row == 1);
  assert(sel_col == row_lengths[1] - 1);

  // Move down to row 2 (15 items, 0..14). Column 15 should clamp to 14.
  press_dpad(KEYB_BIT_dpad_down);
  assert(sel_row == 2);
  assert(sel_col == row_lengths[2] - 1);

  // Move down to row 3 (16 items, 0..15).
  press_dpad(KEYB_BIT_dpad_down);
  assert(sel_row == 3);
  assert(sel_col == 14); // 14 is within bounds of row 3

  // Move down to row 4 (1 item, index 0). Column should clamp to 0.
  press_dpad(KEYB_BIT_dpad_down);
  assert(sel_row == 4);
  assert(sel_col == 0);
  printf("D-pad Right & Down column clamping across varying row widths verified - PASSED\n");

  // Test 5: Verify drawing output runs without error
  keyboard_virt_draw();

  printf("All keyboard_virt optimization tests passed successfully!\n");
  return 0;
}
