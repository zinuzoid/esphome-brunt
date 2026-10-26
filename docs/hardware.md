# Hardware notes

Board: `CURTAIN_100_REV1.5`, dated 2018-01-11, 2-layer, 32.8 × 85.0 mm. Photos in `photos/`.

## Main parts

| Ref | Part | Role |
|---|---|---|
| U1 | STM32F103 (LQFP48, C8/CB) + crystal Y1 | Main controller: buttons, LEDs, motor, encoder, limits |
| U5 | Wi-Fi module, label `WM88300` / `KW4-M` (Marvell 88MW300) | Cloud link, now useless. SMD, **leave in place** |
| U6, D1 (SS14), 5R6 inductor | Buck regulator | Logic rail |
| J3 "BLDC" | 5-pin connector | Motor. Probably integrated-driver BLDC (V+, GND, PWM, DIR, FG); not probed |
| J5 | DC barrel jack | Input, voltage not yet measured |
| J2 "DEBUG" | 6-pin header, silk PA11/PA12 | Likely SWD (PA13/PA14) + NRST + 3V3/GND; not verified |
| SW2 "UP", SW3 "DOWN", SW4 "WIFI" | Tact switches | Physical controls; SW2/SW3 are the emulation points |

## Signals found

* STM32 USART3 ↔ module. Continuity beeped on LQFP **pins 21/22**. The convenient tap points are the
  test pads just below the module (S10–S13, S24, S25); the user knows which pad goes to which
  pin.
* PB10 (pin 21) idles at 3.6 V on the scope, 0.2 V low.

## ESP32-C3 wiring (planned)

| ESP32-C3 | Board | Notes |
|---|---|---|
| GPIO4 (UART RX) | Pin-21 test pad | Through 1 kΩ; read-only |
| GPIO5 | SW2 UP signal pad | Open-drain, inverted (pull low = press). Verify polarity first |
| GPIO6 | SW3 DOWN signal pad | Same as GPIO5 |
| GND | Board GND (e.g. DC jack barrel) | Required |
| 3V3/5V | Own buck from DC jack | Don't load the board's 3.3 V rail |

## Other observations

* C10 (top-left of U1 in the close-up) has a dark, possibly cracked end. Worth checking if
  anything misbehaves.
* LQFP48 pin 1 is at the dot corner, bottom-left in `stm32_closeup.jpg`; pins count
  anticlockwise.
