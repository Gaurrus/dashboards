# AGENTS.md — правила работы с репозиторием

## Обязательный чек-лист после доработки

После изменений кода, прошивки или UI:

1. **Проверить корректность**
   - Собрать симулятор:
     ```bash
     cd sim && cmake --build build -j8
     ```
   - Собрать прошивку Waveshare:
     ```bash
     cd waveshare && ~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
     ```
   - Если менялись общие UI-исходники — синхронизировать Arduino-скетч:
     ```bash
     ./scripts/sync_arduino.sh
     ```
   - Прогнать профильные режимы симулятора и глазами проверить результат
     (например, `sweep`, `hot`, `low`, `dim`, `bye`, `clock`, `dialpick`).
   - Убедиться, что нет ошибок сборки и регрессий в скриншотах.

2. **Обновить `HANDOFF.md`**
   - Добавить раздел `## Обновление YYYY-MM-DD: <кратко>`.
   - Записать: что изменено, какие файлы/пины/адреса затронуты, какие команды
     проверки выполнены, что осталось проверить на железе.
   - Указать версию: `Версия: old → new`.
   - `HANDOFF.md` в `.gitignore` — это локальный контекст, в git не коммитится.

3. **Обновить сборки в `dist/`**
   - **Версия обязательна в имени файлов сборок.** Источник — `VERSION`
     в корне репозитория.
     ```bash
     VER=$(cat VERSION)
     ```
   - Собрать свежий merged bin (прошивка по адресу `0x0`):
     ```bash
     cd waveshare
     ~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
     cd ..
     VER=$(cat VERSION)
     B=waveshare/.pio/build/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
     ~/.platformio-venv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
       --chip esp32s3 merge_bin \
       -o "dist/t3-dash-waveshare-7-16MB-v${VER}.bin" \
       --flash_mode qio --flash_freq 80m --flash_size 16MB \
       0x0 "$B/bootloader.bin" \
       0x8000 "$B/partitions.bin" \
       0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
       0x10000 "$B/firmware.bin"
     ```
   - Пересобрать архив Arduino-скетча:
     ```bash
     VER=$(cat VERSION)
     rm -f "dist/t3-dash-arduino-sketch-v${VER}.zip"
     (cd arduino && zip -r -X "../dist/t3-dash-arduino-sketch-v${VER}.zip" t3-dash -x '*.DS_Store')
     ```
   - Удалить/не создавать безверсионные копии в `dist/`.
   - Сформировать манифест релиза:
     ```bash
     python3 scripts/make_dist_manifest.py
     ```
     Получится `dist/t3-dash-v${VER}.json` с версией, git-коммитом,
     SHA-256 и адресом прошивки.
   - В ответе указать SHA-256 обоих файлов:
     ```bash
     VER=$(cat VERSION)
     shasum -a 256 "dist/t3-dash-waveshare-7-16MB-v${VER}.bin" \
                   "dist/t3-dash-arduino-sketch-v${VER}.zip"
     ```

4. **Сделать превью-скриншот self-check**
   ```bash
   cd sim
   ./build/t3dash_sim /tmp/selfcheck.bmp sweep
   sips -s format png /tmp/selfcheck.bmp --out preview-selfcheck.png
   ```
   - Файл: `sim/preview-selfcheck.png` (игнорируется правилом `sim/preview*.png`).
   - Для максимального self-test использовать режим `hot`:
     `./build/t3dash_sim /tmp/selfcheck.bin hot`.

## Версионирование

Единый источник версии — файл `VERSION` в корне репозитория. Формат:
`MAJOR.MINOR.PATCH`, без префикса `v`.

- **MAJOR** — несовместимые изменения: распиновка, протокол, формат NVS,
  API UI, смена платы/фреймворка, несовместимое поведение.
- **MINOR** — новая обратно совместимая функциональность: экран, датчик,
  режим, настройка.
- **PATCH** — исправления, производительность, рефакторинг, документация,
  мелкие правки без нового поведения.

Правила:

- Версию поднимать после каждой завершённой доработки. Если задача разбита
  на несколько коммитов — один bump в последнем коммите задачи.
- Обновлять `VERSION` в том же коммите, что и доработка.
- После изменения `VERSION` запустить `python3 scripts/gen_version.py`
  (обновляет `include/version.h` и копию для Arduino); PlatformIO-сборка
  Waveshare делает это автоматически через `extra_scripts`.
- Версия попадает в прошивку: Serial при старте печатает
  `VW T3 dashboard v<VER>...`; `t3dash_sim --version` показывает версию
  симулятора.
- Для сборок в `dist/` версия обязательна в имени файла:
  `...-vMAJOR.MINOR.PATCH.bin` / `...-vMAJOR.MINOR.PATCH.zip`;
  манифест — `dist/t3-dash-vMAJOR.MINOR.PATCH.json`.
- Релизный коммит помечать аннотированным тегом `vMAJOR.MINOR.PATCH`.
- В `HANDOFF.md` фиксировать `Версия: old → new`.
- Не понижать версию и не пропускать патч.
- Сомневаешься между MINOR и PATCH — выбирай PATCH, если нет нового
  пользовательского поведения.
- Сомневаешься между MAJOR и MINOR — выбирай MAJOR, если новая прошивка
  несовместима с прежним железом/сохранёнными данными.

## Примечания

- Для чисто документальных правок (без кода/UI/прошивки) пункты 3–4 можно
  пропустить; `HANDOFF.md` обновлять всегда, версия — патч.
- Не запускать два `pio`-билда одновременно.
- Перед коммитом загружать скил `git-conventions` и следовать ему.
- Секреты, токены и локальные пути не коммитить.
