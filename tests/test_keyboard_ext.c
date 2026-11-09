#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define C64_KEYB_MASK_KEY(x) (1ULL << ((((x) >> 4) & 0xf) * 8 + ((x)&0xf)))

static uint32_t mock_cont3_key = 0;
static uint32_t mock_cont3_joy = 0;
static uint32_t mock_cont3_trig = 0;

static uint32_t mmio_read_count = 0;

static volatile uint32_t *mock_reg_ptr(uint32_t *val) {
  return (volatile uint32_t *)val;
}

#define MOCK_CONT3_KEY (*({ mmio_read_count++; mock_reg_ptr(&mock_cont3_key); }))
#define MOCK_CONT3_JOY (*({ mmio_read_count++; mock_reg_ptr(&mock_cont3_joy); }))
#define MOCK_CONT3_TRIG (*({ mmio_read_count++; mock_reg_ptr(&mock_cont3_trig); }))

// Copy of hid2c64 mapping from src/bios/keyboard-ext.c
static const uint8_t hid2c64[] = {
    0xff, // 0x00 // No key pressed
    0xff, // 0x01 // Keyboard Error Roll Over
    0xff, // 0x02 // Keyboard POST Fail
    0xff, // 0x03 // Keyboard Error Undefined
    0x12, // 0x04 // Keyboard a and A
    0x34, // 0x05 // Keyboard b and B
    0x24, // 0x06 // Keyboard c and C
    0x22, // 0x07 // Keyboard d and D
    0x16, // 0x08 // Keyboard e and E
    0x25, // 0x09 // Keyboard f and F
    0x32, // 0x0a // Keyboard g and G
    0x35, // 0x0b // Keyboard h and H
    0x41, // 0x0c // Keyboard i and I
    0x42, // 0x0d // Keyboard j and J
    0x45, // 0x0e // Keyboard k and K
    0x52, // 0x0f // Keyboard l and L
    0x44, // 0x10 // Keyboard m and M
    0x47, // 0x11 // Keyboard n and N
    0x46, // 0x12 // Keyboard o and O
    0x51, // 0x13 // Keyboard p and P
    0x76, // 0x14 // Keyboard q and Q
    0x21, // 0x15 // Keyboard r and R
    0x15, // 0x16 // Keyboard s and S
    0x26, // 0x17 // Keyboard t and T
    0x36, // 0x18 // Keyboard u and U
    0x37, // 0x19 // Keyboard v and V
    0x11, // 0x1a // Keyboard w and W
    0x27, // 0x1b // Keyboard x and X
    0x31, // 0x1c // Keyboard y and Y
    0x14, // 0x1d // Keyboard z and Z

    0x70, // 0x1e // Keyboard 1 and !
    0x73, // 0x1f // Keyboard 2 and @
    0x10, // 0x20 // Keyboard 3 and #
    0x13, // 0x21 // Keyboard 4 and $
    0x20, // 0x22 // Keyboard 5 and %
    0x23, // 0x23 // Keyboard 6 and ^
    0x30, // 0x24 // Keyboard 7 and &
    0x33, // 0x25 // Keyboard 8 and *
    0x40, // 0x26 // Keyboard 9 and (
    0x43, // 0x27 // Keyboard 0 and )

    0x01, // 0x28 // Keyboard Return (ENTER)
    0x77, // 0x29 // Keyboard ESCAPE
    0x00, // 0x2a // Keyboard DELETE (Backspace)
    0x72, // 0x2b // Keyboard Tab
    0x74, // 0x2c // Keyboard Spacebar
    0x50, // 0x2d // Keyboard - and _
    0x53, // 0x2e // Keyboard = and +
    0x56, // 0x2f // Keyboard [ and {
    0x61, // 0x30 // Keyboard ] and }
    0x65, // 0x31 // Keyboard \ and |
    0x65, // 0x32 // Keyboard Non-US # and ~
    0x62, // 0x33 // Keyboard ; and :
    0x55, // 0x34 // Keyboard ' and "
    0x71, // 0x35 // Keyboard ` and ~
    0x57, // 0x36 // Keyboard , and <
    0x54, // 0x37 // Keyboard . and >
    0x67, // 0x38 // Keyboard / and ?
    0x77, // 0x39 // Keyboard Caps Lock

    0x04, // 0x3a // Keyboard F1
    0xff, // 0x3b // Keyboard F2
    0x05, // 0x3c // Keyboard F3
    0xff, // 0x3d // Keyboard F4
    0x06, // 0x3e // Keyboard F5
    0xff, // 0x3f // Keyboard F6
    0x03, // 0x40 // Keyboard F7
    0xff, // 0x41 // Keyboard F8
    0xff, // 0x42 // Keyboard F9
    0xff, // 0x43 // Keyboard F10
    0xff, // 0x44 // Keyboard F11
    0xff, // 0x45 // Keyboard F12

    0xff, // 0x46 // Keyboard Print Screen
    0xff, // 0x47 // Keyboard Scroll Lock
    0xff, // 0x48 // Keyboard Pause
    0x60, // 0x49 // Keyboard Insert
    0x63, // 0x4a // Keyboard Home
    0xff, // 0x4b // Keyboard Page Up
    0x66, // 0x4c // Keyboard Delete Forward
    0xff, // 0x4d // Keyboard End
    0xff, // 0x4e // Keyboard Page Down
    0x02, // 0x4f // Keyboard Right Arrow
    0xff, // 0x50 // Keyboard Left Arrow
    0x07, // 0x51 // Keyboard Down Arrow
    0xff, // 0x52 // Keyboard Up Arrow
};

static uint64_t keyboard_ext_handle_orig() {
  uint64_t mask = 0;
  uint8_t hid_mod = MOCK_CONT3_KEY >> 8;
  uint8_t hid_keys[6];
  hid_keys[0] = MOCK_CONT3_JOY >> 24;
  hid_keys[1] = MOCK_CONT3_JOY >> 16;
  hid_keys[2] = MOCK_CONT3_JOY >> 8;
  hid_keys[3] = MOCK_CONT3_JOY >> 0;
  hid_keys[4] = MOCK_CONT3_TRIG >> 8;
  hid_keys[5] = MOCK_CONT3_TRIG >> 0;
  for (unsigned i = 0; i < 6; i++) {
    if (hid_keys[i] < sizeof(hid2c64)) {
      uint8_t ports = hid2c64[hid_keys[i]];
      if (ports == 0xff)
        continue;
      mask |= C64_KEYB_MASK_KEY(ports);
    }
  }

  if (hid_mod & (1 << 0)) {
    mask |= C64_KEYB_MASK_KEY(0x75);
  }
  if (hid_mod & (1 << 1)) {
    mask |= C64_KEYB_MASK_KEY(0x17);
  }
  if (hid_mod & (1 << 5)) {
    mask |= C64_KEYB_MASK_KEY(0x64);
  }
  return mask;
}

static uint64_t keyboard_ext_handle_opt() {
  uint64_t mask = 0;
  uint32_t joy = MOCK_CONT3_JOY;
  uint32_t trig = MOCK_CONT3_TRIG;
  uint32_t key = MOCK_CONT3_KEY;
  uint8_t hid_mod = (uint8_t)(key >> 8);

  if (((joy | (trig & 0xffff)) == 0) && !(hid_mod & 0x23)) {
    return mask;
  }

  uint8_t hid_keys[6] = {
      (uint8_t)(joy >> 24),
      (uint8_t)(joy >> 16),
      (uint8_t)(joy >> 8),
      (uint8_t)joy,
      (uint8_t)(trig >> 8),
      (uint8_t)trig,
  };

  for (unsigned i = 0; i < 6; i++) {
    uint8_t k = hid_keys[i];
    if (k && k < sizeof(hid2c64)) {
      uint8_t ports = hid2c64[k];
      if (ports != 0xff) {
        mask |= C64_KEYB_MASK_KEY(ports);
      }
    }
  }

  if (hid_mod & (1 << 0)) {
    mask |= C64_KEYB_MASK_KEY(0x75);
  }
  if (hid_mod & (1 << 1)) {
    mask |= C64_KEYB_MASK_KEY(0x17);
  }
  if (hid_mod & (1 << 5)) {
    mask |= C64_KEYB_MASK_KEY(0x64);
  }
  return mask;
}

int main(void) {
  printf("Running keyboard_ext_handle optimization unit tests...\n");

  // Test 1: Idle state - no keys and no modifiers pressed
  mock_cont3_key = 0x40000000; // Device ID in high bits, mod = 0
  mock_cont3_joy = 0x00000000;
  mock_cont3_trig = 0x00000000;

  mmio_read_count = 0;
  uint64_t mask_orig = keyboard_ext_handle_orig();
  uint32_t reads_orig = mmio_read_count;

  mmio_read_count = 0;
  uint64_t mask_opt = keyboard_ext_handle_opt();
  uint32_t reads_opt = mmio_read_count;

  assert(mask_orig == 0);
  assert(mask_opt == 0);
  assert(reads_orig == 7); // 1 key + 4 joy + 2 trig
  assert(reads_opt == 3);  // 1 joy + 1 trig + 1 key
  printf("Test 1 (Idle state): reads orig = %u, opt = %u (%.1f%% MMIO reduction) - PASSED\n",
         reads_orig, reads_opt, ((double)(reads_orig - reads_opt) / reads_orig) * 100.0);

  // Test 2: Single key 'A' (HID 0x04 -> C64 port 0x12) in key 0
  mock_cont3_key = 0x40000000;
  mock_cont3_joy = 0x04000000; // key0 = 0x04 ('A')
  mock_cont3_trig = 0x00000000;

  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == C64_KEYB_MASK_KEY(0x12));
  assert(mask_opt == C64_KEYB_MASK_KEY(0x12));
  printf("Test 2 (Single key 'A'): masks match exactly (0x%llx) - PASSED\n", (unsigned long long)mask_opt);

  // Test 3: Multiple keys in JOY and TRIG (e.g. 'A' 0x04, 'B' 0x05, '1' 0x1E, Return 0x28, Space 0x2C)
  mock_cont3_key = 0x40000000;
  mock_cont3_joy = (0x04 << 24) | (0x05 << 16) | (0x1E << 8) | (0x28 << 0);
  mock_cont3_trig = (0x2C << 8) | (0x00 << 0);

  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == mask_opt);
  assert((mask_opt & C64_KEYB_MASK_KEY(0x12)) != 0); // 'A'
  assert((mask_opt & C64_KEYB_MASK_KEY(0x34)) != 0); // 'B'
  assert((mask_opt & C64_KEYB_MASK_KEY(0x70)) != 0); // '1'
  assert((mask_opt & C64_KEYB_MASK_KEY(0x01)) != 0); // Return
  assert((mask_opt & C64_KEYB_MASK_KEY(0x74)) != 0); // Space
  printf("Test 3 (5 concurrent keys): masks match exactly - PASSED\n");

  // Test 4: Modifiers only (Commodore, Left Shift, Right Shift)
  // Commodore key (bit 0 = 0x01):
  mock_cont3_key = 0x40000100;
  mock_cont3_joy = 0;
  mock_cont3_trig = 0;
  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == mask_opt);
  assert(mask_opt == C64_KEYB_MASK_KEY(0x75));

  // Left shift (bit 1 = 0x02):
  mock_cont3_key = 0x40000200;
  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == mask_opt);
  assert(mask_opt == C64_KEYB_MASK_KEY(0x17));

  // Right shift (bit 5 = 0x20):
  mock_cont3_key = 0x40002000;
  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == mask_opt);
  assert(mask_opt == C64_KEYB_MASK_KEY(0x64));

  // All 3 modifiers together:
  mock_cont3_key = 0x40002300;
  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == mask_opt);
  assert(mask_opt == (C64_KEYB_MASK_KEY(0x75) | C64_KEYB_MASK_KEY(0x17) | C64_KEYB_MASK_KEY(0x64)));
  printf("Test 4 (Modifier keys C=, LShift, RShift): masks match exactly - PASSED\n");

  // Test 5: Unmapped modifiers (e.g. Ctrl bit 2, Alt bit 3, GUI bit 4 -> 0x1C)
  mock_cont3_key = 0x40001C00;
  mock_cont3_joy = 0;
  mock_cont3_trig = 0;
  mask_orig = keyboard_ext_handle_orig();
  mask_opt = keyboard_ext_handle_opt();
  assert(mask_orig == 0);
  assert(mask_opt == 0);
  printf("Test 5 (Unmapped modifiers): fast path triggers, mask = 0 - PASSED\n");

  // Test 6: Exhaustive sweep over all 83 valid HID keycodes
  for (uint8_t k = 0; k < sizeof(hid2c64); k++) {
    mock_cont3_key = 0x40000000;
    mock_cont3_joy = ((uint32_t)k) << 24;
    mock_cont3_trig = 0;

    mask_orig = keyboard_ext_handle_orig();
    mask_opt = keyboard_ext_handle_opt();
    assert(mask_orig == mask_opt);
  }
  printf("Test 6 (Exhaustive sweep of all 83 HID keys): 100%% equivalence verified - PASSED\n");

  // Test 7: Out-of-bounds keycodes (> 83)
  for (uint16_t k = sizeof(hid2c64); k < 256; k++) {
    mock_cont3_key = 0x40000000;
    mock_cont3_joy = ((uint32_t)k) << 24;
    mock_cont3_trig = 0;

    mask_orig = keyboard_ext_handle_orig();
    mask_opt = keyboard_ext_handle_opt();
    assert(mask_orig == 0);
    assert(mask_opt == 0);
  }
  printf("Test 7 (Out-of-bounds keycodes): 100%% equivalence verified - PASSED\n");

  printf("All keyboard_ext_handle tests passed successfully!\n");
  return 0;
}
