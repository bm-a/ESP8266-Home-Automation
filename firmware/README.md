# Prebuilt firmware

`esp-home-v1.0.bin` — v1.0 build for Generic ESP8266 Module (4M flash).

Flash with esptool at `0x00000`:

```sh
esptool.py --port /dev/ttyUSB0 write_flash 0x00000 esp-home-v1.0.bin
```

or via PlatformIO: `pio run -e esp12e -t upload`.

On first boot the board opens the `ESP-Setup-XXXX` AP — see the main README.
SHA-256 (first 16 hex): `db437ca497a53d39` (full hash: run `sha256sum`).
