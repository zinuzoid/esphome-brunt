# CLAUDE.md

Handover notes for Claude Code. Read this first, then `docs/protocol.md` and `docs/hardware.md`.

## Goal

Bring Jetsada's Brunt Blind Engine (roller-blind motor) back as a **local Home Assistant cover**.
Brunt shut down its cloud, so the stock app and the HA `brunt` integration (cloud API) are dead.
The physical UP/DOWN buttons still work, so the STM32 and motor are healthy.

## Chosen approach (user decision, keep to it)

**Listen-only + button emulation, via an ESPHome external component on an ESP32-C3.**

* The ESP32 taps the STM32's status line (USART3 TX, PB10, LQFP pin 21) **read-only**, in
  parallel with the original Wi-Fi module, which is only an input on that line.
* It moves the blind by **pulsing the physical UP/DOWN button lines** (open-drain GPIOs).
* The original Wi-Fi module (Marvell 88MW300 based, SMD) **stays on the board**. The user does not
  want to desolder or cut anything around it. So **do not plan on driving the STM32's RX
  (pin 22)**: the module's TX drives that line, and a second driver would fight it.
  `tools/brunt_probe.py` assumes injection and is therefore parked. Don't steer back to it unless
  the user asks.

Rejected or parked: reflashing the 88MW300 (no open firmware), replacing the STM32 firmware (not
needed), and the SWD dump (no ST-Link; an Orange Pi 5 via OpenOCD `linuxgpiod` is possible if
deeper reverse engineering is ever needed).

## Status (2026-10-26)

Done:
* Protocol framing, CRC and the status frame are fully decoded (`docs/protocol.md`), and
  verified on 149 + 101 real frames.
* `components/brunt/` is a cover platform: it parses status, publishes position and
  operation, has optional motor-load and encoder sensors, and runs a target-position state machine
  driving button pulses.
* Host tests pass under ASan/UBSan (`make test`). They cover a motor simulator with stock button
  semantics, plus a replay of a real capture with injected junk bytes.
* `esphome config` passes and `esphome compile --only-generate` succeeds on ESPHome **2026.10.0**.
  The component also passes `-fsyntax-only` against the real ESPHome headers (host HAL).

Not done yet:
1. **Full ESP32-C3 firmware build** (`esphome compile esphome/brunt-blind.yaml`). It couldn't run in
   the previous sandbox because the toolchain download was blocked. This is the first thing to do.
2. **Hardware bring-up:** wiring, flashing, and checking against the real blind (checklist below).
3. **Tune `stop_latency`** on hardware.

## Open questions / unverified assumptions

* **Button polarity.** The YAML assumes active-low switches (one pad pulled up to 3.3 V, the other
  GND). The user should measure both pads of each tact switch to GND with the board powered.
  If they're active-high, change the pin config (push-pull, not inverted).
* **Logic level.** The scope showed the PB10 idle level at **3.6 V**. The board's 3.3 V rail
  hasn't been measured. A 1 kΩ series resistor on ESP RX is recommended either way.
* **Pressing the opposite button while moving** hasn't been observed on hardware. The simulator
  assumes it stops the motor. The component avoids it: it always stops with the same button,
  then starts the other way.
* **Status byte 1:** meaning unknown. It was seen as 1 for about 10 s once in capture 03, with no
  motion.
* **Module → STM32 frames** `C0 02 00` and `E0 42`: meaning unknown, and not needed.
* **ESP power.** Plan: a separate buck converter from the DC jack (input voltage not yet
  measured), not the board's 3.3 V rail, because the Marvell module already draws Wi-Fi current
  from it.
* **Physical stop coast.** The coast after a stop press is small but hasn't been measured. It's
  absorbed by `stop_latency` (default 100 ms).

## Hardware bring-up checklist (for the user)

1. Measure the board 3.3 V rail, the DC input voltage, and the button pad voltages.
2. Wire: GPIO4 → 1 kΩ → pin-21 test pad; GPIO5/GPIO6 → UP/DOWN switch signal pads; common GND.
   The test pad that beeps to pin 21 was found by continuity. It's one of S10–S13/S24/S25 under the
   module, and the user knows which.
3. Flash and watch the logs. Status should publish every 500 ms. Press the physical buttons and
   confirm HA follows.
4. From HA: go to 50 %, then 100 %, 0 %, 30 %, and stop mid-move. If the blind ends past the target,
   raise `stop_latency`; if short, lower it.

## Commands

```
make test      # build and run the host simulator tests (g++, ASan/UBSan)
make config    # esphome config (needs `pip install esphome` and esphome/secrets.yaml)
make firmware  # esphome compile for the ESP32-C3
python3 tools/brunt_decode.py captures/04_stm32_status_pin21_up_down_sequence.log
```

`esphome/secrets.yaml` is gitignored; copy it from `secrets.example.yaml`.

## Code map

* `components/brunt/brunt_cover.{h,cpp}`: the component. Key invariant: **never press a
  button again until the previous press is visible in the status** (`awaiting_` / `expect_`),
  because a second press of the running direction toggles the motor off or on. Presses time out
  after 1.3 s, with at most 3 start attempts. The move is cancelled if the motor stops without us
  asking (a limit, or someone pressed a physical button).
* `components/brunt/cover.py`: config schema and codegen. It handles both `cover_schema`
  and the older `COVER_SCHEMA`.
* `test/sim.cpp` + `test/mock/`: mock ESPHome headers, a motor simulator and scenarios. Extend this
  when changing control logic, and keep all scenarios passing.
* `tools/sniff.py`: passive UART logger (pyserial), producing timestamped hex frames split on
  5 ms gaps.
* `tools/brunt_decode.py`: decodes scope CSVs (soft UART) or sniff logs, checks CRC, prints C1
  fields.
* `tools/brunt_probe.py`: frame injector. **Parked** (see above).
* `captures/`: raw evidence; see `captures/README.md`. Capture file names were relabelled: the
  original names had TX/RX swapped.

## Conventions

* The user is an experienced C++/embedded engineer. Be direct, skip basics.
* Git: rebase-merge PRs, delete the branch after merge.
