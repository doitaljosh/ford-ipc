#!/usr/bin/env python3
"""Extract the 2020 Escape ACM chime profile records from JX7T-14C302-BM.VBF.

This tool does not redistribute Ford firmware.  Point it at a locally supplied
JX7T-14C302-BM VBF and it locates the consecutive Information/Soft/Hard chime
records by their firmware signatures, writes the raw 0x11c-byte records, and
emits a CSV/JSON summary of the fields that can be established from the binary.

The three record signatures are especially strong because their 1000/500/250 ms
values match Ford's documented Information / Soft-warning / Hard-warning tone
lengths exactly.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import struct
from pathlib import Path

STRIDE = 0x11C
CONTROL_RATE_HZ = 3000
AUDIO_RATE_HZ = 48000
Q23 = float(1 << 23)
Q11 = float(1 << 11)

PROFILES = (
    (3, "information", 1000),
    (4, "soft_warning", 500),
    (5, "hard_warning", 250),
)


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def find_profile(data: bytes, profile_id: int, duration_ms: int) -> int:
    sig = struct.pack("<I", 0x00010000 | profile_id) + (b"\x00" * 8) + struct.pack("<I", duration_ms)
    hits = []
    start = 0
    while True:
        pos = data.find(sig, start)
        if pos < 0:
            break
        hits.append(pos)
        start = pos + 1
    if len(hits) != 1:
        raise RuntimeError(f"profile {profile_id}: expected one signature, found {len(hits)}")
    return hits[0]


def cos_q11_to_hz(value: int, sample_rate: int = AUDIO_RATE_HZ) -> float:
    c = max(-1.0, min(1.0, value / Q11))
    return math.acos(c) * sample_rate / (2.0 * math.pi)


def sincos_to_hz(sin_q13: int, cos_q11: int, sample_rate: int = AUDIO_RATE_HZ) -> float:
    # The paired values in the ACM profile are extremely close to a unit-circle
    # sin/cos pair when interpreted as Q13 and Q11 respectively.  atan2() uses
    # both quantized terms and is therefore the best current oscillator estimate.
    s = sin_q13 / float(1 << 13)
    c = cos_q11 / Q11
    return math.atan2(s, c) * sample_rate / (2.0 * math.pi)


def decode(data: bytes, off: int, name: str, expected_ms: int) -> dict:
    rec = data[off:off + STRIDE]
    if len(rec) != STRIDE:
        raise RuntimeError("truncated profile record")
    duration_ms = u32(rec, 0x0C)
    alpha_raw = u32(rec, 0x2C)
    seg_a = u32(rec, 0x34)
    seg_b = u32(rec, 0x38)
    control_rate = u32(rec, 0x3C)

    # The common tone block in profiles 3/4/5 starts at +0x58.  The first
    # resonator has Q11 cos(w)=0x07F6 and -1.0=0xF800.  A second repeated
    # Q11 cosine is 0x07EE.  We expose both without claiming the entire SAF3600
    # rendering graph has been reconstructed.
    primary_sin = u16(rec, 0x5C)
    primary_cos = u16(rec, 0x5E)
    minus_one = struct.unpack_from("<h", rec, 0x60)[0]
    secondary_sin = u16(rec, 0x66)
    secondary_cos = u16(rec, 0x68)

    total_ticks = seg_a + seg_b
    alpha = alpha_raw / Q23
    expected_alpha = math.exp(-1.0 / total_ticks) if total_ticks else None

    return {
        "name": name,
        "record_id": u32(rec, 0x00) & 0xFFFF,
        "vbf_file_offset_hex": f"0x{off:08X}",
        "duration_ms": duration_ms,
        "expected_duration_ms": expected_ms,
        "field_0x18": u32(rec, 0x18),
        "q23_0x20": u32(rec, 0x20),
        "q23_0x24": u32(rec, 0x24),
        "q23_0x28": u32(rec, 0x28),
        "envelope_alpha_raw": alpha_raw,
        "envelope_alpha": alpha,
        "envelope_alpha_expected_exp_minus_1_over_N": expected_alpha,
        "segment_a_ticks": seg_a,
        "segment_b_ticks": seg_b,
        "total_ticks": total_ticks,
        "control_rate_hz": control_rate,
        "segment_a_ms": 1000.0 * seg_a / control_rate if control_rate else None,
        "segment_b_ms": 1000.0 * seg_b / control_rate if control_rate else None,
        "primary_q13_sin": primary_sin,
        "primary_q11_cos": primary_cos,
        "primary_frequency_hz_from_sincos_if_48k": sincos_to_hz(primary_sin, primary_cos),
        "primary_frequency_hz_from_cos_only_if_48k": cos_q11_to_hz(primary_cos),
        "oscillator_minus_one_q11": minus_one,
        "secondary_q13_sin": secondary_sin,
        "secondary_q11_cos": secondary_cos,
        "secondary_frequency_hz_from_sincos_if_48k": sincos_to_hz(secondary_sin, secondary_cos),
        "secondary_frequency_hz_from_cos_only_if_48k": cos_q11_to_hz(secondary_cos),
        "record_size": STRIDE,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("vbf", type=Path, help="JX7T-14C302-BM.VBF")
    ap.add_argument("--out", type=Path, default=Path("acm_chime_profiles"))
    args = ap.parse_args()
    data = args.vbf.read_bytes()
    args.out.mkdir(parents=True, exist_ok=True)

    decoded = []
    offsets = []
    for profile_id, name, duration in PROFILES:
        off = find_profile(data, profile_id, duration)
        offsets.append(off)
        decoded.append(decode(data, off, name, duration))
        (args.out / f"profile_{profile_id:02d}_{name}.bin").write_bytes(data[off:off + STRIDE])

    if offsets[1] - offsets[0] != STRIDE or offsets[2] - offsets[1] != STRIDE:
        raise RuntimeError("profile signatures are not consecutive 0x11c-byte records")

    with (args.out / "acm_chime_profiles.json").open("w", encoding="utf-8") as f:
        json.dump(decoded, f, indent=2)

    with (args.out / "acm_chime_profiles.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(decoded[0].keys()))
        w.writeheader()
        w.writerows(decoded)

    for p in decoded:
        print(
            f"{p['name']}: offset={p['vbf_file_offset_hex']} "
            f"duration={p['duration_ms']} ms alpha={p['envelope_alpha']:.9f} "
            f"primary≈{p['primary_frequency_hz_from_sincos_if_48k']:.2f} Hz "
            f"secondary≈{p['secondary_frequency_hz_from_sincos_if_48k']:.2f} Hz"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
