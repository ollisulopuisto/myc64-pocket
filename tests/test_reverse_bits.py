import os
import subprocess
import sys
import tempfile
import pytest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCRIPT_PATH = os.path.join(REPO_ROOT, "utils", "reverse-bits.py")

def reference_rev_byte(b):
    out = 0
    for i in range(8):
        if b & (1 << i):
            out |= (1 << (7 - i))
    return out

def run_reverse_bits(in_bytes):
    with tempfile.TemporaryDirectory() as tmpdir:
        in_file = os.path.join(tmpdir, "in.bin")
        out_file = os.path.join(tmpdir, "out.bin")
        with open(in_file, "wb") as f:
            f.write(in_bytes)

        res = subprocess.run([sys.executable, SCRIPT_PATH, in_file, out_file], capture_output=True)
        assert res.returncode == 0, f"Script failed: {res.stderr.decode()}"

        with open(out_file, "rb") as f:
            return f.read()

def test_all_256_byte_reversals():
    """Verify bit reversal of every byte value 0..255."""
    in_bytes = bytes(range(256))
    expected = bytes(reference_rev_byte(b) for b in in_bytes)
    actual = run_reverse_bits(in_bytes)
    assert actual == expected

def test_empty_file():
    """Verify handling of empty input file."""
    actual = run_reverse_bits(b"")
    assert actual == b""

def test_multi_chunk_payload():
    """Verify processing across multiple chunk boundaries (e.g., >64KB)."""
    # 130KB payload with deterministic pattern
    pattern = bytes((i * 37) % 256 for i in range(130000))
    expected = bytes(reference_rev_byte(b) for b in pattern)
    actual = run_reverse_bits(pattern)
    assert actual == expected

def test_known_byte_inversions():
    """Verify specific known bit reversals: 0x80 -> 0x01, 0x0F -> 0xF0, 0xAA -> 0x55."""
    in_bytes = bytes([0x80, 0x01, 0x0F, 0xF0, 0xAA, 0x55, 0xC3])
    expected = bytes([0x01, 0x80, 0xF0, 0x0F, 0x55, 0xAA, 0xC3])
    actual = run_reverse_bits(in_bytes)
    assert actual == expected
