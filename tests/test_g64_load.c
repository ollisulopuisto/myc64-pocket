#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define G64_NUM_TRACKS 84
#define G64_TRACK_OFFSET_TABLE 12
#define BRIDGE_DPRAM_SIZE 1024

// Mock bridge DPRAM storage (standard 35-track G64 is ~260KB)
static uint8_t mock_g64_file[524288];
static uint32_t bridge_tx_count = 0;

static uint16_t mock_bridge_ds_get_uint16(uint16_t slot_id, uint32_t offset) {
  (void)slot_id;
  bridge_tx_count++;
  uint16_t val;
  memcpy(&val, &mock_g64_file[offset], sizeof(uint16_t));
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

// Eager load implementation (prior to lazy loading optimization)
static void load_g64_eager(uint32_t *offsets, uint16_t *sizes) {
  mock_bridge_ds_read(2, G64_TRACK_OFFSET_TABLE, sizeof(uint32_t) * G64_NUM_TRACKS,
                      (uint8_t *)offsets);
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (offsets[i])
      offsets[i] += 2;
  }
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (!offsets[i])
      continue;
    sizes[i] = mock_bridge_ds_get_uint16(2, offsets[i] - 2);
  }
}

// Optimized lazy on-demand load implementation
static void load_g64_lazy(uint32_t *offsets, uint16_t *sizes) {
  mock_bridge_ds_read(2, G64_TRACK_OFFSET_TABLE, sizeof(uint32_t) * G64_NUM_TRACKS,
                      (uint8_t *)offsets);
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    if (offsets[i])
      offsets[i] += 2;
  }
  for (unsigned i = 0; i < G64_NUM_TRACKS; i++) {
    sizes[i] = 0;
  }
}

// On-demand seek track size fetch & cache
static void seek_track_lazy(uint8_t track_id, const uint32_t *offsets, uint16_t *sizes) {
  if (sizes[track_id] == 0 && offsets[track_id]) {
    sizes[track_id] = mock_bridge_ds_get_uint16(2, offsets[track_id] - 2);
  }
}

int main(void) {
  printf("Running G64 track offset table batch read & lazy track size loading optimization tests...\n");

  // Populate mock G64 header:
  memcpy(&mock_g64_file[0], "GCR-1541", 8);
  mock_g64_file[8] = 0;   // version
  mock_g64_file[9] = 84;  // half-tracks
  mock_g64_file[10] = 0xF8;
  mock_g64_file[11] = 0x1E;

  // Set up realistic track offsets and lengths for 35 tracks (even indices):
  uint32_t current_off = 0x02AC;
  const uint16_t sample_track_lens[4] = {7692, 7142, 6666, 6250};
  for (int i = 0; i < G64_NUM_TRACKS; i++) {
    uint32_t off = 0;
    if (i < 70 && (i % 2 == 0)) {
      off = current_off;
      uint16_t trk_len = sample_track_lens[(i / 2) % 4];
      memcpy(&mock_g64_file[off], &trk_len, sizeof(uint16_t));
      current_off += trk_len + 2;
    }
    memcpy(&mock_g64_file[G64_TRACK_OFFSET_TABLE + i * 4], &off, sizeof(uint32_t));
  }

  uint32_t offsets_eager[G64_NUM_TRACKS];
  uint16_t sizes_eager[G64_NUM_TRACKS] = {0};

  uint32_t offsets_lazy[G64_NUM_TRACKS];
  uint16_t sizes_lazy[G64_NUM_TRACKS] = {0};

  // 1. Run eager mounting
  bridge_tx_count = 0;
  load_g64_eager(offsets_eager, sizes_eager);
  uint32_t tx_eager_mount = bridge_tx_count;
  // 1 batch offset table read + 35 scalar track size reads = 36 bridge transactions
  assert(tx_eager_mount == 36);

  // 2. Run lazy mounting
  bridge_tx_count = 0;
  load_g64_lazy(offsets_lazy, sizes_lazy);
  uint32_t tx_lazy_mount = bridge_tx_count;
  // Only 1 batch offset table read!
  assert(tx_lazy_mount == 1);

  // Verify mount transaction reduction
  printf("Mount eager bridge transactions: %u (1 offset table + 35 track sizes)\n", tx_eager_mount);
  printf("Mount lazy bridge transactions:  %u (1 offset table)\n", tx_lazy_mount);
  printf("Mount transaction reduction:     %.1f%% (%u transactions eliminated)\n",
         ((double)(tx_eager_mount - tx_lazy_mount) / tx_eager_mount) * 100.0,
         tx_eager_mount - tx_lazy_mount);

  // Verify offset table equivalence
  for (int i = 0; i < G64_NUM_TRACKS; i++) {
    assert(offsets_eager[i] == offsets_lazy[i]);
  }

  // 3. Simulate drive seeking tracks during operation:
  // First seek to Track 18 (BAM / directory seek, track_id = 34):
  bridge_tx_count = 0;
  seek_track_lazy(34, offsets_lazy, sizes_lazy);
  assert(bridge_tx_count == 1); // 1 scalar read to fetch track 18 size
  assert(sizes_lazy[34] == sizes_eager[34]);

  // Repeated seek to Track 18 (must hit cache, 0 bridge transactions):
  bridge_tx_count = 0;
  seek_track_lazy(34, offsets_lazy, sizes_lazy);
  assert(bridge_tx_count == 0); // Cached!

  // Seek to Track 1 (track_id = 0):
  seek_track_lazy(0, offsets_lazy, sizes_lazy);
  assert(sizes_lazy[0] == sizes_eager[0]);

  // Populate remaining tracks on demand and verify 100% equivalence
  for (int i = 0; i < G64_NUM_TRACKS; i++) {
    seek_track_lazy(i, offsets_lazy, sizes_lazy);
    assert(sizes_lazy[i] == sizes_eager[i]);
  }

  printf("All G64 track offset table batch read & lazy track size loading tests passed successfully!\n");
  return 0;
}
