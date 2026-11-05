#!/bin/zsh
set -e
cd "$(dirname "$0")"

if [ ! -d third_party/lvgl ]; then
  echo "[0/3] Fetch LVGL 8.4.0..."
  ./fetch_lvgl.sh
fi

echo "[1/3] Configure..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
echo "[2/3] Build..."
cmake --build build -j8
echo "[3/3] Run (close window or Esc to exit)"
exec ./build/t3dash_sim "$@"
