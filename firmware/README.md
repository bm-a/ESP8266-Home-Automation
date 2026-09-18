# Prebuilt firmware

`esp-home-v1.01.bin` — v1.01 build for Generic ESP8266 Module (4M flash).

Flash with esptool at `0x00000`:

```sh
esptool.py --port /dev/ttyUSB0 write_flash 0x00000 esp-home-v1.01.bin
```

or via PlatformIO: `pio run -e esp12e -t upload`.

On first boot the board opens the `ESP-Setup-XXXX` AP — see the main README.
After that, updates arrive in one press from `/update` (GitHub release OTA).
SHA-256 (first 16 hex): `91377996ba2e1bf4` (full hash: run `sha256sum`).
