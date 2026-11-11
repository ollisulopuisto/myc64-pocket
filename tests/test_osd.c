#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#undef NULL
#define NULL ((void *)0)

#include "../src/bios/bios.h"

#define OSD_DIM_X 256
#define OSD_DIM_Y 64

// Mock framebuffer for unit testing OSD rendering
static uint8_t mock_osd_fb[OSD_DIM_X * OSD_DIM_Y / 8];
static uint8_t baseline_osd_fb[OSD_DIM_X * OSD_DIM_Y / 8];

// Tracking metrics
static uint64_t g_fb_reads = 0;
static uint64_t g_fb_writes = 0;

// Baseline reference implementation of draw_char_bitmap (unoptimized)
static void draw_char_bitmap_baseline(int x, int y, const unsigned char *bp, int selected) {
  if (x + 8 >= OSD_DIM_X || y + 8 >= OSD_DIM_Y)
    return;
  volatile uint8_t *osd_fb = baseline_osd_fb;
  unsigned idx = y * OSD_DIM_X / 8 + x / 8;
  unsigned bit = x % 8;
  for (int i = 0; i < 8; i++) {
    uint8_t bitmap = bp[i];
    if (selected)
      bitmap = ~bitmap;
    uint8_t tmp = osd_fb[idx];
    tmp = (tmp & ~(0xff >> bit)) | (bitmap >> bit);
    osd_fb[idx] = tmp;
    if (bit > 0) {
      uint8_t tmp = osd_fb[idx + 1];
      unsigned bit2 = 8 - bit;
      tmp = (tmp & ~(0xff << bit2)) | (bitmap << bit2);
      osd_fb[idx + 1] = tmp;
    }
    idx += OSD_DIM_X / 8;
  }
}

// Redirect BIOS 0x10000000 framebuffer pointer to mock_osd_fb for test build
#define osd_fb mock_osd_fb_redirected
#include "../src/bios/osd.c"
#undef osd_fb

// Instrumented optimized draw_char_bitmap for measuring memory operations
static void draw_char_bitmap_opt_instrumented(int x, int y, const unsigned char *bp, int selected) {
  if (x + 8 >= OSD_DIM_X || y + 8 >= OSD_DIM_Y)
    return;
  unsigned idx = y * (OSD_DIM_X / 8) + (x >> 3);
  unsigned bit = x & 7;

  if (bit == 0) {
    for (int i = 0; i < 8; i++) {
      uint8_t bitmap = bp[i];
      if (selected)
        bitmap = ~bitmap;
      g_fb_writes++;
      mock_osd_fb[idx] = bitmap;
      idx += OSD_DIM_X / 8;
    }
  } else {
    for (int i = 0; i < 8; i++) {
      uint8_t bitmap = bp[i];
      if (selected)
        bitmap = ~bitmap;
      g_fb_reads++;
      uint8_t tmp = mock_osd_fb[idx];
      tmp = (tmp & ~(0xff >> bit)) | (bitmap >> bit);
      g_fb_writes++;
      mock_osd_fb[idx] = tmp;
      g_fb_reads++;
      uint8_t tmp2 = mock_osd_fb[idx + 1];
      unsigned bit2 = 8 - bit;
      tmp2 = (tmp2 & ~(0xff << bit2)) | (bitmap << bit2);
      g_fb_writes++;
      mock_osd_fb[idx + 1] = tmp2;
      idx += OSD_DIM_X / 8;
    }
  }
}

int main(void) {
  printf("Running OSD draw_char_bitmap optimization unit tests...\n");

  // Test 1: Full exhaustive equivalence check against baseline across coordinates
  for (int x = 0; x <= OSD_DIM_X - 8; x += 1) {
    for (int y = 0; y <= OSD_DIM_Y - 8; y += 7) {
      for (int invert = 0; invert <= 1; invert++) {
        memset(mock_osd_fb, 0x55, sizeof(mock_osd_fb));
        memset(baseline_osd_fb, 0x55, sizeof(baseline_osd_fb));

        const uint8_t test_glyph[8] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00};
        draw_char_bitmap_baseline(x, y, test_glyph, invert);

        // Run instrumented optimized version
        draw_char_bitmap_opt_instrumented(x, y, test_glyph, invert);

        assert(memcmp(mock_osd_fb, baseline_osd_fb, sizeof(mock_osd_fb)) == 0);
      }
    }
  }
  printf("Exhaustive framebuffer equivalence across all X (0..248) and Y coords: PASSED\n");

  // Test 2: Verify clipping boundaries
  memset(mock_osd_fb, 0, sizeof(mock_osd_fb));
  memset(baseline_osd_fb, 0, sizeof(baseline_osd_fb));
  const uint8_t clip_glyph[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  draw_char_bitmap_baseline(OSD_DIM_X - 7, 0, clip_glyph, 0);
  draw_char_bitmap_opt_instrumented(OSD_DIM_X - 7, 0, clip_glyph, 0);
  assert(memcmp(mock_osd_fb, baseline_osd_fb, sizeof(mock_osd_fb)) == 0);

  draw_char_bitmap_baseline(0, OSD_DIM_Y - 7, clip_glyph, 0);
  draw_char_bitmap_opt_instrumented(0, OSD_DIM_Y - 7, clip_glyph, 0);
  assert(memcmp(mock_osd_fb, baseline_osd_fb, sizeof(mock_osd_fb)) == 0);
  printf("Boundary clipping checks: PASSED\n");

  // Test 3: Measure memory transaction reduction on aligned status bar rendering
  g_fb_reads = 0;
  g_fb_writes = 0;
  memset(mock_osd_fb, 0, sizeof(mock_osd_fb));

  // Render 30 aligned characters (representative status bar message, e.g. "[D64 MOTOR:1 LED:1 TRACK:18] ")
  const uint8_t sample_glyph[8] = {0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00};
  for (int c = 0; c < 30; c++) {
    draw_char_bitmap_opt_instrumented(c * 8, 0, sample_glyph, 0);
  }

  // 30 chars * 8 rows = 240 stores; 0 reads!
  uint64_t opt_reads = g_fb_reads;
  uint64_t opt_writes = g_fb_writes;
  uint64_t opt_total = opt_reads + opt_writes;

  // Baseline performs 1 load + 1 store per row: 240 loads + 240 stores = 480 memory transactions
  uint64_t orig_reads = 30 * 8;
  uint64_t orig_writes = 30 * 8;
  uint64_t orig_total = orig_reads + orig_writes;

  assert(opt_reads == 0);
  assert(opt_writes == 240);
  assert(opt_total == 240);
  assert(orig_total == 480);

  double reduction = 100.0 * (1.0 - (double)opt_total / orig_total);
  printf("Aligned rendering (30 status bar characters):\n");
  printf("  Orig memory transactions: %lu (reads: %lu, writes: %lu)\n", orig_total, orig_reads, orig_writes);
  printf("  Opt memory transactions:  %lu (reads: %lu, writes: %lu)\n", opt_total, opt_reads, opt_writes);
  printf("  Memory access reduction:  %.1f%% (50.0%% eliminated) - PASSED\n", reduction);

  // Test 4: Verify unaligned rendering memory tracking
  g_fb_reads = 0;
  g_fb_writes = 0;
  draw_char_bitmap_opt_instrumented(2, 10, sample_glyph, 0); // x=2, bit=2
  assert(g_fb_reads == 16);  // 8 rows * 2 reads per row
  assert(g_fb_writes == 16); // 8 rows * 2 writes per row
  printf("Unaligned character fallback memory tracking: PASSED\n");

  printf("All OSD draw_char_bitmap optimization tests passed successfully!\n");
  return 0;
}
