# Brunt Blind Engine → Home Assistant (ESPHome)

Brunt's cloud is gone, which left the Blind Engine controllable only by its buttons. This revives it
as a local HA cover. An ESP32-C3 listens to the STM32's status UART and presses the physical
UP/DOWN buttons. The original Wi-Fi module stays in place.

* `components/brunt/`: ESPHome external component `brunt` (cover platform)
* `esphome/`: example device YAML
* `docs/protocol.md`: decoded UART protocol (framing, CRC-8/MAXIM, status fields)
* `docs/hardware.md`: board notes and wiring
* `tools/`: UART sniffer, decoder, (parked) frame injector
* `captures/`: raw captures the protocol was decoded from
* `test/`: host simulator tests for the control logic

Quick start:

```
cp esphome/secrets.example.yaml esphome/secrets.yaml   # fill in
make test
make firmware
```

Using the component from another ESPHome config:

```yaml
external_components:
  - source: github://zinuzoid/esphome-brunt
    components: [brunt]
```

See `esphome/brunt-blind.yaml` for a full device config.

See `CLAUDE.md` for status, open questions and the bring-up checklist.

Brunt's own shutdown announcement, with an English translation, is in
[`docs/brunt-shutdown-notice.md`](docs/brunt-shutdown-notice.md).
