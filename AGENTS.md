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
   - `HANDOFF.md` в `.gitignore` — это локальный контекст, в git не коммитится.

3. **Обновить сборки в `dist/`**
   - Собрать свежий merged bin (прошивка по адресу `0x0`):
     ```bash
     cd waveshare
     ~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
     cd ..
     B=waveshare/.pio/build/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
     ~/.platformio-venv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
       --chip esp32s3 merge_bin \
       -o dist/t3-dash-waveshare-7-16MB.bin \
       --flash_mode qio --flash_freq 80m --flash_size 16MB \
       0x0 "$B/bootloader.bin" \
       0x8000 "$B/partitions.bin" \
       0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
       0x10000 "$B/firmware.bin"
     ```
   - Пересобрать архив Arduino-скетча:
     ```bash
     rm -f dist/t3-dash-arduino-sketch.zip
     (cd arduino && zip -r -X ../dist/t3-dash-arduino-sketch.zip t3-dash -x '*.DS_Store')
     ```
   - В ответе указать SHA-256 обоих файлов:
     ```bash
     shasum -a 256 dist/t3-dash-waveshare-7-16MB.bin dist/t3-dash-arduino-sketch.zip
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

## Примечания

- Для чисто документальных правок (без кода/UI/прошивки) пункты 3–4 можно
  пропустить; `HANDOFF.md` обновлять всегда.
- Не запускать два `pio`-билда одновременно.
- Перед коммитом загружать скил `git-conventions` и следовать ему.
- Секреты, токены и локальные пути не коммитить.
