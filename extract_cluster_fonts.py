#!/usr/bin/env python3
"""Extract Ford Antenna fonts from the user's own 2020 Escape IPC firmware.

Accepted input:
  * LV4T-14C088-AJ.ZIP
  * LV4T-14C088-AJ.VBF
  * an already extracted ifs2.bin

The script writes only to the local project. The distributed project intentionally
contains no Ford font files.
"""
from __future__ import annotations
import argparse, io, struct, tarfile, zipfile, zlib
from pathlib import Path

OUT_NAMES = {
    "light": "FordAntennaCondWGL-Light.otf",
    "regular": "FordAntennaCondWGL-Reg.otf",
}

def load_input(path: Path) -> bytes:
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            candidates = [n for n in z.namelist() if n.lower().endswith('.vbf')]
            if not candidates:
                raise SystemExit("ZIP does not contain a VBF file")
            return z.read(candidates[0])
    return path.read_bytes()

def looks_like_ifs2(data: bytes) -> bool:
    return (b"Ford Antenna Cond" in data or b"Antenna CondLight" in data) and b"OTTO" in data

def recover_ifs2(data: bytes) -> bytes:
    if looks_like_ifs2(data):
        return data

    # The Jade 14C088-AJ package carries ifs2.bin.tar.gz as a contiguous gzip
    # payload inside the VBF. Locate the member by its gzip filename.
    pos = 0
    while True:
        pos = data.find(b"\x1f\x8b\x08", pos)
        if pos < 0:
            break
        hdr = data[pos:pos + 96]
        if b"ifs2.bin.tar\x00" in hdr:
            dec = zlib.decompressobj(16 + zlib.MAX_WBITS)
            tar_bytes = dec.decompress(data[pos:]) + dec.flush()
            if not dec.eof:
                raise SystemExit("Found ifs2 gzip member but it did not decompress cleanly")
            with tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r:") as tf:
                members = [m for m in tf.getmembers() if Path(m.name).name == "ifs2.bin"]
                if not members:
                    raise SystemExit("ifs2 gzip unpacked, but ifs2.bin was not found in the tar")
                f = tf.extractfile(members[0])
                if f is None:
                    raise SystemExit("Could not read ifs2.bin from tar")
                return f.read()
        pos += 1
    raise SystemExit("Could not locate ifs2.bin in this input. Use LV4T-14C088-AJ or an extracted ifs2.bin.")

def sfnt_span(data: bytes, off: int, sig: bytes) -> int | None:
    if off + 12 > len(data) or data[off:off+4] != sig:
        return None
    n = struct.unpack_from(">H", data, off + 4)[0]
    if not (5 <= n <= 64):
        return None
    end = 12 + 16 * n
    tags = set()
    for i in range(n):
        e = off + 12 + 16 * i
        if e + 16 > len(data):
            return None
        tag = data[e:e+4]
        if any(c < 32 or c > 126 for c in tag):
            return None
        _, rel, length = struct.unpack_from(">III", data, e + 4)
        if rel > 32_000_000 or length > 32_000_000:
            return None
        tags.add(tag)
        end = max(end, rel + length)
    if not {b"name", b"head", b"cmap"}.issubset(tags):
        return None
    if off + end > len(data):
        return None
    return end

def contains_name(blob: bytes, name: str) -> bool:
    raw = name.encode("ascii")
    u16 = name.encode("utf-16-be")
    return raw in blob or u16 in blob

def find_font(data: bytes, sig: bytes, names: list[str]) -> bytes | None:
    pos = 0
    while True:
        pos = data.find(sig, pos)
        if pos < 0:
            return None
        span = sfnt_span(data, pos, sig)
        if span:
            blob = data[pos:pos+span]
            if any(contains_name(blob, n) for n in names):
                return blob
        pos += 1

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("firmware", type=Path, help="LV4T-14C088-AJ ZIP/VBF or extracted ifs2.bin")
    ap.add_argument("--output", type=Path, default=Path(__file__).resolve().parent / "assets/ford_ipc/private_fonts")
    args = ap.parse_args()

    raw = load_input(args.firmware)
    ifs2 = recover_ifs2(raw)
    fonts = {
        "light": find_font(ifs2, b"OTTO", ["Ford Antenna Cond Light", "Antenna CondLight"]),
        "regular": find_font(ifs2, b"OTTO", ["Ford Antenna Cond Regular", "Antenna CondRegular"]),
    }
    missing = [k for k,v in fonts.items() if v is None]
    if missing:
        raise SystemExit("Could not identify cluster font(s): " + ", ".join(missing))

    args.output.mkdir(parents=True, exist_ok=True)
    for key, blob in fonts.items():
        out = args.output / OUT_NAMES[key]
        out.write_bytes(blob)
        print(f"wrote {out} ({len(blob):,} bytes)")
    print("Done. These fonts were extracted locally from your supplied IPC firmware.")

if __name__ == "__main__":
    main()
