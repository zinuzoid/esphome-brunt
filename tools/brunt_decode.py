#!/usr/bin/env python3
"""Decode Brunt Blind Engine STM32 <-> Wi-Fi module UART frames.

Accepts either:
  * a scope CSV export (first two columns: time in s, voltage) -> soft-UART decode at 9600 8N1
  * a sniff.py log (lines containing hex bytes)

Frame format (confirmed over 149 frames, both directions):
  A9 | type | len | payload[len] | crc8
  crc8 = CRC-8/MAXIM (poly 0x31 reflected, init 0) over every byte before it.
  WCDMA is still checked only as a sanity control; it matched once by chance.

Usage:
  python3 brunt_decode.py capture.csv [--ch 1] [--baud 9600] [--thresh 1.8]
  python3 brunt_decode.py stm32_tx_9600_xxx.log
"""
import argparse, re, sys

def _rev(x):
    return int(f"{x:08b}"[::-1], 2)

def crc8(data, poly, init, refl):
    c = init
    for b in data:
        c ^= _rev(b) if refl else b
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return _rev(c) if refl else c

CRCS = {"MAXIM": (0x31, 0x00, True), "WCDMA": (0x9B, 0x00, True)}

def soft_uart(path, ch, baud, thresh):
    t, v = [], []
    with open(path) as fh:
        next(fh)  # header
        for line in fh:
            p = line.split(",")  # scope exports use ',' as thousands sep in later cols; only first cols needed
            try:
                t.append(float(p[0])); v.append(float(p[ch]))
            except (ValueError, IndexError):
                continue
    dt = t[1] - t[0]
    hi = [x > thresh for x in v]
    bt = 1.0 / baud
    if bt / dt < 3:
        print(f"warning: only {bt/dt:.1f} samples per bit, decode may be unreliable", file=sys.stderr)
    out, i, n = [], 0, len(hi)
    sample = lambda st, k: hi[min(int(round((st + (k + 0.5) * bt - t[0]) / dt)), n - 1)]
    while i < n - 1:
        if hi[i] and not hi[i + 1]:
            st = t[i + 1]
            if not sample(st, 0):
                val = sum(1 << b for b in range(8) if sample(st, 1 + b))
                if not sample(st, 9):
                    print(f"framing error at {st:.5f}s", file=sys.stderr)
                out.append((st, val))
                i = int((st + 9.5 * bt - t[0]) / dt)
                continue
        i += 1
    return out

def from_log(path):
    out = []
    for line in open(path):
        m = re.match(r"\s*([\d.]+)s\s+\[\s*\d+\]\s+((?:[0-9A-F]{2}\s?)+)", line)
        if m:
            ts = float(m.group(1))
            out += [(ts, int(h, 16)) for h in m.group(2).split()]
    return out

def be32(b, i):
    return int.from_bytes(b[i:i + 4], "big")

def describe_c1(p):
    """C1 = STM32 status, sent every 500 ms. Field meanings are provisional."""
    pos, lim_a, lim_b, rel = be32(p, 22), be32(p, 14), be32(p, 18), be32(p, 7)
    span = lim_b - lim_a
    calc = (pos - lim_a) * 100 // span if span else -1
    return (f"pct={p[0]} pos={pos} limA={lim_a} limB={lim_b} rel={rel} "
            f"(calc pct {calc}) p1={p[1]} p2={p[2]} p11={p[11]} p12={p[12]}")

def frames(stream):
    b = [v for _, v in stream]
    ts = [s for s, _ in stream]
    i = 0
    while i < len(b):
        if b[i] != 0xA9 or i + 3 > len(b):
            i += 1; continue
        ln = b[i + 2]
        end = i + 3 + ln + 1
        if end > len(b):
            print(f"truncated frame at {ts[i]:.4f}s", file=sys.stderr); break
        f = bytes(b[i:end])
        ok = [n for n, p in CRCS.items() if crc8(f[:-1], *p) == f[-1]]
        yield ts[i], f, ok
        i = end

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--ch", type=int, default=1, help="CSV column of the channel (1 = CH1)")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--thresh", type=float, default=1.8)
    a = ap.parse_args()
    stream = soft_uart(a.path, a.ch, a.baud, a.thresh) if a.path.lower().endswith(".csv") else from_log(a.path)
    print(f"{len(stream)} bytes")
    tally = {n: 0 for n in CRCS}
    nf = 0
    for ts, f, ok in frames(stream):
        nf += 1
        for n in ok: tally[n] += 1
        typ, ln, pl = f[1], f[2], f[3:-1]
        print(f"{ts:9.4f}s type={typ:02X} len={ln:3d} crc={f[-1]:02X} [{','.join(ok) or 'NO MATCH'}]")
        print("    payload:", pl.hex(" ").upper())
        if typ == 0xC1 and ln == 28:
            print("    " + describe_c1(pl))
    if nf:
        print("CRC matches across frames:", tally)

if __name__ == "__main__":
    main()
