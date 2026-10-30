#!/usr/bin/env bash
# Собирает прошивку и готовит сайт для установки из браузера (ESP Web Tools) в dist/web:
#   index.html, manifest.json и части прошивки по отдельности (загрузчик, таблица
#   разделов, boot_app0, приложение). Манифест пишет их по своим адресам, поэтому
#   область NVS с паролем Wi-Fi (0x9000) и раздел настроек при переустановке не трогаются.
# Плюс для релиза:
#   bambu-matrix.bin     — всё одним файлом с адреса 0x0, для esptool (стирает и Wi-Fi);
#   bambu-matrix-ota.bin — только приложение, для обновления по Wi-Fi.
# Используется локально и в GitHub Actions. Версия — первый аргумент или git describe.
set -euo pipefail

cd "$(dirname "$0")/.."
VERSION="${1:-$(git describe --tags --always 2>/dev/null || echo dev)}"
PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null || PIO="$HOME/.platformio/penv/bin/pio"

"$PIO" run -e esp32

BUILD=.pio/build/esp32
PKG="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages"
BOOT_APP0="$PKG/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
PYTHON="${PYTHON:-python3}"

# esptool: тот, что поставила платформа ESP32 (v5, бинарник), иначе python-модуль
if [ -x "$PKG/tool-esptool/esptool" ]; then
  ESPTOOL=("$PKG/tool-esptool/esptool")
elif "$PYTHON" -m esptool version >/dev/null 2>&1; then
  ESPTOOL=("$PYTHON" -m esptool)
else
  echo "Не найден esptool: pip install esptool" >&2
  exit 1
fi

OUT=dist/web
rm -rf "$OUT"
mkdir -p "$OUT"

# Части для классического ESP32: загрузчик 0x1000, разделы 0x8000, boot_app0 0xe000, прошивка 0x10000
cp "$BUILD/bootloader.bin" "$OUT/bootloader.bin"
cp "$BUILD/partitions.bin" "$OUT/partitions.bin"
cp "$BOOT_APP0" "$OUT/boot_app0.bin"
cp "$BUILD/firmware.bin" "$OUT/bambu-matrix-ota.bin"

# Всё одним файлом — для «esptool write-flash 0x0 bambu-matrix.bin»
PARTS=(0x1000 "$OUT/bootloader.bin" 0x8000 "$OUT/partitions.bin"
       0xe000 "$OUT/boot_app0.bin" 0x10000 "$OUT/bambu-matrix-ota.bin")
# В esptool v5 команда merge-bin, в v4 — merge_bin
"${ESPTOOL[@]}" --chip esp32 merge-bin -o "$OUT/bambu-matrix.bin" "${PARTS[@]}" ||
  "${ESPTOOL[@]}" --chip esp32 merge_bin -o "$OUT/bambu-matrix.bin" "${PARTS[@]}"

cp web-install/index.html "$OUT/index.html"
cat > "$OUT/manifest.json" <<EOF
{
  "name": "Bambu Matrix",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "bambu-matrix-ota.bin", "offset": 65536 }
      ]
    }
  ]
}
EOF
sed -i.bak "s/__VERSION__/$VERSION/g" "$OUT/index.html" && rm "$OUT/index.html.bak"

echo "Готово: $OUT (версия $VERSION)"
ls -la "$OUT"
