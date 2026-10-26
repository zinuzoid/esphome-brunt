# STM32 ↔ Wi-Fi module protocol

Link: STM32F103 USART3 ↔ Marvell 88MW300 module. **9600 baud, 8N1**, 3.3 V logic (idle measured
3.6 V on the scope). Bytes arrive about 1.15 ms apart, which is roughly a one-bit gap.

| STM32 pin | Signal | Direction |
|---|---|---|
| 21 (PB10) | USART3_TX | STM32 → module (status) |
| 22 (PB11) | USART3_RX | module → STM32 (commands) |

## Framing

```
A9 | type | len | payload[len] | crc
```

`crc` = **CRC-8/MAXIM** (Dallas 1-Wire: poly 0x31 reflected, i.e. 0x8C, init 0x00, no final XOR)
computed over every byte before it, including `A9`. It was confirmed on all 250 captured frames,
in both directions. CRC-8/WCDMA matched the very first frame by coincidence and failed on the rest.

```python
def crc8_maxim(data):
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0x8C if c & 1 else c >> 1
    return c
```

## `C1`: status, STM32 → module, every ~500 ms (495–506 ms jitter), len 28

Multi-byte fields are **big-endian**. Offsets are into the payload.

| Off | Example | Meaning | Confidence |
|---|---|---|---|
| 0 | `19` | Position %, `floor((enc − bottom) × 100 / (top − bottom))`. 0 = bottom/closed, 100 = top/open | confirmed |
| 1 | `00` | Unknown. Seen `01` for about 10 s once, with no motion | unknown |
| 2 | `01` | Motor: 0 idle, 1 moving up (opening), 2 moving down (closing) | confirmed |
| 3–4 | `16 20` | Constant | – |
| 5–6 | `00 04` | Motor load: about 4 idle, 800–3200 moving. Likely current ADC | likely |
| 7–10 | `00 00 0C 9F` | enc − bottom limit | confirmed |
| 11 | `01` | Last command: 1 up, 2 down. Set to 0 by a button stop; kept after a limit stop | confirmed |
| 12 | `01` | One-frame flag on a button press (start or stop); not set by a limit stop | confirmed |
| 13 | `04` | Constant | – |
| 14–17 | `00 00 26 8A` | Bottom limit, encoder (9866 on this unit) | confirmed |
| 18–21 | `00 00 58 B3` | Top limit, encoder (22707 on this unit) | confirmed |
| 22–25 | `00 00 33 29` | Encoder position | confirmed |
| 26–27 | `00 00` | Constant | – |

Worked check: enc 11808, bottom 9866, top 22707 gives rel 1942 (= bytes 7–10) and 15.1 %, so
byte 0 = 15.

Travel: about 485 counts per frame up and about 467 per frame down, i.e. about 970 and 935
counts/s. Full span 12841 counts ≈ 13.5 s.

The firmware stops at the limits by itself. A button press while idle starts that direction;
pressing the same button again while moving stops it.

## Module → STM32 (seen once each, at module start-up)

| Frame | Payload | Notes |
|---|---|---|
| `A9 C0 02 02 00 27` | `02 00` | Unknown |
| `A9 E0 01 42 C3` | `42` | Unknown |

The module is otherwise silent, since its cloud is gone. No move command has been identified. The
chosen approach doesn't need one.
