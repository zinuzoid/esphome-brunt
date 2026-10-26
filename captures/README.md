# Captures

All at 9600 8N1. Logs are `tools/sniff.py` output (timestamp, byte count, hex, ASCII). Files were
renamed: the user's original names for 02/03 had TX and RX swapped, and the contents decide.

| File | Line | What happened |
|---|---|---|
| `01_scope_pin21_boot.csv` | Pin 21 (STM32 TX), scope CH1 | 0.4 s at 50 kS/s; one `C1` frame. CH2 was unconnected. Columns after the voltages use `,` as a thousands separator, so parse only the first 3 fields |
| `02_module_to_stm32_pin22.log` | Pin 22 (module → STM32) | Only `C0 02 00` and `E0 42`, around module start-up. Original name `stm32_tx_9600_20261025_184541.log` |
| `03_stm32_status_pin21_bench.log` | Pin 21 | ~75 s of status with a few button presses; only one 43-count move (motor likely loaded oddly or barely engaged). Shows byte 1 = 1 for ~10 s. Original name `stm32_rx_9600_20261025_184630.log` |
| `04_stm32_status_pin21_up_down_sequence.log` | Pin 21 | Annotated sequence below. Original name `stm32_tx_9600_20261025_185721.log` |

## Capture 04 action sequence (user notes)

1. UP (motor starts) → 1 s → UP again (stops)
2. 1 s → DOWN (starts) → 1 s → DOWN again (stops)
3. 1 s → UP, ran to the top limit by itself
4. DOWN, ran to the bottom limit by itself
5. Idle a few seconds

Mapped in the log: UP at 9.6 s, stop 11.1 s; DOWN 13.1 s, stop 14.1 s; UP 16.1 s → top 26.1 s
(100 %); DOWN 28.6 s → bottom 42.6 s (0 %).
