#!/bin/zsh
set -e
cd "$(dirname "$0")"

mkdir -p third_party
if [ ! -d third_party/lvgl ]; then
  echo "Cloning LVGL v8.4.0..."
  git clone --depth 1 --branch v8.4.0 https://github.com/lvgl/lvgl.git third_party/lvgl
fi
