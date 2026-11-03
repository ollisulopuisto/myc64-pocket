import os
import sys
import tempfile
import pytest

# Ensure repository root is on Python path
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from utils.d64_to_g64 import (
    BIN_TO_GCR,
    BYTE_TO_GCR,
    encode_4bytes,
    gcr_encode_block,
    d64_to_g64,
    SECTORS_PER_TRACK,
)

def test_byte_to_gcr_lut_consistency():
    """Verify that BYTE_TO_GCR table correctly reflects BIN_TO_GCR high and low nibbles."""
    assert len(BYTE_TO_GCR) == 256
    for b in range(256):
        hi = BIN_TO_GCR[(b >> 4) & 0x0F]
        lo = BIN_TO_GCR[b & 0x0F]
        expected = (hi << 5) | lo
        assert BYTE_TO_GCR[b] == expected

def test_gcr_encode_block_matches_encode_4bytes():
    """Verify that gcr_encode_block yields identical results to encode_4bytes for all test vectors."""
    # Test all 256 byte values in chunks of 4
    test_bytes = bytes(range(256))
    encoded = gcr_encode_block(test_bytes)
    assert len(encoded) == (256 * 5) // 4

    expected = bytearray()
    for i in range(0, 256, 4):
        expected.extend(encode_4bytes(test_bytes[i], test_bytes[i+1], test_bytes[i+2], test_bytes[i+3]))
    assert encoded == bytes(expected)

def test_gcr_encode_known_markers():
    """Verify standard Commodore 1541 header ($52) and data ($55) block markers."""
    # Header format start: $08 -> nibbles 0, 8 -> GCR 0x0A, 0x09 -> (0x0A << 3) | (0x09 >> 2) = 0x52
    raw_hdr = bytes([0x08, 0x00, 0x00, 0x01, 0x41, 0x32, 0x0F, 0x0F])
    gcr_hdr = gcr_encode_block(raw_hdr)
    assert len(gcr_hdr) == 10
    assert gcr_hdr[0] == 0x52

    # Data format start: $07 -> nibbles 0, 7 -> GCR 0x0A, 0x17 -> (0x0A << 3) | (0x17 >> 2) = 0x55
    raw_data = bytes([0x07, 0x00, 0x00, 0x00])
    gcr_data = gcr_encode_block(raw_data)
    assert len(gcr_data) == 5
    assert gcr_data[0] == 0x55

def test_full_d64_to_g64_conversion():
    """Verify full conversion of a synthetic 35-track D64 disk image to G64 format."""
    total_bytes = sum(SECTORS_PER_TRACK[t] * 256 for t in range(1, 36))
    assert total_bytes == 174848

    d64_data = bytearray(total_bytes)
    # Put disk ID at BAM (Track 18, Sector 0, offset 162-163)
    t18_offset = sum(SECTORS_PER_TRACK[t] * 256 for t in range(1, 18))
    d64_data[t18_offset + 162] = 0x32  # '2'
    d64_data[t18_offset + 163] = 0x41  # 'A'

    with tempfile.TemporaryDirectory() as tmpdir:
        d64_path = os.path.join(tmpdir, "test.d64")
        g64_path = os.path.join(tmpdir, "test.g64")

        with open(d64_path, "wb") as f:
            f.write(d64_data)

        d64_to_g64(d64_path, g64_path)

        assert os.path.exists(g64_path)
        assert os.path.getsize(g64_path) == 252758

        with open(g64_path, "rb") as f:
            header = f.read(12)
            assert header[:8] == b"GCR-1541"
            assert header[8] == 0  # version
            assert header[9] == 84  # half-tracks
