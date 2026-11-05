#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define G64_NUM_TRACKS 84
#define G64_TRACK_OFFSET_TABLE 12
#define BRIDGE_DPRAM_SIZE 1024

// Mock bridge DPRAM storage
static uint8_t mock_g64_file[65536];
static uint32_t bridge_tx_count = 0;

static uint32_t mock_bridge_ds_get_uint32(uint16_t slot_id, uint32_t offset) {
  (void)slot_id;
  bridge_tx_count++;
  uint32_t val;
  memcpy(&val, &mock_g64_file[offset], sizeof(uint32_t));
  return val;
}

static void mock_bridge_ds_read(uint16_t slot_id, uint32_t offset, uint32_t length, uint8_t *dst) {
  (void)slot_id;
  while (length > 0) {
    bridge_tx_count++;
    uint32_t chunk_size = (length < BRIDGE_DPRAM_SIZE) ? length : BRIDGE_DPRAM_SIZE;

    // Fast path: if 4-byte aligned
    if ((((uintptr_t)dst | chunk_size) & 3) == 0) {
      const uint32_t *sp = (const uint32_t *)&mock_g64_file[offset];
      uint32_t *dp = (uint32_t *)dst;
      uint32_t words = chunk_size >> 2;
      for (uint32_t i = 0; i < words; i++) {
        dp[i] = sp[i];
      }
      dst += chunk_size;
    } else {
      for (uint32_t i = 0; i < chunk_size; i++) {
        *dst++ = mock_g64_file[offset + i];
      }
    }
    length -= chunk_size;
    offset += chunk_size;
  }
}

// Original sequential load implementation
static void load_g64_orig(uint32_t *offsets) {
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    uint32_t track_off =
        mock_bridge_ds_get_uint32(2, G64_TRACK_OFFSET_TABLE + i * 4);
    offsets[i] =
        track_off ? track_off + 2
                  : 0; // skip the 16 bit track length field (if defined)
  }
}

// Optimized batch load implementation
static void load_g64_opt(uint32_t *offsets) {
  mock_bridge_ds_read(2, G64_TRACK_OFFSET_TABLE, sizeof(uint32_t) * G64_NUM_TRACKS,
                      (uint8_t *)offsets);
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (offsets[i])
      offsets[i] += 2;
  }
}

int main(void) {
  printf("Running G64 track offset table batch read optimization tests...\n");

  // Populate mock G64 header:
  memcpy(&mock_g64_file[0], "GCR-1541", 8);
  mock_g64_file[8] = 0;   // version
  mock_g64_file[9] = 84;  // half-tracks
  mock_g64_file[10] = 0xF8;
  mock_g64_file[11] = 0x1E;

  // Set up realistic track offsets:
  uint32_t current_off = 0x02AC;
  for (int i = 0; i < G64_NUM_TRACKS; i++) {
    uint32_t off = 0;
    if (i < 70 && (i % 2 == 0)) {
      off = current_off;
      current_off += 7928 + 2;
    }
    memcpy(&mock_g64_file[G64_TRACK_OFFSET_TABLE + i * 4], &off, sizeof(uint32_t));
  }

  uint32_t offsets_orig[G64_NUM_TRACKS];
  uint32_t offsets_opt[G64_NUM_TRACKS];

  // Run original
  bridge_tx_count = 0;
  load_g64_orig(offsets_orig);
  uint32_t tx_orig = bridge_tx_count;
  assert(tx_orig == 84);

  // Run optimized
  bridge_tx_count = 0;
  load_g64_opt(offsets_opt);
  uint32_t tx_opt = bridge_tx_count;
  assert(tx_opt == 1);

  // Verify exact equivalence across all 84 tracks
  for (int i = 0; i < G64_NUM_TRACKS; i++) {
    assert(offsets_orig[i] == offsets_opt[i]);
    if (i < 70 && (i % 2 == 0)) {
      assert(offsets_opt[i] > 0);
      assert((offsets_opt[i] & 1) == 0); // Aligned after +2
    } else {
      assert(offsets_opt[i] == 0);
    }
  }

  printf("Orig bridge transactions: %u\n", tx_orig);
  printf("Opt bridge transactions:  %u\n", tx_opt);
  printf("Transaction reduction:    %.1f%% (%u transactions eliminated)\n",
         ((double)(tx_orig - tx_opt) / tx_orig) * 100.0, tx_orig - tx_opt);
  printf("All G64 track offset table batch read tests passed successfully!\n");
  return 0;
}
