#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BRIDGE_DPRAM_SIZE 1024

static uint8_t mock_crt_file[131072];
static uint32_t bridge_tx_count = 0;

static inline uint16_t swap16(uint16_t x) { return (x << 8) | (x >> 8); }
static inline uint32_t swap32(uint32_t x) {
  return ((uint32_t)swap16(x) << 16) | ((uint32_t)swap16(x >> 16));
}

static uint16_t mock_bridge_ds_get_uint16(uint16_t slot_id, uint32_t offset) {
  (void)slot_id;
  bridge_tx_count++;
  uint16_t val;
  memcpy(&val, &mock_crt_file[offset], sizeof(uint16_t));
  return val;
}

static uint32_t mock_bridge_ds_get_uint32(uint16_t slot_id, uint32_t offset) {
  (void)slot_id;
  bridge_tx_count++;
  uint32_t val;
  memcpy(&val, &mock_crt_file[offset], sizeof(uint32_t));
  return val;
}

static void mock_bridge_ds_read(uint16_t slot_id, uint32_t offset, uint32_t length, uint8_t *dst) {
  (void)slot_id;
  while (length > 0) {
    bridge_tx_count++;
    uint32_t chunk_size = (length < BRIDGE_DPRAM_SIZE) ? length : BRIDGE_DPRAM_SIZE;

    // Fast path: if 4-byte aligned
    if ((((uintptr_t)dst | chunk_size) & 3) == 0) {
      const uint32_t *sp = (const uint32_t *)&mock_crt_file[offset];
      uint32_t *dp = (uint32_t *)dst;
      uint32_t words = chunk_size >> 2;
      for (uint32_t i = 0; i < words; i++) {
        dp[i] = sp[i];
      }
      dst += chunk_size;
    } else {
      for (uint32_t i = 0; i < chunk_size; i++) {
        *dst++ = mock_crt_file[offset + i];
      }
    }
    length -= chunk_size;
    offset += chunk_size;
  }
}

typedef struct {
  uint32_t signature;
  uint32_t packet_length;
  uint16_t bank_number;
  uint16_t load_address;
  uint16_t image_size;
  uint8_t payload_checksum;
} chip_info_t;

// Original CRT CHIP packet loading logic
static int load_crt_orig(uint32_t slot_length, chip_info_t *chips_out, int max_chips) {
  uint32_t chip_packet_base = 0x40;
  int count = 0;
  while (chip_packet_base < slot_length && count < max_chips) {
    uint32_t signature =
        swap32(mock_bridge_ds_get_uint32(0, chip_packet_base));
    if (signature != 0x43484950) // Check for "CHIP" signature
      return count;
    uint32_t chip_packet_length =
        swap32(mock_bridge_ds_get_uint32(0, chip_packet_base + 0x4));

    uint16_t bank_number =
        swap16(mock_bridge_ds_get_uint16(0, chip_packet_base + 0xa));
    uint16_t load_address =
        swap16(mock_bridge_ds_get_uint16(0, chip_packet_base + 0xc));
    uint16_t image_size =
        swap16(mock_bridge_ds_get_uint16(0, chip_packet_base + 0xe));

    uint8_t rom_buf[8192] __attribute__((aligned(4)));
    mock_bridge_ds_read(0, chip_packet_base + 0x10, image_size, rom_buf);

    chips_out[count].signature = signature;
    chips_out[count].packet_length = chip_packet_length;
    chips_out[count].bank_number = bank_number;
    chips_out[count].load_address = load_address;
    chips_out[count].image_size = image_size;
    uint8_t chk = 0;
    for (int i = 0; i < image_size; i++) chk ^= rom_buf[i];
    chips_out[count].payload_checksum = chk;

    chip_packet_base += chip_packet_length;
    count++;
  }
  return count;
}

// Optimized CRT CHIP packet loading logic
static int load_crt_opt(uint32_t slot_length, chip_info_t *chips_out, int max_chips) {
  uint32_t chip_packet_base = 0x40;
  uint8_t chip_hdr[16] __attribute__((aligned(4)));
  int count = 0;

  while (chip_packet_base < slot_length && count < max_chips) {
    mock_bridge_ds_read(0, chip_packet_base, sizeof(chip_hdr), chip_hdr);

    uint32_t signature = swap32(*(uint32_t *)&chip_hdr[0x0]);
    if (signature != 0x43484950) // Check for "CHIP" signature
      return count;
    uint32_t chip_packet_length = swap32(*(uint32_t *)&chip_hdr[0x4]);

    uint16_t bank_number = swap16(*(uint16_t *)&chip_hdr[0xa]);
    uint16_t load_address = swap16(*(uint16_t *)&chip_hdr[0xc]);
    uint16_t image_size = swap16(*(uint16_t *)&chip_hdr[0xe]);

    uint8_t rom_buf[8192] __attribute__((aligned(4)));
    mock_bridge_ds_read(0, chip_packet_base + 0x10, image_size, rom_buf);

    chips_out[count].signature = signature;
    chips_out[count].packet_length = chip_packet_length;
    chips_out[count].bank_number = bank_number;
    chips_out[count].load_address = load_address;
    chips_out[count].image_size = image_size;
    uint8_t chk = 0;
    for (int i = 0; i < image_size; i++) chk ^= rom_buf[i];
    chips_out[count].payload_checksum = chk;

    chip_packet_base += chip_packet_length;
    count++;
  }
  return count;
}

int main(void) {
  printf("Running CRT CHIP packet header batch read optimization tests...\n");

  // Build a realistic CRT image with 8 CHIP packets (e.g. 64KB Magic Desk cartridge: 8 x 8KB banks)
  // CRT Header (0x00..0x3F):
  memcpy(&mock_crt_file[0x00], "C64 CARTRIDGE   ", 16);
  uint32_t crt_hdr_len = swap32(0x0040);
  memcpy(&mock_crt_file[0x10], &crt_hdr_len, 4);
  uint16_t crt_ver = swap16(0x0100);
  memcpy(&mock_crt_file[0x14], &crt_ver, 2);
  uint16_t cart_hw = swap16(19); // Magic Desk
  memcpy(&mock_crt_file[0x16], &cart_hw, 2);

  const int num_chips = 8;
  const uint16_t chip_size = 8192;
  const uint32_t packet_size = 16 + chip_size; // 0x2010

  uint32_t curr_pos = 0x40;
  for (int b = 0; b < num_chips; b++) {
    // CHIP signature "CHIP" = 0x43 0x48 0x49 0x50
    mock_crt_file[curr_pos + 0] = 'C';
    mock_crt_file[curr_pos + 1] = 'H';
    mock_crt_file[curr_pos + 2] = 'I';
    mock_crt_file[curr_pos + 3] = 'P';

    uint32_t pkt_len = swap32(packet_size);
    memcpy(&mock_crt_file[curr_pos + 4], &pkt_len, 4);

    uint16_t chip_type = swap16(0); // ROM
    memcpy(&mock_crt_file[curr_pos + 8], &chip_type, 2);

    uint16_t bank = swap16(b);
    memcpy(&mock_crt_file[curr_pos + 0xa], &bank, 2);

    uint16_t addr = swap16(0x8000);
    memcpy(&mock_crt_file[curr_pos + 0xc], &addr, 2);

    uint16_t sz = swap16(chip_size);
    memcpy(&mock_crt_file[curr_pos + 0xe], &sz, 2);

    // Payload data pattern
    for (int i = 0; i < chip_size; i++) {
      mock_crt_file[curr_pos + 16 + i] = (uint8_t)((b * 73 + i * 19) & 0xFF);
    }

    curr_pos += packet_size;
  }

  uint32_t total_crt_len = curr_pos;

  chip_info_t chips_orig[16];
  chip_info_t chips_opt[16];

  // Run original sequential test
  bridge_tx_count = 0;
  int n_orig = load_crt_orig(total_crt_len, chips_orig, 16);
  uint32_t tx_orig = bridge_tx_count;
  assert(n_orig == num_chips);

  // Run optimized batch test
  bridge_tx_count = 0;
  int n_opt = load_crt_opt(total_crt_len, chips_opt, 16);
  uint32_t tx_opt = bridge_tx_count;
  assert(n_opt == num_chips);

  // Each CHIP packet payload is 8192 bytes = 8 chunks of 1024 bytes
  // Header:
  // - Orig: 5 scalar transactions (2x uint32, 3x uint16)
  // - Opt: 1 batch transaction (16 bytes)
  // Payload: 8 transactions per chip
  // Total transactions per chip:
  // - Orig: 5 + 8 = 13
  // - Opt: 1 + 8 = 9
  // Across 8 chips:
  // - Orig: 13 * 8 = 104
  // - Opt: 9 * 8 = 72
  // Header transactions alone:
  // - Orig: 40
  // - Opt: 8 (80% reduction in header transactions)
  printf("Chips loaded: %d\n", n_opt);
  printf("Orig bridge transactions: %u (header: %u, data: %u)\n", tx_orig, num_chips * 5, tx_orig - num_chips * 5);
  printf("Opt bridge transactions:  %u (header: %u, data: %u)\n", tx_opt, num_chips * 1, tx_opt - num_chips * 1);
  printf("Total transaction reduction: %.1f%% (%u transactions eliminated)\n",
         ((double)(tx_orig - tx_opt) / tx_orig) * 100.0, tx_orig - tx_opt);
  printf("Header transaction reduction: %.1f%% (%u transactions eliminated)\n",
         ((double)(num_chips * 5 - num_chips * 1) / (num_chips * 5)) * 100.0, num_chips * 4);

  assert(tx_orig == 104);
  assert(tx_opt == 72);

  // Verify correctness of every parsed field and payload checksum
  for (int b = 0; b < num_chips; b++) {
    assert(chips_orig[b].signature == chips_opt[b].signature);
    assert(chips_orig[b].packet_length == chips_opt[b].packet_length);
    assert(chips_orig[b].bank_number == chips_opt[b].bank_number);
    assert(chips_orig[b].load_address == chips_opt[b].load_address);
    assert(chips_orig[b].image_size == chips_opt[b].image_size);
    assert(chips_orig[b].payload_checksum == chips_opt[b].payload_checksum);
  }

  printf("All CRT CHIP packet header batch read tests passed successfully!\n");
  return 0;
}
