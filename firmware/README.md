# Prebuilt firmware

`esp-home-v1.02.bin` — v1.02 build for Generic ESP8266 Module (4M flash).

Flash with esptool at `0x00000`:

```sh
esptool.py --port /dev/ttyUSB0 write_flash 0x00000 esp-home-v1.02.bin
```

or via PlatformIO: `pio run -e esp12e -t upload`.

On first boot the board opens the `ESP-Setup-XXXX` AP — see the main README.
After that, updates arrive in one press from `/update` (GitHub release OTA).
SHA-256 (first 16 hex): `d9f7bf0b203a8394` (full hash: run `sha256sum`).
