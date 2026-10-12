#
# Savestate scan chain for Amaranth designs.
#
# add_scan_chain() walks an elaborated fragment tree and threads every
# register of the "sync" domain into one 32-bit-wide shift chain. While
# i_ss_shift is high each register takes the value of the register 32 bits
# below it in the chain and the lowest 32 bits take i_ss_si; o_ss_so is the
# top 32 bits. Feeding o_ss_so back into i_ss_si rotates the chain, so after
# o_ss_words shifts the state is unchanged (save); shifting saved words back
# in the same order restores it (load).
#
# The chain statements are appended after everything else, so they override
# the clock enable hold that EnableInserter adds. Run this on the fragment
# returned by Fragment.get(EnableInserter(...)(top), None).
#
# Registers that a black box (T65 Instance) cannot expose are handled through
# captures: register_capture(shadow, q) makes the pass add
# "if i_ss_capture: shadow <= q" after the enable hold, and shadow is a normal
# chain register.
#
# This uses Amaranth 0.5 internals (Fragment.statements, _lhs_signals); the
# build pins amaranth==0.5.10.
#

import json
import zlib

from amaranth.hdl import Signal, Cat, Const
from amaranth.hdl._ast import Switch, _StatementList
from amaranth.hdl._ir import Fragment, Instance
from amaranth.hdl._mem import MemoryInstance

_captures = []


def register_capture(shadow, q):
  _captures.append((shadow, q))


def _walk(fragment, path):
  yield fragment, path
  for subfragment, name, _ in fragment.subfragments:
    if isinstance(subfragment, (Instance, MemoryInstance)):
      continue
    yield from _walk(subfragment, path + (str(name),))


def add_scan_chain(fragment, i_shift, i_si, o_so, i_capture, o_words, o_hash, map_path, exclude=()):
  exclude = {id(s) for s in exclude}

  # Registers per fragment, in a stable order
  owners = []
  for frag, path in _walk(fragment, ()):
    stmts = frag.statements.get("sync")
    if not stmts:
      continue
    sigs = {}
    for stmt in stmts:
      for sig in stmt._lhs_signals():
        if id(sig) not in exclude:
          sigs[id(sig)] = sig
    ordered = sorted(sigs.values(), key=lambda s: (s.name, len(s)))
    if ordered:
      owners.append((frag, path, ordered))
  owners.sort(key=lambda o: "/".join(o[1]))

  entries = []
  offset = 0
  for frag, path, sigs in owners:
    for sig in sigs:
      entries.append((frag, path, sig, offset))
      offset += len(sig)

  # Pad to whole words with a register in the top fragment
  pad_len = (-offset) % 32
  chain_sigs = [e[2] for e in entries]
  if pad_len:
    pad = Signal(pad_len, name="ss_pad")
    entries.append((fragment, (), pad, offset))
    chain_sigs.append(pad)
    offset += pad_len
  n_bits = offset
  n_words = n_bits // 32

  chain = Cat(*chain_sigs)
  shifted = Cat(i_si, chain[0:n_bits - 32]) if n_words > 1 else i_si

  # Per fragment: on shift, every register loads its slice of the shifted chain
  per_frag = {}
  for frag, path, sig, off in entries:
    per_frag.setdefault(id(frag), (frag, []))[1].append(sig.eq(shifted[off:off + len(sig)]))
  for frag, stmts in per_frag.values():
    frag.statements.setdefault("sync", _StatementList()).append(Switch(i_shift, [(1, stmts, None)]))

  # Captures (after the enable hold, like the chain)
  for shadow, q in _captures:
    owner = next((e[0] for e in entries if e[2] is shadow), None)
    assert owner is not None, f"capture register {shadow.name} is not in the chain"
    owner.statements["sync"].append(Switch(i_capture, [(1, [shadow.eq(q)], None)]))

  layout = [{"path": "/".join(path), "name": sig.name, "width": len(sig), "offset": off}
            for _, path, sig, off in entries]
  digest = zlib.crc32(json.dumps(layout, sort_keys=True).encode()) & 0xffffffff
  with open(map_path, "w") as f:
    json.dump({"bits": n_bits, "words": n_words, "hash": "%08x" % digest, "registers": layout}, f, indent=1)

  fragment.add_statements("comb",
                          o_so.eq(chain[n_bits - 32:n_bits]),
                          o_words.eq(n_words),
                          o_hash.eq(digest))
  return n_words, digest
