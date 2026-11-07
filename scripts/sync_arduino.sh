#!/bin/zsh
# Copy the shared LVGL UI sources into the Arduino IDE sketch folder.
# The Arduino IDE needs real files (not symlinks) in the sketch directory.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/ui/lvgl"
INC="$ROOT/include"
ASSETS="$ROOT/assets"
DST="$ROOT/arduino/t3-dash"

python3 "$ROOT/scripts/gen_version.py"

UI_CPP=(ui_lvgl.cpp ui_clock.cpp ui_brightness.cpp ui_bye.cpp)
UI_H=(ui_theme.h ui_clock.h ui_brightness.h ui_bye.h)
INC_H=(ui_lvgl.h dash_types.h demo_values.h version.h)
ASSET_FILES=(icons.c icons.h vw_gif.c vw_gif.h bye_gif.c bye_gif.h)

changed=0
for f in $UI_CPP $UI_H; do
  if ! cmp -s "$SRC/$f" "$DST/$f"; then
    cp "$SRC/$f" "$DST/$f"
    echo "synced  src/ui/lvgl/$f"
    changed=1
  fi
done
for f in $INC_H; do
  if ! cmp -s "$INC/$f" "$DST/$f"; then
    cp "$INC/$f" "$DST/$f"
    echo "synced  include/$f"
    changed=1
  fi
done
for f in $ASSET_FILES; do
  if ! cmp -s "$ASSETS/$f" "$DST/$f"; then
    cp "$ASSETS/$f" "$DST/$f"
    echo "synced  assets/$f"
    changed=1
  fi
done

if [ "$changed" -eq 0 ]; then
  echo "Arduino sketch is up to date."
else
  echo "Arduino sketch updated."
fi
