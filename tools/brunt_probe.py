#!/usr/bin/env python3
"""PARKED: needs a driven STM32 RX (pin 22), but the Wi-Fi module drives that line and stays on
the board. Kept only in case the module is ever removed. See CLAUDE.md.

Send frames to the Brunt STM32 and watch its C1 status for changes.

Wiring: adapter TX -> STM32 pin 22 (PB11, USART3_RX), adapter RX -> pin 21 (PB10), GND.
The Wi-Fi module must NOT also be driving pin 22: hold it in reset, lift its TX pad,
or cut that trace first. Two push-pull outputs on one line will fight.

Usage:
  python3 brunt_probe.py PORT watch                 # just print status changes
  python3 brunt_probe.py PORT send C0 0200          # send one frame (type, payload hex)
  python3 brunt_probe.py PORT sweep 32 C2 DF        # send types C2..DF with payload 32, one at a time

Keep a hand near the power: a guessed command may move the blind.
Needs: pip install pyserial --break-system-packages
"""
import sys, time, threading, serial

def crc8_maxim(data):
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0x8C if c & 1 else c >> 1
    return c

def build(typ, payload):
    f = bytes([0xA9, typ, len(payload)]) + payload
    return f + bytes([crc8_maxim(f)])

assert build(0xC0, bytes.fromhex("0200")).hex() == "a9c0020200" + "27"  # matches the module's real frame
assert build(0xE0, bytes.fromhex("42")).hex() == "a9e00142c3"

class Reader(threading.Thread):
    """Parses incoming frames; prints C1 only when it changes, everything else always."""
    def __init__(self, ser):
        super().__init__(daemon=True)
        self.ser, self.last_c1, self.buf = ser, None, bytearray()
    def run(self):
        while True:
            self.buf += self.ser.read(64)
            while True:
                i = self.buf.find(b"\xA9")
                if i < 0: self.buf.clear(); break
                del self.buf[:i]
                if len(self.buf) < 3: break
                end = 3 + self.buf[2] + 1
                if len(self.buf) < end: break
                f = bytes(self.buf[:end]); del self.buf[:end]
                if crc8_maxim(f[:-1]) != f[-1]:
                    print("  bad crc:", f.hex(" ")); continue
                if f[1] == 0xC1:
                    if f[3:-1] != self.last_c1:
                        prev = self.last_c1
                        self.last_c1 = f[3:-1]
                        diff = "" if prev is None else "  changed bytes: " + ",".join(
                            str(k) for k in range(len(prev)) if prev[k] != f[3 + k])
                        print(f"{time.strftime('%H:%M:%S')} C1 {f[3:-1].hex(' ')}{diff}")
                else:
                    print(f"{time.strftime('%H:%M:%S')} RX type={f[1]:02X} payload={f[3:-1].hex(' ')}")

def main():
    port, mode = sys.argv[1], sys.argv[2]
    ser = serial.Serial(port, 9600, timeout=0.05)
    Reader(ser).start()
    time.sleep(1.2)  # catch at least two status frames first
    if mode == "send":
        f = build(int(sys.argv[3], 16), bytes.fromhex(sys.argv[4] if len(sys.argv) > 4 else ""))
        print("TX", f.hex(" ")); ser.write(f); time.sleep(3)
    elif mode == "sweep":
        payload = bytes.fromhex(sys.argv[3])
        for typ in range(int(sys.argv[4], 16), int(sys.argv[5], 16) + 1):
            f = build(typ, payload)
            print("TX", f.hex(" ")); ser.write(f)
            time.sleep(2)  # 4 status frames to see a reaction
    else:
        while True: time.sleep(1)

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: pass
