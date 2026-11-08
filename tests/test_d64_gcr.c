#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#undef NULL
#include "../src/bios/bios.h"

#undef C1541_TRACK_RAM
#undef C1541_TRACK_LEN
#undef C1541_STATUS

static uint8_t mock_track_ram[8192];
static uint32_t mock_track_len = 0;
#define C1541_TRACK_RAM mock_track_ram
#define C1541_TRACK_LEN (&mock_track_len)
#define C1541_STATUS ((volatile uint32_t *)&mock_track_len)

#define MOCK_D64_SIZE 196608
static uint8_t mock_d64_file[MOCK_D64_SIZE];
static uint32_t bridge_tx_count = 0;

uint32_t bridge_ds_get_length(uint16_t slot_id) {
  (void)slot_id;
  return MOCK_D64_SIZE;
}

void bridge_ds_read(uint16_t slot_id, uint32_t offset, uint32_t length, uint8_t *dst) {
  (void)slot_id;
  const uint32_t buf_size = BRIDGE_DPRAM_SIZE;
  while (length > 0) {
    bridge_tx_count++;
    uint32_t chunk_size = (length < buf_size) ? length : buf_size;
    if ((((uintptr_t)dst | chunk_size) & 3) == 0) {
      const uint32_t *sp = (const uint32_t *)&mock_d64_file[offset];
      uint32_t *dp = (uint32_t *)dst;
      uint32_t words = chunk_size >> 2;
      for (uint32_t i = 0; i < words; i++) {
        dp[i] = sp[i];
      }
      dst += chunk_size;
    } else {
      for (uint32_t i = 0; i < chunk_size; i++) {
        *dst++ = mock_d64_file[offset + i];
      }
    }
    length -= chunk_size;
    offset += chunk_size;
  }
}

// BIOS environment mocks for compiling d64.c directly
volatile uint32_t timer_ticks = 0;
uint64_t c64_isr_keyb_mask = 0;
uint8_t updated_slots = 0;
osd_mode_t osd_mode = OSD_OFF;
volatile uint8_t osd_dirty = 0;
void misc_reset_core(uint8_t cart_type) { (void)cart_type; }
void osd_printf(int x, int y, const char *fmt, ...) { (void)x; (void)y; (void)fmt; }

// Include the actual production d64.c directly to exercise its synthesize_d64_track implementation
#include "../src/bios/d64.c"

// Baseline sequential per-sector track synthesis (pre-optimization reference)
static void synthesize_d64_track_orig(uint8_t track_id, uint8_t *dest_ram, uint16_t *out_len) {
  uint8_t phys_track = (track_id >> 1) + 1;
  if (phys_track > d64_max_tracks) {
    phys_track = d64_max_tracks;
  }

  uint8_t num_sectors = (phys_track <= 40) ? sectors_per_track[phys_track] : 17;
  uint8_t zone = get_speed_zone(phys_track);
  uint16_t target_len = zone_track_len[zone];

  uint16_t base_gap = (target_len - (num_sectors * 354)) / num_sectors;
  if (base_gap < 4) base_gap = 4;

  uint32_t d64_offset = get_track_d64_offset(phys_track);
  uint16_t track_pos = 0;
  uint8_t sec_buf[256] __attribute__((aligned(4)));

  for (uint8_t sec = 0; sec < num_sectors; sec++) {
    bridge_ds_read(D64_SLOT_ID, d64_offset, 256, sec_buf);
    d64_offset += 256;

    for (int i = 0; i < 5; i++) {
      dest_ram[track_pos++] = 0xFF;
    }

    uint8_t hdr_chk = sec ^ phys_track ^ disk_id2 ^ disk_id1;
    uint8_t raw_hdr[8] = {0x08, hdr_chk, sec, phys_track, disk_id2, disk_id1, 0x0F, 0x0F};

    // Encode to dest_ram buffer
    uint32_t g01 = byte_to_gcr[raw_hdr[0]];
    uint32_t g23 = byte_to_gcr[raw_hdr[1]];
    uint32_t g45 = byte_to_gcr[raw_hdr[2]];
    uint32_t g67 = byte_to_gcr[raw_hdr[3]];
    dest_ram[track_pos + 0] = (uint8_t)(g01 >> 2);
    dest_ram[track_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
    dest_ram[track_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
    dest_ram[track_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
    dest_ram[track_pos + 4] = (uint8_t)g67;
    track_pos += 5;

    g01 = byte_to_gcr[raw_hdr[4]];
    g23 = byte_to_gcr[raw_hdr[5]];
    g45 = byte_to_gcr[raw_hdr[6]];
    g67 = byte_to_gcr[raw_hdr[7]];
    dest_ram[track_pos + 0] = (uint8_t)(g01 >> 2);
    dest_ram[track_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
    dest_ram[track_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
    dest_ram[track_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
    dest_ram[track_pos + 4] = (uint8_t)g67;
    track_pos += 5;

    for (int i = 0; i < 9; i++) {
      dest_ram[track_pos++] = 0x55;
    }

    for (int i = 0; i < 5; i++) {
      dest_ram[track_pos++] = 0xFF;
    }

    uint8_t chunk0[4] = {0x07, sec_buf[0], sec_buf[1], sec_buf[2]};
    g01 = byte_to_gcr[chunk0[0]];
    g23 = byte_to_gcr[chunk0[1]];
    g45 = byte_to_gcr[chunk0[2]];
    g67 = byte_to_gcr[chunk0[3]];
    dest_ram[track_pos + 0] = (uint8_t)(g01 >> 2);
    dest_ram[track_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
    dest_ram[track_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
    dest_ram[track_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
    dest_ram[track_pos + 4] = (uint8_t)g67;
    track_pos += 5;

    uint8_t data_chk = sec_buf[0] ^ sec_buf[1] ^ sec_buf[2];

    for (int c = 0; c < 63; c++) {
      int byte_idx = 3 + c * 4;
      data_chk ^= sec_buf[byte_idx] ^ sec_buf[byte_idx + 1] ^
                  sec_buf[byte_idx + 2] ^ sec_buf[byte_idx + 3];

      g01 = byte_to_gcr[sec_buf[byte_idx]];
      g23 = byte_to_gcr[sec_buf[byte_idx + 1]];
      g45 = byte_to_gcr[sec_buf[byte_idx + 2]];
      g67 = byte_to_gcr[sec_buf[byte_idx + 3]];
      dest_ram[track_pos + 0] = (uint8_t)(g01 >> 2);
      dest_ram[track_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
      dest_ram[track_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
      dest_ram[track_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
      dest_ram[track_pos + 4] = (uint8_t)g67;
      track_pos += 5;
    }

    data_chk ^= sec_buf[255];
    uint8_t chunk_last[4] = {sec_buf[255], data_chk, 0x00, 0x00};
    g01 = byte_to_gcr[chunk_last[0]];
    g23 = byte_to_gcr[chunk_last[1]];
    g45 = byte_to_gcr[chunk_last[2]];
    g67 = byte_to_gcr[chunk_last[3]];
    dest_ram[track_pos + 0] = (uint8_t)(g01 >> 2);
    dest_ram[track_pos + 1] = (uint8_t)((g01 << 6) | (g23 >> 4));
    dest_ram[track_pos + 2] = (uint8_t)((g23 << 4) | (g45 >> 6));
    dest_ram[track_pos + 3] = (uint8_t)((g45 << 2) | (g67 >> 8));
    dest_ram[track_pos + 4] = (uint8_t)g67;
    track_pos += 5;

    for (int i = 0; i < base_gap; i++) {
      if (track_pos < target_len) {
        dest_ram[track_pos++] = 0x55;
      }
    }
  }

  while (track_pos < target_len) {
    dest_ram[track_pos++] = 0x55;
  }

  *out_len = target_len;
}

int main() {
    printf("Testing D64 GCR encoding unit test...\n");

    // Test known header encoding:
    // Header bytes: 0x08, checksum=0, sector=0, track=1, id2=0x41, id1=0x32, 0x0F, 0x0F
    uint8_t raw_hdr[8] = {0x08, 0x00, 0x00, 0x01, 0x41, 0x32, 0x0F, 0x0F};
    memset((void *)C1541_TRACK_RAM, 0, 10);
    encode_4bytes_to_track(&raw_hdr[0], 0);
    encode_4bytes_to_track(&raw_hdr[4], 5);

    // Nibbles for chunk 0: 0, 8, 0, 0, 0, 0, 0, 1 -> GCR: 0x0A, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B
    // out[0] = (0x0A << 3) | (0x09 >> 2) = 0x50 | 0x02 = 0x52
    uint8_t header_marker = C1541_TRACK_RAM[0];
    assert(header_marker == 0x52); // Standard 1541 GCR header marker $52!

    // Test data block header marker: 0x07
    uint8_t raw_data_hdr[4] = {0x07, 0x00, 0x00, 0x00};
    memset((void *)C1541_TRACK_RAM, 0, 5);
    encode_4bytes_to_track(raw_data_hdr, 0);
    // Nibbles: 0, 7, 0, 0, 0, 0, 0, 0 -> GCR: 0x0A, 0x17, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A
    // out[0] = (0x0A << 3) | (0x17 >> 2) = 0x50 | 0x05 = 0x55
    uint8_t data_marker = C1541_TRACK_RAM[0];
    assert(data_marker == 0x55); // Standard 1541 GCR data marker $55!

    // Verify byte_to_gcr values for nibble consistency
    static const uint8_t bin_to_gcr[16] = {
        0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
        0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
    };
    for (int b = 0; b < 256; b++) {
        uint8_t hi = bin_to_gcr[(b >> 4) & 0x0F];
        uint8_t lo = bin_to_gcr[b & 0x0F];
        uint16_t expected = ((uint16_t)hi << 5) | lo;
        assert(byte_to_gcr[b] == expected);
    }

    printf("Header marker: 0x%02X (expected 0x52)\n", header_marker);
    printf("Data marker:   0x%02X (expected 0x55)\n", data_marker);

    // Test production track synthesis batching against baseline sequential reads
    printf("\nTesting D64 track synthesis batch bridge read optimization...\n");

    // Fill mock D64 image with test pattern
    for (uint32_t i = 0; i < sizeof(mock_d64_file); i++) {
        mock_d64_file[i] = (uint8_t)((i * 37 + 11) & 0xFF);
    }

    uint8_t ram_orig[8192];
    uint8_t ram_prod[8192];
    uint16_t len_orig = 0;

    uint32_t total_tx_orig = 0;
    uint32_t total_tx_opt = 0;

    // Test representative tracks across all 4 speed zones (Tracks 1, 18, 25, 31)
    uint8_t test_tracks[] = {1, 18, 25, 31};
    for (size_t i = 0; i < sizeof(test_tracks); i++) {
        uint8_t trk = test_tracks[i];
        uint8_t track_id = (trk - 1) * 2;
        uint8_t num_sec = sectors_per_track[trk];
        uint16_t expected_tx_opt = (num_sec * 256 + BRIDGE_DPRAM_SIZE - 1) / BRIDGE_DPRAM_SIZE;

        memset(ram_orig, 0, sizeof(ram_orig));
        memset((void *)C1541_TRACK_RAM, 0, sizeof(mock_track_ram));

        // 1. Run baseline sequential per-sector reads
        bridge_tx_count = 0;
        synthesize_d64_track_orig(track_id, ram_orig, &len_orig);
        uint32_t tx_orig = bridge_tx_count;

        // 2. Directly call actual production synthesize_d64_track() from src/bios/d64.c
        bridge_tx_count = 0;
        synthesize_d64_track(track_id);
        uint32_t tx_opt = bridge_tx_count;

        memcpy(ram_prod, (const void *)C1541_TRACK_RAM, sizeof(ram_prod));

        assert(tx_orig == num_sec);
        assert(tx_opt == expected_tx_opt);
        assert(*C1541_TRACK_LEN == len_orig);
        assert(memcmp(ram_orig, ram_prod, len_orig) == 0);

        printf("Track %2d (%2d sectors): Orig %2u tx -> Opt %2u tx (%.1f%% reduction)\n",
               trk, num_sec, tx_orig, tx_opt,
               ((double)(tx_orig - tx_opt) / tx_orig) * 100.0);

        total_tx_orig += tx_orig;
        total_tx_opt += tx_opt;
    }

    printf("Overall representative track seeking: %u -> %u bridge transactions (%.1f%% reduction)\n",
           total_tx_orig, total_tx_opt,
           ((double)(total_tx_orig - total_tx_opt) / total_tx_orig) * 100.0);

    // Verify all 35 tracks: directly exercise production synthesize_d64_track() vs baseline
    for (uint8_t trk = 1; trk <= 35; trk++) {
        uint8_t track_id = (trk - 1) * 2;
        memset(ram_orig, 0, sizeof(ram_orig));
        memset((void *)C1541_TRACK_RAM, 0, sizeof(mock_track_ram));

        synthesize_d64_track_orig(track_id, ram_orig, &len_orig);
        synthesize_d64_track(track_id);

        assert(*C1541_TRACK_LEN == len_orig);
        assert(memcmp(ram_orig, (const void *)C1541_TRACK_RAM, len_orig) == 0);
    }

    printf("All 35 physical tracks verified byte-for-byte identical against production BIOS code!\n");
    printf("All D64 GCR & track synthesis tests passed successfully!\n");
    return 0;
}
