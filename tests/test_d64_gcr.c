#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// 4-bit nibble to 5-bit GCR lookup table
static const uint8_t bin_to_gcr[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

static void encode_4bytes(const uint8_t *in, uint8_t *out) {
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

  out[0] = ((g0 << 3) | (g1 >> 2)) & 0xFF;
  out[1] = ((g1 << 6) | (g2 << 1) | (g3 >> 4)) & 0xFF;
  out[2] = ((g3 << 4) | (g4 >> 1)) & 0xFF;
  out[3] = ((g4 << 7) | (g5 << 2) | (g6 >> 3)) & 0xFF;
  out[4] = ((g6 << 5) | g7) & 0xFF;
}

int main() {
    printf("Testing D64 GCR encoding unit test...\n");

    // Test known header encoding:
    // Header bytes: 0x08, checksum=0, sector=0, track=1, id2=0x41, id1=0x32, 0x0F, 0x0F
    uint8_t raw_hdr[8] = {0x08, 0x00, 0x00, 0x01, 0x41, 0x32, 0x0F, 0x0F};
    uint8_t gcr_hdr[10];
    encode_4bytes(&raw_hdr[0], &gcr_hdr[0]);
    encode_4bytes(&raw_hdr[4], &gcr_hdr[5]);

    // Nibbles for chunk 0: 0, 8, 0, 0, 0, 0, 0, 1 -> GCR: 0x0A, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B
    // out[0] = (0x0A << 3) | (0x09 >> 2) = 0x50 | 0x02 = 0x52
    assert(gcr_hdr[0] == 0x52); // Standard 1541 GCR header marker $52!

    // Test data block header marker: 0x07
    uint8_t raw_data_hdr[4] = {0x07, 0x00, 0x00, 0x00};
    uint8_t gcr_data[5];
    encode_4bytes(raw_data_hdr, gcr_data);
    // Nibbles: 0, 7, 0, 0, 0, 0, 0, 0 -> GCR: 0x0A, 0x17, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A
    // out[0] = (0x0A << 3) | (0x17 >> 2) = 0x50 | 0x05 = 0x55
    assert(gcr_data[0] == 0x55); // Standard 1541 GCR data marker $55!

    printf("Header marker: 0x%02X (expected 0x52)\n", gcr_hdr[0]);
    printf("Data marker:   0x%02X (expected 0x55)\n", gcr_data[0]);
    printf("All D64 GCR tests passed successfully!\n");
    return 0;
}
