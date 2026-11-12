# VW T3 Dashboard

Приборная панель для **VW Transporter T3 (1.6 D/TD)** на
**Waveshare ESP32-S3-Touch-LCD-7**: 7" 800×480 IPS, ESP32-S3N16R8
(16 МБ flash, 8 МБ PSRAM), тач GT911, RGB565.

UI — **LVGL 8.4.0**. В репозитории есть PC-симулятор 800×480 на SDL2,
прошивка Waveshare и скетч для Arduino IDE.

Текущая версия: **1.3.0** (`VERSION`).

## Возможности

- Boot-заставка VW, self-test ламп и шкал, переход к приборке.
- Аналоговые часы + цифровое время; настройка времени тапом, 00:00–23:59.
- Спидометр 0–180 км/ч, тахометр 0–5500 об/мин с красной зоной.
- Четыре дизайна приборки, переключение кнопкой-шестерёнкой (список
  `DESIGN`): CLASSIC — часы и круглые шкалы; CLASSIC+ — та же раскладка с
  сегментными заливками; COMPACT — крупная курсивная скорость с градиентом,
  цифровые часы/пробег и вложенные четверть-круговые шкалы справа; WELLS —
  два белых колодца со стрелками на всю ширину экрана с отступами 16 px,
  лампы внутри, TEMP/FUEL вертикально по центру, серые панели с пробегом и
  часами. Удержание по скорости — общий
  `ARC COLOR`.
- TEMP/FUEL с градиентными шкалами и предупреждениями.
- Вольтметр, одометр, индикаторы: масло, заряд, свечи, ближний/дальний,
  поворотники, ручник, неисправность тормозов, ПТФ.
- Свайп по правому краю — программная яркость (подсветка Waveshare только вкл/выкл).
- Бай-бай экран при выключении зажигания.
- Удержание в центре спидометра — выбор цвета дуги (`ARC COLOR`).
- Версия прошивки мелким шрифтом на экране приветствия (`v1.3.0`).
- Демо-режим на Waveshare; реальные датчики подключаются по схеме ниже.

## Железо

Основная плата:

- **Waveshare ESP32-S3-Touch-LCD-7** — 800×480, GT911, CH422G, 16 МБ flash,
  8 МБ OPI PSRAM. Питание 5 В (USB-C), логика 3.3 В.

Датчики и обвязка (подробности — `schematic.svg` / `schematic.png`):

| Позиция | Модуль | Назначение |
|---|---|---|
| U2 | ADS1115 (Adafruit 1085 / CJMCU) | АКБ, температура, топливо, RPM F/V |
| U3 | MCP23017-E/SP (DIP-28) или модуль | входы оптронов, зуммер |
| OPT1 | 16-канальный PC817 (12 В → 3.3 В, open-collector) | лампы, поворотники, зажигание, тормоза |
| U4 | LM2917N-8 | W-терминал генератора → 0..3.3 В |
| U5 | LM1815N | индуктивный ABS-датчик → импульсы |
| U6 | DC-DC 12 В → 5 В 3 А (MP1584EN) | питание платы |
| J4 | NEO-6M | GPS/время, опционально |
| — | F1 2 А, D1 1N5822, D2 SMBJ15A | защита питания |

Ключевые линии:

- I²C: `GPIO8` SDA, `GPIO9` SCL. Адреса: GT911 `0x5D/0x14`,
  CH422G `0x24`, ADS1115 `0x48`, MCP23017 `0x21`.
- Sensor header: `GPIO6` (ADC/PCNT) — импульсы ABS.
- UART2: `GPIO43` TX, `GPIO44` RX (делится с USB-UART).
- Оптроны: oil/charge/glow/ручник/brake fault — low-side;
  ближний/дальний/поворотники/зажигание — high-side.
- 12 В входы — только через предохранитель и оптроны. I²C level на Waveshare —
  **3.3 В**, не 5 В.

Схема: `schematic.svg` (вектор), `schematic.png` (растр), netlist —
`wiring.json`, генератор — `scripts/draw_schematic.py`.

## Структура

```
include/                 общие типы, конфиг, demo-данные, version.h
src/
  hal/                   hal.h, hal_sim.cpp, hal_real.cpp
  logic/                 model.cpp, calibration.cpp
  ui/lvgl/               UI-модули LVGL: ui_lvgl, ui_design_b, ui_design_d,
                         ui_clock,
                         ui_brightness, ui_bye
  ui/dashboard.cpp       старый TFT-UI (legacy)
assets/                  иконки, GIF, C-массивы
sim/                     PC-симулятор LVGL + SDL2
waveshare/               PlatformIO-проект Waveshare 7"
arduino/t3-dash/         скетч Arduino IDE
scripts/                 sync_arduino, gen_version, make_dist_manifest, draw_schematic
dist/                    локальные сборки (в git не хранятся)
```

## Сборка и запуск

### Симулятор

```bash
cd sim
./run.sh                 # собрать и запустить интерактивно
```

Скриншоты без окна:

```bash
cd sim
./build/t3dash_sim /tmp/dash.bmp sdl      # обычная приборка
./build/t3dash_sim /tmp/boot.bmp boot     # заставка VW
./build/t3dash_sim /tmp/sweep.bmp sweep   # self-test посередине
./build/t3dash_sim /tmp/hot.bmp hot       # self-test на максимуме
./build/t3dash_sim /tmp/low.bmp low       # дальний свет
./build/t3dash_sim /tmp/dim.bmp dim       # яркость 40%
./build/t3dash_sim /tmp/bye.bmp bye       # бай-бай экран
./build/t3dash_sim /tmp/clock.bmp clock   # время 10:08
./build/t3dash_sim /tmp/dial.bmp dialpick # меню ARC COLOR
./build/t3dash_sim /tmp/designlist.bmp designlist # список дизайнов
./build/t3dash_sim /tmp/design2.bmp design2 # второй дизайн
./build/t3dash_sim /tmp/design2color.bmp design2color # смена цвета скорости
./build/t3dash_sim /tmp/design2clock.bmp design2clock # SET TIME из COMPACT
./build/t3dash_sim /tmp/design3.bmp design3 # CLASSIC+ (сегментная классика)
./build/t3dash_sim /tmp/design3color.bmp design3color # смена цвета в CLASSIC+
./build/t3dash_sim /tmp/design4.bmp design4 # WELLS (два колодца)
./build/t3dash_sim /tmp/design4clock.bmp design4clock # SET TIME из WELLS
```

Конвертация BMP → PNG:

```bash
sips -s format png /tmp/dash.bmp --out /tmp/dash.png
```

Версия симулятора:

```bash
./build/t3dash_sim --version
```

Видео-режимы:

```bash
./build/t3dash_sim /tmp/frames video_all 28 30
ffmpeg -y -framerate 30 -i /tmp/frames/frame_%04d.bmp \
  -c:v libx264 -preset medium -crf 20 -pix_fmt yuv420p \
  -movflags +faststart sim/preview-all.mp4
```

### Прошивка Waveshare

```bash
cd waveshare
~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7 -t upload
```

Готовый merged bin (шить по адресу `0x0`) лежит в `dist/vX.Y.Z/` с версией в имени.

### Arduino IDE

Скетч: `arduino/t3-dash/t3-dash.ino`.

- Плата: **ESP32S3 Dev Module**, Flash 16MB, PSRAM OPI, partition 16MB.
- Библиотеки: `ESP32_Display_Panel` (GitHub master), `ESP32_IO_Expander`,
  `esp-lib-utils`, `LVGL 8.4.0`.
- Общие UI-исходники синхронизируются:

```bash
./scripts/sync_arduino.sh
```

### Legacy-прототип (ESP32 DevKit)

Wokwi и связанные с ним файлы удалены. Остался старый TFT-UI для
ESP32 DevKit — он не используется Waveshare/Arduino/sim:

```bash
~/.platformio-venv/bin/pio run -e hardware
```

## Версионирование и релизы

Единый источник версии — `VERSION` (`MAJOR.MINOR.PATCH`).

- После изменения `VERSION`:

```bash
python3 scripts/gen_version.py
```

Обновляет `include/version.h`, копию для Arduino и симлинк Waveshare.
PlatformIO Waveshare делает это автоматически (`extra_scripts`), CMake
симулятора — через custom command.

- Версия печатается в Serial при старте и доступна в `t3dash_sim --version`.
- Каждая версия — отдельная папка `dist/vX.Y.Z/`; версия также остаётся
  в именах файлов:
  `dist/v1.3.0/t3-dash-waveshare-7-16MB-v1.3.0.bin`,
  `dist/v1.3.0/t3-dash-arduino-sketch-v1.3.0.zip`.
- Манифест релиза:

```bash
python3 scripts/make_dist_manifest.py
# dist/v1.3.0/t3-dash-v1.3.0.json
```

- Релизный коммит помечается тегом `vMAJOR.MINOR.PATCH`.

Правила bump: MAJOR — несовместимые изменения, MINOR — новая
функциональность, PATCH — исправления/производительность/документация.

## Проверка после доработки

```bash
cd sim && cmake --build build -j8
cd waveshare && ~/.platformio-venv/bin/pio run -e BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_7
./scripts/sync_arduino.sh
```

Затем прогнать профильные режимы симулятора и глазами проверить скриншоты.
Полный чек-лист — в `AGENTS.md`.

## Ограничения и безопасность

- Прошивка Waveshare сейчас работает на демо-данных; реальные датчики
  подключаются по схеме, но на автомобиле ещё не проверялись.
- 12 В нельзя заводить на GPIO: только через оптроны/делители.
- Полной гальванической изоляции нет, если 12→5 В преобразователь не изолирован;
  `GND_CAR` и `GND` соединяются в одной точке на выходе U6.
- I²C на Waveshare общий с GT911 и CH422G; свободные адреса — ADS1115 `0x48`,
  MCP23017 `0x21`. CH422G на этой ревизии — `0x24`.
- Перед первым пуском — предохранитель 2 А и проверка полярности мультиметром.
- Калибровки температуры/топлива/скорости/оборотов проверить на машине.

## Ассеты и лицензии

- `assets/vw.gif` — заставка VW (пользовательский ассет).
- `assets/bye.gif` — чертёж T3/T25 для бай-бай экрана.
- `assets/icons_svg/` — исходники иконок (OEM/ISO 2575, Openclipart,
  Wikimedia Commons; лицензии в `_manifest.json`/исходниках).
- `assets/icons_svg/fuel.svg` — иконка топлива для WELLS (белая, рамка серая).
- `assets/icons_svg/temp.svg` — иконка температуры для WELLS (белая, рамка серая).
- `assets/icons_svg/settings.svg` — иконка переключения дизайна
  (пользовательский ассет; перекрашена под тёмную тему).
- `assets/icons_svg/vw_logo.svg` — логотип VW 2019 с Wikimedia Commons
  (Volkswagen_logo_2019.svg), вписан в центр WELLS.
- `assets/font_speed_160.c` — Montserrat Bold Italic 160 px (цифры и `-`),
  сгенерирован через `lv_font_conv`; лицензия SIL OFL 1.1 —
  `assets/Montserrat-OFL.txt`.
- Логотип VW — товарный знак Volkswagen. Проект личный/некоммерческий.
