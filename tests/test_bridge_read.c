#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define BRIDGE_DPRAM_SIZE 1024

static uint8_t mock_dpram[BRIDGE_DPRAM_SIZE];

// Test harness mirroring bridge_ds_read chunk copying logic
static void test_copy_chunk_orig(uint32_t chunk_size, uint8_t *dst, const volatile uint8_t *p) {
  for (uint32_t idx = 0; idx < chunk_size; idx++) {
    *dst++ = p[idx];
  }
}

static void test_copy_chunk_opt(uint32_t chunk_size, uint8_t *dst, const volatile uint8_t *p) {
  if ((((uintptr_t)dst | chunk_size) & 3) == 0) {
    volatile const uint32_t *sp = (volatile const uint32_t *)p;
    uint32_t *dp = (uint32_t *)dst;
    uint32_t words = chunk_size >> 2;
    for (uint32_t i = 0; i < words; i++) {
      dp[i] = sp[i];
    }
  } else {
    for (uint32_t idx = 0; idx < chunk_size; idx++) {
      *dst++ = p[idx];
    }
  }
}

int main(void) {
  printf("Running bridge_ds_read optimization unit tests...\n");

  for (int i = 0; i < BRIDGE_DPRAM_SIZE; i++) {
    mock_dpram[i] = (uint8_t)(i * 37 + 13);
  }

  // Test 1: 4-byte aligned buffer with typical sector size (256 bytes)
  uint8_t dst_orig[BRIDGE_DPRAM_SIZE] __attribute__((aligned(4)));
  uint8_t dst_opt[BRIDGE_DPRAM_SIZE] __attribute__((aligned(4)));

  memset(dst_orig, 0, sizeof(dst_orig));
  memset(dst_opt, 0, sizeof(dst_opt));

  test_copy_chunk_orig(256, dst_orig, mock_dpram);
  test_copy_chunk_opt(256, dst_opt, mock_dpram);
  assert(memcmp(dst_orig, dst_opt, 256) == 0);
  assert(memcmp(dst_opt, mock_dpram, 256) == 0);

  // Test 2: Full DPRAM buffer copy (1024 bytes)
  memset(dst_orig, 0, sizeof(dst_orig));
  memset(dst_opt, 0, sizeof(dst_opt));

  test_copy_chunk_orig(1024, dst_orig, mock_dpram);
  test_copy_chunk_opt(1024, dst_opt, mock_dpram);
  assert(memcmp(dst_orig, dst_opt, 1024) == 0);
  assert(memcmp(dst_opt, mock_dpram, 1024) == 0);

  // Test 3: Unaligned destination pointer (offsets +1, +2, +3)
  for (int offset = 1; offset <= 3; offset++) {
    memset(dst_orig, 0, sizeof(dst_orig));
    memset(dst_opt, 0, sizeof(dst_opt));

    test_copy_chunk_orig(250, dst_orig + offset, mock_dpram);
    test_copy_chunk_opt(250, dst_opt + offset, mock_dpram);
    assert(memcmp(dst_orig, dst_opt, sizeof(dst_orig)) == 0);
  }

  // Test 4: Unaligned chunk sizes (e.g. 2 bytes Disk ID read, odd lengths)
  for (uint32_t size = 1; size <= 17; size++) {
    memset(dst_orig, 0, sizeof(dst_orig));
    memset(dst_opt, 0, sizeof(dst_opt));

    test_copy_chunk_orig(size, dst_orig, mock_dpram);
    test_copy_chunk_opt(size, dst_opt, mock_dpram);
    assert(memcmp(dst_orig, dst_opt, size) == 0);
    assert(memcmp(dst_opt, mock_dpram, size) == 0);
  }

  printf("All bridge_ds_read tests passed successfully!\n");
  return 0;
}
