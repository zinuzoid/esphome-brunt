#!/usr/bin/env python3
"""Passive UART sniffer: prints timestamped hex frames, split on idle gaps.

Usage:  python3 sniff.py /dev/ttyUSB0 115200 stm32_tx
        (label is just used for the log file name)

Wire ONLY adapter RX + GND to the line you're listening to. Never connect TX.
Needs: pip install pyserial --break-system-packages
"""
import sys, time, serial

port, baud = sys.argv[1], int(sys.argv[2])
label = sys.argv[3] if len(sys.argv) > 3 else "uart"
GAP_S = 0.005  # bytes separated by >5 ms start a new frame

ser = serial.Serial(port, baud, timeout=GAP_S)
log = open(f"{label}_{baud}_{time.strftime('%Y%m%d_%H%M%S')}.log", "w")
t0 = time.monotonic()
buf = bytearray()

def flush():
    if not buf:
        return
    ts = time.monotonic() - t0
    hx = " ".join(f"{b:02X}" for b in buf)
    asc = "".join(chr(b) if 32 <= b < 127 else "." for b in buf)
    line = f"{ts:9.3f}s [{len(buf):3d}] {hx}  |{asc}|"
    print(line); log.write(line + "\n"); log.flush()
    buf.clear()

print(f"Listening on {port} @ {baud}. Ctrl+C to stop. Type notes into the log via a 2nd terminal if useful.")
try:
    while True:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
        else:
            flush()
except KeyboardInterrupt:
    flush()
    log.close()
