/*
 * Copyright (C) 2024 Markus Lavin (https://www.zzzconsulting.se/)
 * Copyright (C) 2026 Antigravity & Olli Sulopuisto
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "bios.h"

// 8-bit byte to 10-bit GCR lookup table (512 bytes in .rodata).
// Precombines high and low nibble GCR transformations ((gcr[hi] << 5) | gcr[lo]).
// Eliminates 4 nibble shifts and 4 bitwise ANDs per byte, reducing memory lookups
// in encode_4bytes_to_track by 50% (from 8 down to 4 per 4 bytes encoded).
static const uint16_t byte_to_gcr[256] = {
    0x14A, 0x14B, 0x152, 0x153, 0x14E, 0x14F, 0x156, 0x157,
    0x149, 0x159, 0x15A, 0x15B, 0x14D, 0x15D, 0x15E, 0x155,
    0x16A, 0x16B, 0x172, 0x173, 0x16E, 0x16F, 0x176, 0x177,
    0x169, 0x179, 0x17A, 0x17B, 0x16D, 0x17D, 0x17E, 0x175,
    0x24A, 0x24B, 0x252, 0x253, 0x24E, 0x24F, 0x256, 0x257,
    0x249, 0x259, 0x25A, 0x25B, 0x24D, 0x25D, 0x25E, 0x255,
    0x26A, 0x26B, 0x272, 0x273, 0x26E, 0x26F, 0x276, 0x277,
    0x269, 0x279, 0x27A, 0x27B, 0x26D, 0x27D, 0x27E, 0x275,
    0x1CA, 0x1CB, 0x1D2, 0x1D3, 0x1CE, 0x1CF, 0x1D6, 0x1D7,
    0x1C9, 0x1D9, 0x1DA, 0x1DB, 0x1CD, 0x1DD, 0x1DE, 0x1D5,
    0x1EA, 0x1EB, 0x1F2, 0x1F3, 0x1EE, 0x1EF, 0x1F6, 0x1F7,
    0x1E9, 0x1F9, 0x1FA, 0x1FB, 0x1ED, 0x1FD, 0x1FE, 0x1F5,
    0x2CA, 0x2CB, 0x2D2, 0x2D3, 0x2CE, 0x2CF, 0x2D6, 0x2D7,
    0x2C9, 0x2D9, 0x2DA, 0x2DB, 0x2CD, 0x2DD, 0x2DE, 0x2D5,
    0x2EA, 0x2EB, 0x2F2, 0x2F3, 0x2EE, 0x2EF, 0x2F6, 0x2F7,
    0x2E9, 0x2F9, 0x2FA, 0x2FB, 0x2ED, 0x2FD, 0x2FE, 0x2F5,
    0x12A, 0x12B, 0x132, 0x133, 0x12E, 0x12F, 0x136, 0x137,
    0x129, 0x139, 0x13A, 0x13B, 0x12D, 0x13D, 0x13E, 0x135,
    0x32A, 0x32B, 0x332, 0x333, 0x32E, 0x32F, 0x336, 0x337,
    0x329, 0x339, 0x33A, 0x33B, 0x32D, 0x33D, 0x33E, 0x335,
    0x34A, 0x34B, 0x352, 0x353, 0x34E, 0x34F, 0x356, 0x357,
    0x349, 0x359, 0x35A, 0x35B, 0x34D, 0x35D, 0x35E, 0x355,
    0x36A, 0x36B, 0x372, 0x373, 0x36E, 0x36F, 0x376, 0x377,
    0x369, 0x379, 0x37A, 0x37B, 0x36D, 0x37D, 0x37E, 0x375,
    0x1AA, 0x1AB, 0x1B2, 0x1B3, 0x1AE, 0x1AF, 0x1B6, 0x1B7,
    0x1A9, 0x1B9, 0x1BA, 0x1BB, 0x1AD, 0x1BD, 0x1BE, 0x1B5,
    0x3AA, 0x3AB, 0x3B2, 0x3B3, 0x3AE, 0x3AF, 0x3B6, 0x3B7,
    0x3A9, 0x3B9, 0x3BA, 0x3BB, 0x3AD, 0x3BD, 0x3BE, 0x3B5,
    0x3CA, 0x3CB, 0x3D2, 0x3D3, 0x3CE, 0x3CF, 0x3D6, 0x3D7,
    0x3C9, 0x3D9, 0x3DA, 0x3DB, 0x3CD, 0x3DD, 0x3DE, 0x3D5,
    0x2AA, 0x2AB, 0x2B2, 0x2B3, 0x2AE, 0x2AF, 0x2B6, 0x2B7,
    0x2A9, 0x2B9, 0x2BA, 0x2BB, 0x2AD, 0x2BD, 0x2BE, 0x2B5
};

// Sectors per physical track (1-based index, index 0 unused)
static const uint8_t sectors_per_track[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // 1-17
    19, 19, 19, 19, 19, 19, 19,                                         // 18-24
    18, 18, 18, 18, 18, 18,                                             // 25-30
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17                              // 31-40
};

// Precomputed byte offsets into D64 file for each track 1..41 (O(1) lookup).
static const uint32_t track_d64_offset_lut[42] = {
    0,      0,      5376,   10752,  16128,  21504,  26880,  32256,  37632,
    43008,  48384,  53760,  59136,  64512,  69888,  75264,  80640,  86016,
    91392,  96256,  101120, 105984, 110848, 115712, 120576, 125440, 130048,
    134656, 139264, 143872, 148480, 153088, 157440, 161792, 166144, 170496,
    174848, 179200, 183552, 187904, 192256, 196608
};

// Target track byte lengths per speed zone
static const uint16_t zone_track_len[4] = {
    6250, // Zone 0 (tracks 31-40)
    6666, // Zone 1 (tracks 25-30)
    7142, // Zone 2 (tracks 18-24)
    7692  // Zone 3 (tracks 1-17)
};

static inline uint8_t get_speed_zone(uint8_t track) {
  if (track <= 17) return 3;
  if (track <= 24) return 2;
  if (track <= 30) return 1;
  return 0;
}

volatile uint8_t d64_loaded = 0;
static volatile uint8_t d64_track_no = 0xff;
static volatile uint8_t d64_led_on;
static volatile uint8_t d64_motor_on;
static volatile uint32_t d64_status_bar_timeout;

static uint8_t disk_id1 = 0x32; // Default '2'
static uint8_t disk_id2 = 0x41; // Default 'A'
static uint8_t d64_max_tracks = 35;

static uint8_t sector_buf[256];

// Encodes 4 raw binary bytes into 5 GCR-encoded bytes directly into C1541 track RAM.
// Utilizes the precomputed byte_to_gcr 8b->10b table for fast single-instruction lookups.
static inline void encode_4bytes_to_track(const uint8_t *in, uint16_t out_pos) {
  uint32_t g01 = byte_to_gcr[in[0]];
  uint32_t g23 = byte_to_gcr[in[1]];
  uint32_t g45 = byte_to_gcr[in[2]];
  uint32_t g67 = byte_to_gcr[in[3]];

  C1541_TRACK_RAM[out_pos + 0] = (uint8_t)(g01 >> 2);
  C1541_TRACK_RAM[out_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
  C1541_TRACK_RAM[out_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
  C1541_TRACK_RAM[out_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
  C1541_TRACK_RAM[out_pos + 4] = (uint8_t)g67;
}

static inline uint32_t get_track_d64_offset(uint8_t track) {
  if (track <= 41) return track_d64_offset_lut[track];
  return track_d64_offset_lut[41] + (uint32_t)(track - 41) * (17 * 256);
}

static void synthesize_d64_track(uint8_t track_id) {
  // Convert 1541 half-track step to 1-based physical track:
  // track_id: 0, 1 -> track 1; 2, 3 -> track 2, etc. (physical track = (track_id >> 1) + 1)
  uint8_t phys_track = (track_id >> 1) + 1;
  if (phys_track > d64_max_tracks) {
    phys_track = d64_max_tracks;
  }

  uint8_t num_sectors = (phys_track <= 40) ? sectors_per_track[phys_track] : 17;
  uint8_t zone = get_speed_zone(phys_track);
  uint16_t target_len = zone_track_len[zone];

  // Each sector standard layout: 5 sync + 10 header + 9 gap + 5 sync + 325 data = 354 GCR bytes
  // Remainder is divided among inter-sector gaps
  uint16_t base_gap = (target_len - (num_sectors * 354)) / num_sectors;
  if (base_gap < 4) base_gap = 4;

  uint32_t d64_offset = get_track_d64_offset(phys_track);
  uint16_t track_pos = 0;

  for (uint8_t sec = 0; sec < num_sectors; sec++) {
    // Read 256 bytes for this sector from APF Bridge
    bridge_ds_read(D64_SLOT_ID, d64_offset, 256, sector_buf);
    d64_offset += 256;

    // 1. Header Sync (5 bytes 0xFF)
    for (int i = 0; i < 5; i++) {
      C1541_TRACK_RAM[track_pos++] = 0xFF;
    }

    // 2. Header Info (8 raw bytes -> 10 GCR bytes)
    // Format: $08, checksum, sector, track, id2, id1, $0F, $0F
    uint8_t hdr_chk = sec ^ phys_track ^ disk_id2 ^ disk_id1;
    uint8_t raw_hdr[8] = {0x08, hdr_chk, sec, phys_track, disk_id2, disk_id1, 0x0F, 0x0F};
    encode_4bytes_to_track(&raw_hdr[0], track_pos);
    track_pos += 5;
    encode_4bytes_to_track(&raw_hdr[4], track_pos);
    track_pos += 5;

    // 3. Header Gap (9 bytes 0x55)
    for (int i = 0; i < 9; i++) {
      C1541_TRACK_RAM[track_pos++] = 0x55;
    }

    // 4. Data Sync (5 bytes 0xFF)
    for (int i = 0; i < 5; i++) {
      C1541_TRACK_RAM[track_pos++] = 0xFF;
    }

    // 5. Data Block (260 raw bytes -> 325 GCR bytes)
    // Format: $07, 256 data bytes, data_chk, $00, $00
    // Chunk 0: $07, sec[0], sec[1], sec[2] -> 5 GCR bytes
    uint8_t chunk0[4] = {0x07, sector_buf[0], sector_buf[1], sector_buf[2]};
    encode_4bytes_to_track(chunk0, track_pos);
    track_pos += 5;

    uint8_t data_chk = sector_buf[0] ^ sector_buf[1] ^ sector_buf[2];

    // Chunks 1 to 63: 252 data bytes in 63 chunks of 4 -> 315 GCR bytes
    for (int c = 0; c < 63; c++) {
      int byte_idx = 3 + c * 4;
      data_chk ^= sector_buf[byte_idx] ^ sector_buf[byte_idx + 1] ^
                  sector_buf[byte_idx + 2] ^ sector_buf[byte_idx + 3];
      encode_4bytes_to_track(&sector_buf[byte_idx], track_pos);
      track_pos += 5;
    }

    // Chunk 64 (last 4 raw bytes): sec[255], data_chk, $00, $00 -> 5 GCR bytes
    data_chk ^= sector_buf[255];
    uint8_t chunk_last[4] = {sector_buf[255], data_chk, 0x00, 0x00};
    encode_4bytes_to_track(chunk_last, track_pos);
    track_pos += 5;

    // 6. Inter-sector Gap (base_gap bytes 0x55)
    for (int i = 0; i < base_gap; i++) {
      if (track_pos < target_len) {
        C1541_TRACK_RAM[track_pos++] = 0x55;
      }
    }
  }

  // Pad remaining track space with 0x55 up to target_len
  while (track_pos < target_len) {
    C1541_TRACK_RAM[track_pos++] = 0x55;
  }

  // Update hardware track length register
  *C1541_TRACK_LEN = target_len;
}

void load_d64() {
  uint32_t d64_size = bridge_ds_get_length(D64_SLOT_ID);
  if (d64_size == 0) return;

  d64_max_tracks = (d64_size >= 196608) ? 40 : 35;

  // Read Disk ID from Track 18, Sector 0 (BAM)
  // Track 18 offset = (17 * 21) * 256 = 357 * 256 = 91392 (0x16500)
  // Disk ID is at offset 162-163 ($A2-$A3) within sector 0
  uint32_t t18_offset = get_track_d64_offset(18);
  bridge_ds_read(D64_SLOT_ID, t18_offset + 162, 2, sector_buf);
  disk_id1 = sector_buf[0];
  disk_id2 = sector_buf[1];

  d64_track_no = 0xff;
  d64_loaded = 1;
  d64_autoboot_trigger();
}

// Auto-load keystroke injection: LOAD "*",8,1 <RET> RUN <RET>
typedef struct {
  uint8_t key1;
  uint8_t key2; // e.g. Shift
} c64_key_chord_t;

static const c64_key_chord_t autoboot_seq[] = {
    {0x52, 0x00}, // L
    {0x46, 0x00}, // O
    {0x12, 0x00}, // A
    {0x22, 0x00}, // D
    {0x74, 0x00}, // <SPACE>
    {0x73, 0x17}, // " (Shift + 2)
    {0x61, 0x00}, // *
    {0x73, 0x17}, // " (Shift + 2)
    {0x57, 0x00}, // ,
    {0x33, 0x00}, // 8
    {0x57, 0x00}, // ,
    {0x70, 0x00}, // 1
    {0x01, 0x00}, // <RET>
};

static int autoboot_idx = -1;
static uint32_t autoboot_wait = 0;
static uint8_t autoboot_state = 0; // 0=idle, 1=boot_wait, 2=typing, 3=type_release
static uint64_t autoboot_mask;

void d64_autoboot_trigger(void) {
  // Reset C64 to clean BASIC prompt before auto-loading
  misc_reset_core(0);
  autoboot_idx = 0;
  autoboot_wait = timer_ticks + 300; // wait ~3-4 seconds for C64 BASIC to boot
  autoboot_state = 1;
}

void d64_autoboot_poll(void) {
  switch (autoboot_state) {
  case 1: // Wait for C64 boot
    c64_isr_keyb_mask = 0;
    if (timer_ticks >= autoboot_wait) {
      autoboot_idx = 0;
      autoboot_state = 2; // Press first key
    }
    break;

  case 2: // Press key (after the pause between keystrokes)
    if (timer_ticks < autoboot_wait)
      break;
    if (autoboot_idx < (int)(sizeof(autoboot_seq) / sizeof(autoboot_seq[0]))) {
      uint64_t mask = C64_KEYB_MASK_KEY(autoboot_seq[autoboot_idx].key1);
      if (autoboot_seq[autoboot_idx].key2) {
        mask |= C64_KEYB_MASK_KEY(autoboot_seq[autoboot_idx].key2);
      }
      autoboot_mask = mask;
      c64_isr_keyb_mask = mask;
      autoboot_wait = timer_ticks + 10; // Hold key for 10 ticks
      autoboot_state = 3;
    } else {
      c64_isr_keyb_mask = 0;
      autoboot_state = 0; // Finished
    }
    break;

  case 3: // Hold, then release key
    // Re-assert every tick: prgs_irq() clears c64_isr_keyb_mask while idle,
    // which otherwise shortens each press to one tick and drops keys.
    c64_isr_keyb_mask = autoboot_mask;
    if (timer_ticks >= autoboot_wait) {
      c64_isr_keyb_mask = 0;
      autoboot_wait = timer_ticks + 10; // Pause between keystrokes
      autoboot_idx++;
      autoboot_state = 2;
    }
    break;

  default:
    break;
  }
}

void d64_irq() {
  if (updated_slots & (1 << D64_SLOT_ID)) {
    load_d64();
    return;
  }

  if (!d64_loaded) return;
  d64_autoboot_poll();

  uint8_t prev_led = d64_led_on;
  uint8_t prev_motor = d64_motor_on;
  uint8_t prev_track = d64_track_no;

  uint32_t status = *C1541_STATUS;
  uint8_t req_track_no = status & 0x7f;
  d64_led_on = (status >> 7) & 1;
  d64_motor_on = (status >> 8) & 1;

  if (req_track_no != d64_track_no) {
    synthesize_d64_track(req_track_no);
    d64_track_no = req_track_no;
  }

  if (osd_mode == OSD_OFF || osd_mode == OSD_STATUS_BAR) {
    if (d64_led_on || d64_motor_on) {
      if (osd_mode != OSD_STATUS_BAR)
        osd_dirty = 1;
      osd_mode = OSD_STATUS_BAR;
      d64_status_bar_timeout = timer_ticks + 300;
    }

    if (osd_mode == OSD_STATUS_BAR && timer_ticks > d64_status_bar_timeout) {
      osd_mode = OSD_OFF;
    }
  }

  if (osd_mode == OSD_STATUS_BAR &&
      (prev_led != d64_led_on || prev_motor != d64_motor_on || prev_track != d64_track_no)) {
    osd_dirty = 1;
  }
}

void d64_draw_status_bar() {
  osd_printf(0, 0, "[D64 MOTOR:%c LED:%c TRACK:%d] ", d64_motor_on ? '1' : '0',
             d64_led_on ? '1' : '0', (d64_track_no >> 1) + 1);
}
