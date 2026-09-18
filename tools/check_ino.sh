#!/bin/sh
# Compile-check the GENERATED Arduino sketch in a scratch PIO project.
# (src_dir is project-wide in PlatformIO, so this can't live in platformio.ini.)
set -e
PROJ="$(dirname "$(readlink -f "$0")")/.."
INO="$PROJ/arduino/ESP8266_Home_Automation"
SCRATCH="${TMPDIR:-/tmp}/ha_ino_check"
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH/src"
cp "$INO/ESP8266_Home_Automation.ino" "$SCRATCH/src/sketch.ino"
cat > "$SCRATCH/platformio.ini" <<'EOF'
[env:ino_check]
platform = espressif8266
board = esp12e
framework = arduino
lib_deps =
  tzapu/WiFiManager@^2.0.17
  arduino-libraries/NTPClient@^3.2.1
  paulstoffregen/Time@^1.6.1
EOF
cd "$SCRATCH"
pio run -e ino_check 2>&1 | grep -E "error|Error|SUCCESS|FAILED|RAM:|Flash:" | head -20
