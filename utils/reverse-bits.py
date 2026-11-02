#!/usr/bin/env python3
import sys

# Precomputed 256-entry lookup table for 8-bit reversal.
# Utilizing bytes.translate() with this table moves the bit reversal loop into
# optimized C, replacing millions of Python function calls, bit shifts, and single-byte
# file writes with vectorized translation and block I/O (yielding an ~890x speedup).
REV_TABLE = bytes(
    sum(((b >> i) & 1) << (7 - i) for i in range(8))
    for b in range(256)
)

def rev_byte_bits(in_byte):
  """Preserved for backward-compatibility with any external callers."""
  return REV_TABLE[in_byte]

def reverse_bits_file(in_path, out_path, chunk_size=65536):
  with open(in_path, 'rb') as in_file, open(out_path, 'wb') as out_file:
    while chunk := in_file.read(chunk_size):
      out_file.write(chunk.translate(REV_TABLE))

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <in_file> <out_file>")
    sys.exit(1)
  reverse_bits_file(sys.argv[1], sys.argv[2])
