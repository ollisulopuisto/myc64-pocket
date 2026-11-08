#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// 8-bit byte to 10-bit GCR lookup table (matching src/bios/d64.c)
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

static void encode_4bytes_opt(const uint8_t *in, uint8_t *out) {
  uint32_t g01 = byte_to_gcr[in[0]];
  uint32_t g23 = byte_to_gcr[in[1]];
  uint32_t g45 = byte_to_gcr[in[2]];
  uint32_t g67 = byte_to_gcr[in[3]];

  out[0] = (uint8_t)(g01 >> 2);
  out[1] = (uint8_t)((g01 << 6) | (g23 >> 4));
  out[2] = (uint8_t)((g23 << 4) | (g45 >> 6));
  out[3] = (uint8_t)((g45 << 2) | (g67 >> 8));
  out[4] = (uint8_t)g67;
}

#define BRIDGE_DPRAM_SIZE 1024
static uint8_t mock_d64_file[196608];
static uint32_t bridge_tx_count = 0;

static void mock_bridge_ds_read(uint16_t slot_id, uint32_t offset, uint32_t length, uint8_t *dst) {
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

static const uint8_t sectors_per_track[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

static const uint32_t track_d64_offset_lut[42] = {
    0,      0,      5376,   10752,  16128,  21504,  26880,  32256,  37632,
    43008,  48384,  53760,  59136,  64512,  69888,  75264,  80640,  86016,
    91392,  96256,  101120, 105984, 110848, 115712, 120576, 125440, 130048,
    134656, 139264, 143872, 148480, 153088, 157440, 161792, 166144, 170496,
    174848, 179200, 183552, 187904, 192256, 196608
};

static const uint16_t zone_track_len[4] = { 6250, 6666, 7142, 7692 };

#define SPEED_ZONE(trk) ((trk) <= 17 ? 3 : ((trk) <= 24 ? 2 : ((trk) <= 30 ? 1 : 0)))

// Synthesize track using sequential per-sector reads (original approach)
static void synth_orig(uint8_t track_id, uint8_t *track_ram) {
    uint8_t phys_track = (track_id >> 1) + 1;
    uint8_t num_sectors = sectors_per_track[phys_track];
    uint8_t zone = SPEED_ZONE(phys_track);
    uint16_t target_len = zone_track_len[zone];
    uint16_t base_gap = (target_len - (num_sectors * 354)) / num_sectors;
    if (base_gap < 4) base_gap = 4;
    uint32_t d64_offset = track_d64_offset_lut[phys_track];
    uint16_t track_pos = 0;
    uint8_t sec_buf[256];

    for (uint8_t sec = 0; sec < num_sectors; sec++) {
        mock_bridge_ds_read(3, d64_offset, 256, sec_buf);
        d64_offset += 256;

        for (int i = 0; i < 5; i++) track_ram[track_pos++] = 0xFF;

        uint8_t hdr_chk = sec ^ phys_track ^ 0x41 ^ 0x32;
        uint8_t raw_hdr[8] = {0x08, hdr_chk, sec, phys_track, 0x41, 0x32, 0x0F, 0x0F};
        encode_4bytes_opt(&raw_hdr[0], &track_ram[track_pos]);
        track_pos += 5;
        encode_4bytes_opt(&raw_hdr[4], &track_ram[track_pos]);
        track_pos += 5;

        for (int i = 0; i < 9; i++) track_ram[track_pos++] = 0x55;
        for (int i = 0; i < 5; i++) track_ram[track_pos++] = 0xFF;

        uint8_t chunk0[4] = {0x07, sec_buf[0], sec_buf[1], sec_buf[2]};
        encode_4bytes_opt(chunk0, &track_ram[track_pos]);
        track_pos += 5;

        uint8_t data_chk = sec_buf[0] ^ sec_buf[1] ^ sec_buf[2];
        for (int c = 0; c < 63; c++) {
            int byte_idx = 3 + c * 4;
            data_chk ^= sec_buf[byte_idx] ^ sec_buf[byte_idx + 1] ^
                        sec_buf[byte_idx + 2] ^ sec_buf[byte_idx + 3];
            encode_4bytes_opt(&sec_buf[byte_idx], &track_ram[track_pos]);
            track_pos += 5;
        }

        data_chk ^= sec_buf[255];
        uint8_t chunk_last[4] = {sec_buf[255], data_chk, 0x00, 0x00};
        encode_4bytes_opt(chunk_last, &track_ram[track_pos]);
        track_pos += 5;

        for (int i = 0; i < base_gap; i++) {
            if (track_pos < target_len) track_ram[track_pos++] = 0x55;
        }
    }
    while (track_pos < target_len) track_ram[track_pos++] = 0x55;
}

// Synthesize track using batch read (optimized approach)
static void synth_opt(uint8_t track_id, uint8_t *track_ram) {
    uint8_t phys_track = (track_id >> 1) + 1;
    uint8_t num_sectors = sectors_per_track[phys_track];
    uint8_t zone = SPEED_ZONE(phys_track);
    uint16_t target_len = zone_track_len[zone];
    uint16_t base_gap = (target_len - (num_sectors * 354)) / num_sectors;
    if (base_gap < 4) base_gap = 4;
    uint32_t d64_offset = track_d64_offset_lut[phys_track];
    uint16_t track_pos = 0;
    uint8_t track_buf[21 * 256] __attribute__((aligned(4)));

    uint32_t track_raw_bytes = (uint32_t)num_sectors * 256;
    mock_bridge_ds_read(3, d64_offset, track_raw_bytes, track_buf);

    for (uint8_t sec = 0; sec < num_sectors; sec++) {
        const uint8_t *sec_data = &track_buf[sec * 256];

        for (int i = 0; i < 5; i++) track_ram[track_pos++] = 0xFF;

        uint8_t hdr_chk = sec ^ phys_track ^ 0x41 ^ 0x32;
        uint8_t raw_hdr[8] = {0x08, hdr_chk, sec, phys_track, 0x41, 0x32, 0x0F, 0x0F};
        encode_4bytes_opt(&raw_hdr[0], &track_ram[track_pos]);
        track_pos += 5;
        encode_4bytes_opt(&raw_hdr[4], &track_ram[track_pos]);
        track_pos += 5;

        for (int i = 0; i < 9; i++) track_ram[track_pos++] = 0x55;
        for (int i = 0; i < 5; i++) track_ram[track_pos++] = 0xFF;

        uint8_t chunk0[4] = {0x07, sec_data[0], sec_data[1], sec_data[2]};
        encode_4bytes_opt(chunk0, &track_ram[track_pos]);
        track_pos += 5;

        uint8_t data_chk = sec_data[0] ^ sec_data[1] ^ sec_data[2];
        for (int c = 0; c < 63; c++) {
            int byte_idx = 3 + c * 4;
            data_chk ^= sec_data[byte_idx] ^ sec_data[byte_idx + 1] ^
                        sec_data[byte_idx + 2] ^ sec_data[byte_idx + 3];
            encode_4bytes_opt(&sec_data[byte_idx], &track_ram[track_pos]);
            track_pos += 5;
        }

        data_chk ^= sec_data[255];
        uint8_t chunk_last[4] = {sec_data[255], data_chk, 0x00, 0x00};
        encode_4bytes_opt(chunk_last, &track_ram[track_pos]);
        track_pos += 5;

        for (int i = 0; i < base_gap; i++) {
            if (track_pos < target_len) track_ram[track_pos++] = 0x55;
        }
    }
    while (track_pos < target_len) track_ram[track_pos++] = 0x55;
}

int main() {
    printf("Testing D64 GCR encoding unit test...\n");

    // Test known header encoding:
    // Header bytes: 0x08, checksum=0, sector=0, track=1, id2=0x41, id1=0x32, 0x0F, 0x0F
    uint8_t raw_hdr[8] = {0x08, 0x00, 0x00, 0x01, 0x41, 0x32, 0x0F, 0x0F};
    uint8_t gcr_hdr[10];
    encode_4bytes_opt(&raw_hdr[0], &gcr_hdr[0]);
    encode_4bytes_opt(&raw_hdr[4], &gcr_hdr[5]);

    // Nibbles for chunk 0: 0, 8, 0, 0, 0, 0, 0, 1 -> GCR: 0x0A, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B
    // out[0] = (0x0A << 3) | (0x09 >> 2) = 0x50 | 0x02 = 0x52
    assert(gcr_hdr[0] == 0x52); // Standard 1541 GCR header marker $52!

    // Test data block header marker: 0x07
    uint8_t raw_data_hdr[4] = {0x07, 0x00, 0x00, 0x00};
    uint8_t gcr_data[5];
    encode_4bytes_opt(raw_data_hdr, gcr_data);
    // Nibbles: 0, 7, 0, 0, 0, 0, 0, 0 -> GCR: 0x0A, 0x17, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A
    // out[0] = (0x0A << 3) | (0x17 >> 2) = 0x50 | 0x05 = 0x55
    assert(gcr_data[0] == 0x55); // Standard 1541 GCR data marker $55!

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

    printf("Header marker: 0x%02X (expected 0x52)\n", gcr_hdr[0]);
    printf("Data marker:   0x%02X (expected 0x55)\n", gcr_data[0]);

    // Test track synthesis batching vs sequential bridge reads
    printf("\nTesting D64 track synthesis batch bridge read optimization...\n");

    // Fill mock D64 image with test pattern
    for (uint32_t i = 0; i < sizeof(mock_d64_file); i++) {
        mock_d64_file[i] = (uint8_t)((i * 37 + 11) & 0xFF);
    }

    uint8_t ram_orig[8192];
    uint8_t ram_opt[8192];

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
        memset(ram_opt, 0, sizeof(ram_opt));

        bridge_tx_count = 0;
        synth_orig(track_id, ram_orig);
        uint32_t tx_orig = bridge_tx_count;

        bridge_tx_count = 0;
        synth_opt(track_id, ram_opt);
        uint32_t tx_opt = bridge_tx_count;

        assert(tx_orig == num_sec);
        assert(tx_opt == expected_tx_opt);
        assert(memcmp(ram_orig, ram_opt, zone_track_len[SPEED_ZONE(trk)]) == 0);

        printf("Track %2d (%2d sectors): Orig %2u tx -> Opt %2u tx (%.1f%% reduction)\n",
               trk, num_sec, tx_orig, tx_opt,
               ((double)(tx_orig - tx_opt) / tx_orig) * 100.0);

        total_tx_orig += tx_orig;
        total_tx_opt += tx_opt;
    }

    printf("Overall representative track seeking: %u -> %u bridge transactions (%.1f%% reduction)\n",
           total_tx_orig, total_tx_opt,
           ((double)(total_tx_orig - total_tx_opt) / total_tx_orig) * 100.0);

    // Verify all 35 tracks for 100% byte equivalence
    for (uint8_t trk = 1; trk <= 35; trk++) {
        uint8_t track_id = (trk - 1) * 2;
        memset(ram_orig, 0, sizeof(ram_orig));
        memset(ram_opt, 0, sizeof(ram_opt));
        synth_orig(track_id, ram_orig);
        synth_opt(track_id, ram_opt);
        assert(memcmp(ram_orig, ram_opt, zone_track_len[SPEED_ZONE(trk)]) == 0);
    }

    printf("All 35 physical tracks verified byte-for-byte identical!\n");
    printf("All D64 GCR & track synthesis tests passed successfully!\n");
    return 0;
}
