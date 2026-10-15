#!/usr/bin/env python3
"""Unpack EDI LZSS2 (EDI Install Pro) files from civ1win/ -> unpacked/.

Format (from file(1) magic + deark modules/misc3.c + src/fmtutil-cmpr.c):
  0-7:  "EDILZSS2"
  8-20: original filename, NUL-terminated, padded to 13 bytes
  21-24: original size, LE u32
  25..: LZSS stream (Okumura variant):
    flags LSB-first, 1=literal, 0=match(2 bytes:
      matchpos = ((x1 & 0xF0) << 4) | x0, matchlen = (x1 & 0x0F) + 3)
    4K ring buffer prefilled with 0x20, curpos starts at 4096-18.
Non-EDI files are copied verbatim.
"""
import glob
import os
import shutil
import struct
import sys

SRC_GLOB = "civ1win/*"
DST_DIR = "unpacked"


def edi_unpack(data: bytes):
    assert data[:8] == b"EDILZSS2", "not EDI LZSS2"
    fname = data[8:21].split(b"\x00")[0].decode("ascii", errors="replace")
    unpacked_len = struct.unpack("<I", data[21:25])[0]
    comp = data[25:]
    bufn = 4096
    buf = bytearray([0x20]) * bufn
    curpos = bufn - 18
    out = bytearray()
    cpos = 0
    bitbuf = 0
    bits_left = 0

    def getbit():
        nonlocal bitbuf, bits_left, cpos
        if bits_left == 0:
            if cpos >= len(comp):
                return None
            bitbuf = comp[cpos]
            cpos += 1
            bits_left = 8
        b = bitbuf & 1
        bitbuf >>= 1
        bits_left -= 1
        return b

    while len(out) < unpacked_len:
        bit = getbit()
        if bit is None:
            raise ValueError(f"truncated stream at out={len(out)}")
        if bit == 1:
            b = comp[cpos]
            cpos += 1
            out.append(b)
            buf[curpos] = b
            curpos = (curpos + 1) & (bufn - 1)
        else:
            x0 = comp[cpos]
            x1 = comp[cpos + 1]
            cpos += 2
            matchpos = ((x1 & 0xF0) << 4) | x0
            matchlen = (x1 & 0x0F) + 3
            for _ in range(matchlen):
                if len(out) >= unpacked_len:
                    break
                b = buf[matchpos & (bufn - 1)]
                out.append(b)
                buf[curpos] = b
                curpos = (curpos + 1) & (bufn - 1)
                matchpos = (matchpos + 1) & (bufn - 1)
    return fname, unpacked_len, bytes(out)


def main():
    os.makedirs(DST_DIR, exist_ok=True)
    n = 0
    for f in sorted(glob.glob(SRC_GLOB)):
        d = open(f, "rb").read()
        if d[:8] == b"EDILZSS2":
            fname, ulen, out = edi_unpack(d)
            assert len(out) == ulen, f"{f}: {len(out)} != {ulen}"
            open(os.path.join(DST_DIR, fname), "wb").write(out)
            print(f"{f} -> {DST_DIR}/{fname} ({len(out)} B)")
            n += 1
        else:
            base = os.path.basename(f)
            dst = os.path.join(DST_DIR, base)
            if not os.path.exists(dst):
                shutil.copy(f, dst)
                print(f"{f} (raw) -> {dst}")
    print(f"unpacked {n} EDI files")


if __name__ == "__main__":
    sys.exit(main())
