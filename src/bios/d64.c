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

// 4-bit nibble to 5-bit GCR lookup table
static const uint8_t bin_to_gcr[16] = {
    0x0A, // 0000 -> 01010
    0x0B, // 0001 -> 01011
    0x12, // 0010 -> 10010
    0x13, // 0011 -> 10011
    0x0E, // 0100 -> 01110
    0x0F, // 0101 -> 01111
    0x16, // 0110 -> 10110
    0x17, // 0111 -> 10111
    0x09, // 1000 -> 01001
    0x19, // 1001 -> 11001
    0x1A, // 1010 -> 11010
    0x1B, // 1011 -> 11011
    0x0D, // 1100 -> 01101
    0x1D, // 1101 -> 11101
    0x1E, // 1110 -> 11110
    0x15  // 1111 -> 10101
};

// Sectors per physical track (1-based index, index 0 unused)
static const uint8_t sectors_per_track[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // 1-17
    19, 19, 19, 19, 19, 19, 19,                                         // 18-24
    18, 18, 18, 18, 18, 18,                                             // 25-30
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17                              // 31-40
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

static inline void encode_4bytes_to_track(const uint8_t *in, uint16_t out_pos) {
  uint8_t n0 = (in[0] >> 4) & 0x0F;
  uint8_t n1 = in[0] & 0x0F;
  uint8_t n2 = (in[1] >> 4) & 0x0F;
  uint8_t n3 = in[1] & 0x0F;
  uint8_t n4 = (in[2] >> 4) & 0x0F;
  uint8_t n5 = in[2] & 0x0F;
  uint8_t n6 = (in[3] >> 4) & 0x0F;
  uint8_t n7 = in[3] & 0x0F;

  uint8_t g0 = bin_to_gcr[n0];
  uint8_t g1 = bin_to_gcr[n1];
  uint8_t g2 = bin_to_gcr[n2];
  uint8_t g3 = bin_to_gcr[n3];
  uint8_t g4 = bin_to_gcr[n4];
  uint8_t g5 = bin_to_gcr[n5];
  uint8_t g6 = bin_to_gcr[n6];
  uint8_t g7 = bin_to_gcr[n7];

  C1541_TRACK_RAM[out_pos + 0] = ((g0 << 3) | (g1 >> 2)) & 0xFF;
  C1541_TRACK_RAM[out_pos + 1] = ((g1 << 6) | (g2 << 1) | (g3 >> 4)) & 0xFF;
  C1541_TRACK_RAM[out_pos + 2] = ((g3 << 4) | (g4 >> 1)) & 0xFF;
  C1541_TRACK_RAM[out_pos + 3] = ((g4 << 7) | (g5 << 2) | (g6 >> 3)) & 0xFF;
  C1541_TRACK_RAM[out_pos + 4] = ((g6 << 5) | g7) & 0xFF;
}

static uint32_t get_track_d64_offset(uint8_t track) {
  uint32_t sec_count = 0;
  for (uint8_t t = 1; t < track; t++) {
    sec_count += (t <= 40) ? sectors_per_track[t] : 17;
  }
  return sec_count * 256;
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
    *C1541_TRACK_ACK = 0x80 | req_track_no;
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
  osd_printf(0, 0, "[D64 MOTOR:%c LED:%c TRACK:%d%s] ", d64_motor_on ? '1' : '0',
             d64_led_on ? '1' : '0', (d64_track_no >> 1) + 1,
             (*C1541_STATUS & C1541_STATUS_WARP) ? " WARP" : "");
}
