#!/usr/bin/env python3
"""Generate the VW T3 dashboard schematic (SVG).

    python3 scripts/draw_schematic.py
    rsvg-convert -w 2400 schematic.svg -o schematic.png
"""
from __future__ import annotations

W, H = 2400, 2000
parts: list[str] = []


def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, cls="t", size=12, anchor="start", weight=None, fill=None):
    extra = f' font-weight="{weight}"' if weight else ""
    if fill: extra += f' fill="{fill}"'
    parts.append(f'<text x="{x}" y="{y}" class="{cls}" font-size="{size}" text-anchor="{anchor}"{extra}>{esc(s)}</text>')


def line(x1, y1, x2, y2, cls="wire", width=1.6):
    parts.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}" stroke-width="{width}"/>')


def poly(points, cls="wire", width=1.6):
    p = " ".join(f"{x},{y}" for x, y in points)
    parts.append(f'<polyline points="{p}" class="{cls}" stroke-width="{width}" fill="none"/>')


def rect(x, y, w, h, cls="box", rx=4, fill=None):
    f = f' fill="{fill}"' if fill else ""
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" class="{cls}"{f}/>')


def circle(x, y, r=3.0, cls="junction"):
    parts.append(f'<circle cx="{x}" cy="{y}" r="{r}" class="{cls}"/>')


def dot(x, y): circle(x, y, 3.0)


def net(x, y, label, anchor="start", color="#0d47a1"):
    parts.append(f'<rect x="{x-2.5}" y="{y-2.5}" width="5" height="5" fill="{color}"/>')
    text(x + (7 if anchor == "start" else -7), y + 4, label, cls="net", size=9, anchor=anchor, fill=color)


def gnd(x, y, label=None):
    line(x, y, x, y + 12)
    line(x - 8, y + 12, x + 8, y + 12)
    line(x - 5, y + 17, x + 5, y + 17)
    line(x - 2, y + 22, x + 2, y + 22)
    if label: text(x + 12, y + 18, label, cls="small", size=8)


def res_v(x, y, ref, val, h=40):
    rect(x - 8, y - h // 2, 16, h, cls="comp", rx=0, fill="#fff")
    text(x + 12, y - 2, ref, cls="ref", size=9)
    text(x + 12, y + 10, val, cls="val", size=8)


def res_h(x, y, ref, val, w=40):
    rect(x - w // 2, y - 8, w, 16, cls="comp", rx=0, fill="#fff")
    text(x - w // 2, y - 13, ref, cls="ref", size=9)
    text(x - w // 2, y + 23, val, cls="val", size=8)


def cap_v(x, y, ref, val, gap=6):
    line(x - 11, y - gap // 2, x + 11, y - gap // 2)
    line(x - 11, y + gap // 2, x + 11, y + gap // 2)
    text(x + 14, y - 2, ref, cls="ref", size=9)
    text(x + 14, y + 10, val, cls="val", size=8)


def cap_h(x, y, ref, val, gap=6):
    line(x - gap // 2, y - 11, x - gap // 2, y + 11)
    line(x + gap // 2, y - 11, x + gap // 2, y + 11)
    text(x - 8, y - 15, ref, cls="ref", size=9)
    text(x - 8, y + 24, val, cls="val", size=8)


def diode_v(x, y, ref, val, zener=False, down=True):
    d = 1 if down else -1
    poly([(x - 8, y - 8 * d), (x + 8, y - 8 * d), (x, y + 6 * d)], cls="comp", width=1.4)
    line(x - 8, y + 6 * d, x + 8, y + 6 * d)
    if zener:
        line(x - 8, y + 6 * d, x - 12, y + 10 * d)
        line(x + 8, y + 6 * d, x + 12, y + 2 * d)
    text(x + 13, y - 1, ref, cls="ref", size=9)
    text(x + 13, y + 11, val, cls="val", size=8)


def diode_h(x, y, ref, val, zener=False):
    poly([(x - 8, y - 8), (x - 8, y + 8), (x + 6, y)], cls="comp", width=1.4)
    line(x + 6, y - 8, x + 6, y + 8)
    if zener:
        line(x + 6, y - 8, x + 10, y - 12)
        line(x + 6, y + 8, x + 2, y + 12)
    text(x - 10, y - 13, ref, cls="ref", size=9)
    text(x - 10, y + 23, val, cls="val", size=8)


def ic(x, y, w, h, ref, title, left, right):
    rect(x, y, w, h, cls="box", rx=4, fill="#fff")
    text(x + 8, y + 16, ref, cls="ref", size=12, weight="700")
    text(x + 8, y + 30, title, cls="h", size=9)
    for side, pins in (("L", left), ("R", right)):
        n = len(pins)
        step = 32
        for i, (pin, name) in enumerate(pins):
            py = y + 58 + i * step
            if side == "L":
                line(x - 24, py, x, py)
                text(x + 5, py + 3, f"{pin} {name}".strip(), cls="pin", size=8)
                circle(x - 24, py, 2.2, "pin")
            else:
                line(x + w, py, x + w + 24, py)
                text(x + w - 5, py + 3, f"{name} {pin}".strip(), cls="pin", size=8, anchor="end")
                circle(x + w + 24, py, 2.2, "pin")


def card(x, y, w, h, title, subtitle=None):
    rect(x, y, w, h, cls="card", rx=10, fill="#fff")
    text(x + 12, y + 21, title, cls="h", size=13, weight="700")
    if subtitle: text(x + 12, y + 37, subtitle, cls="small", size=9)


def connector(x, y, pins, title):
    h = 18 + len(pins) * 18
    rect(x, y, 120, h, cls="comp", rx=3, fill="#fff")
    text(x + 6, y + 13, title, cls="ref", size=9)
    for i, (label, netname, color) in enumerate(pins):
        py = y + 28 + i * 18
        text(x + 6, py + 3, label, cls="pin", size=8)
        line(x + 120, py, x + 160, py)
        net(x + 166, py, netname, color=color)


# ------------------------------------------------------------------ frame
parts.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="#f7f9fc"/>')
for x in range(20, W, 20):
    parts.append(f'<line x1="{x}" y1="0" x2="{x}" y2="{H}" stroke="#e8eef5" stroke-width="0.4"/>')
for y in range(20, H, 20):
    parts.append(f'<line x1="0" y1="{y}" x2="{W}" y2="{y}" stroke="#e8eef5" stroke-width="0.4"/>')

text(40, 38, "VW T3 Dashboard — принципиальная электрическая схема", size=25, weight="700")
text(40, 60, "Waveshare ESP32-S3-Touch-LCD-7 · датчики 1.6 D/TD · лист 1/1 · rev 1 · 2026-11-07", cls="sub", size=11)
text(2360, 60, "12 В — только через предохранитель и оптрон", cls="note", size=9, anchor="end", fill="#b71c1c")

# ------------------------------------------------------------------ power
card(40, 100, 460, 310, "Питание", "12 В автомобиля → 5 В для дисплея")
rect(70, 170, 44, 54, cls="comp", rx=3, fill="#fff")
text(92, 162, "J_PWR", cls="ref", size=9, anchor="middle")
text(92, 190, "1", cls="pin", size=9, anchor="middle")
text(92, 220, "2", cls="pin", size=9, anchor="middle")
line(114, 190, 150, 190)
text(154, 186, "+12V_CAR", cls="net", size=8, fill="#c62828")
res_h(190, 190, "F1", "2 A", w=40)
line(210, 190, 230, 190)
diode_h(255, 190, "D1", "1N5822")
line(261, 190, 300, 190)
text(304, 186, "+12V_F", cls="net", size=8, fill="#c62828")
dot(320, 190)
line(320, 190, 320, 230)
diode_v(320, 255, "D2", "SMBJ15A", zener=True)
line(320, 240, 320, 247)
line(320, 272, 320, 285)
gnd(320, 285, "GND_CAR")
line(300, 190, 300, 230)
dot(300, 190)
cap_v(300, 245, "C1", "470µF/25V")
line(300, 252, 300, 285)
gnd(300, 285, "GND_CAR")
rect(370, 220, 100, 100, cls="box", rx=6, fill="#fff")
text(420, 242, "U6", cls="ref", size=11, anchor="middle")
text(420, 257, "12 В → 5 В 3 A", cls="small", size=8, anchor="middle")
text(420, 272, "MP1584EN", cls="small", size=8, anchor="middle")
text(378, 296, "IN+", cls="pin", size=8)
text(378, 314, "IN−", cls="pin", size=8)
text(460, 296, "OUT+", cls="pin", size=8, anchor="end")
text(460, 314, "OUT−", cls="pin", size=8, anchor="end")
line(300, 190, 370, 190)
line(370, 190, 370, 296)
line(370, 296, 366, 296)
line(320, 228, 370, 228)
line(370, 228, 370, 296)
line(370, 314, 366, 314)
line(320, 314, 370, 314)
line(320, 314, 320, 330)
gnd(320, 330, "GND_CAR")
line(460, 296, 495, 296)
text(499, 292, "+5V", cls="net", size=8, fill="#c62828")
line(495, 296, 495, 330)
cap_v(495, 345, "C2", "100µF/10V")
line(495, 352, 495, 365)
gnd(495, 365, "")
line(495, 330, 525, 330)
dot(495, 330)
cap_h(545, 330, "C3", "100nF")
line(551, 330, 575, 330)
gnd(575, 330, "")
line(460, 314, 525, 314)
gnd(525, 314, "GND")
text(60, 372, "GND_CAR и GND — одна точка на выходе U6.", cls="note", size=8)
text(60, 388, "D1 — защита от переполюсовки; D2 — TVS; >1 A лучше P-MOSFET.", cls="note", size=8)

# ------------------------------------------------------------------ U1
card(40, 440, 440, 520, "U1 — Waveshare ESP32-S3-Touch-LCD-7", "800×480, GT911, CH422G; питание 5 В")
text(60, 500, "5 В и GND — с гребёнки U1; 3V3 идёт на все модули.", cls="note", size=8)
text(60, 516, "U4 питается от +12 В через Rdrop; U5 — от 3.3 В.", cls="note", size=8)
text(60, 532, "Перемычка I²C level на плате — 3.3 В (не 5 В).", cls="note", size=8, fill="#b71c1c")
connector(60, 548, [("1  3V3", "3V3", "#ef6c00"), ("2  GND", "GND", "#263238"),
                    ("3  SDA/GPIO8", "SDA", "#1565c0"), ("4  SCL/GPIO9", "SCL", "#f9a825")], "J1 I²C")
connector(60, 620, [("1  3V3", "3V3", "#ef6c00"), ("2  GND", "GND", "#263238"),
                    ("3  ADC/GPIO6", "GPIO6", "#2e7d32")], "J2 SENSOR")
connector(60, 706, [("1  3V3", "3V3", "#ef6c00"), ("2  GND", "GND", "#263238"),
                    ("3  TX/GPIO43", "GPIO43", "#6a1b9a"), ("4  RX/GPIO44", "GPIO44", "#6a1b9a")], "J3 UART2")
text(60, 820, "GPIO15/16 — RS485; 19/20 — CAN/USB; 11/12/13 — TF. Не занимать.", cls="note", size=8, fill="#b71c1c")
text(60, 836, "I²C уже занят GT911 (0x5D/0x14) и CH422G (0x20).", cls="note", size=8)
text(60, 852, "Нумерация pin 1..4 — по шелкографии гребёнки.", cls="note", size=8)
text(60, 868, "5 В — на USB-C U1; 3.3 В на 5 В не подавать.", cls="note", size=8, fill="#b71c1c")

# ------------------------------------------------------------------ ADS1115
card(520, 100, 360, 300, "U2 — ADS1115", "модуль ADS1115 (Adafruit 1085 / CJMCU), 0x48")
ic(590, 160, 220, 200, "U2", "ADS1115",
   [("", "VDD"), ("", "GND"), ("", "SDA"), ("", "SCL"), ("", "ADDR")],
   [("", "A0"), ("", "A1"), ("", "A2"), ("", "A3")])
for py, lab, col in [(218, "3V3", "#ef6c00"), (250, "GND", "#263238"), (282, "SDA", "#1565c0"),
                     (314, "SCL", "#f9a825"), (346, "GND", "#263238")]:
    line(566, py, 540, py)
    net(534, py, lab, anchor="end", color=col)
for py, lab in [(218, "A0_BAT"), (250, "A1_COOL"), (282, "A2_FUEL"), (314, "A3_RPM")]:
    line(834, py, 864, py)
    net(870, py, lab, color="#2e7d32")
text(566, 388, "ADDR → GND: 0x48. ALRT не используется.", cls="note", size=8)

# ------------------------------------------------------------------ analog
card(920, 100, 520, 460, "Аналоговые входы", "делитель АКБ, pull-up датчиков, F/V")
# battery
text(940, 145, "АКБ", cls="ref", size=10)
line(940, 160, 990, 160)
text(944, 154, "+12V_F", cls="net", size=8, fill="#c62828")
res_h(1010, 160, "R1", "100k", w=42)
line(1031, 160, 1060, 160)
dot(1060, 160)
text(1064, 156, "A0_DIV", cls="net", size=8, fill="#2e7d32")
line(1060, 160, 1060, 185)
res_v(1060, 205, "R2", "22k")
line(1060, 225, 1060, 240)
gnd(1060, 240, "")
line(1040, 160, 1040, 195)
cap_v(1040, 210, "C4", "100nF")
line(1040, 217, 1040, 240)
gnd(1040, 240, "")
diode_v(1082, 205, "D3", "3V3", zener=True)
line(1082, 185, 1082, 197)
line(1082, 217, 1082, 240)
gnd(1082, 240, "")
line(1060, 160, 1110, 160)
res_h(1130, 160, "R3", "1k", w=34)
line(1147, 160, 1170, 160)
net(1176, 160, "A0_BAT", color="#2e7d32")
# coolant
text(940, 295, "Температура / CHT", cls="ref", size=10)
line(940, 310, 990, 310)
text(944, 304, "+3V3", cls="net", size=8, fill="#ef6c00")
res_h(1010, 310, "R4", "2.2k", w=42)
line(1031, 310, 1060, 310)
dot(1060, 310)
text(1064, 306, "A1_DIV", cls="net", size=8, fill="#2e7d32")
line(1060, 310, 1060, 335)
text(1064, 339, "датчик → GND", cls="small", size=8)
line(1040, 310, 1040, 340)
cap_v(1040, 355, "C5", "100nF")
line(1040, 362, 1040, 380)
gnd(1040, 380, "")
line(1060, 310, 1110, 310)
res_h(1130, 310, "R5", "1k", w=34)
line(1147, 310, 1170, 310)
net(1176, 310, "A1_COOL", color="#2e7d32")
# fuel
text(940, 445, "Топливо", cls="ref", size=10)
line(940, 460, 990, 460)
text(944, 454, "+3V3", cls="net", size=8, fill="#ef6c00")
res_h(1010, 460, "R6", "150", w=42)
line(1031, 460, 1060, 460)
dot(1060, 460)
text(1064, 456, "A2_DIV", cls="net", size=8, fill="#2e7d32")
line(1060, 460, 1060, 485)
text(1064, 489, "датчик → GND", cls="small", size=8)
line(1040, 460, 1040, 490)
cap_v(1040, 505, "C6", "100nF")
line(1040, 512, 1040, 530)
gnd(1040, 530, "")
line(1060, 460, 1110, 460)
res_h(1130, 460, "R7", "1k", w=34)
line(1147, 460, 1170, 460)
net(1176, 460, "A2_FUEL", color="#2e7d32")
text(940, 545, "R1/R2 = 100k/22k; D3 = 3.3 В; R4/R6 — pull-up датчиков.", cls="note", size=8)

# ------------------------------------------------------------------ MCP
card(1700, 100, 440, 540, "U3 — MCP23017", "MCP23017-E/SP (DIP-28) или модуль, 0x21")
ic(1780, 160, 280, 440, "U3", "MCP23017",
   [("9", "VDD"), ("10", "VSS"), ("12", "SCL"), ("13", "SDA"), ("15", "A0"), ("16", "A1"), ("17", "A2"), ("18", "RESET")],
   [("21", "GPA0"), ("22", "GPA1"), ("23", "GPA2"), ("24", "GPA3"), ("25", "GPA4"), ("26", "GPA5"),
    ("27", "GPA6"), ("28", "GPA7"), ("1", "GPB0"), ("2", "GPB1"), ("3", "GPB2"), ("8", "GPB7")])
for py, lab, col in [(218, "3V3", "#ef6c00"), (250, "GND", "#263238"), (282, "SCL", "#f9a825"),
                     (314, "SDA", "#1565c0"), (346, "3V3", "#ef6c00"), (378, "GND", "#263238"),
                     (410, "GND", "#263238"), (442, "3V3", "#ef6c00")]:
    line(1756, py, 1730, py)
    net(1724, py, lab, anchor="end", color=col)
right = [
    (218, "GPA0", "#2e7d32"), (250, "GPA1", "#2e7d32"), (282, "GPA2", "#2e7d32"), (314, "GPA3", "#2e7d32"),
    (346, "GPA4", "#2e7d32"), (378, "GPA5", "#2e7d32"), (410, "GPA6", "#2e7d32"), (442, "GPA7", "#2e7d32"),
    (474, "GPB0", "#2e7d32"), (506, "GPB1", "#2e7d32"), (538, "GPB2", "#2e7d32"),
    (570, "GPB7", "#0d47a1"),
]
for py, lab, col in right:
    line(2060, py, 2088, py)
    net(2094, py, lab, color=col)
text(1780, 620, "A0=1, A1=0, A2=0; RESET → 3V3. CH422G — 0x24.", cls="note", size=8)
text(1780, 634, "GPA0..7/GPB0..2 — входы оптронов; GPB3..6 — NC; GPB7 — зуммер.", cls="note", size=8)

# ------------------------------------------------------------------ buzzer
card(1700, 680, 440, 230, "BZ1 — зуммер (опционально)", "GPB7 → NPN")
net(1760, 740, "GPB7", color="#0d47a1")
line(1796, 736, 1820, 736)
res_h(1840, 736, "R18", "1k", w=34)
line(1857, 736, 1888, 736)
line(1888, 736, 1908, 736)
line(1908, 736, 1908, 756)
line(1908, 756, 1920, 746)
line(1908, 756, 1920, 766)
line(1920, 746, 1920, 724)
line(1920, 766, 1920, 786)
line(1920, 786, 1920, 800)
gnd(1920, 800, "")
text(1928, 754, "Q2", cls="ref", size=8)
text(1928, 768, "2N2222", cls="val", size=7)
line(1888, 766, 1888, 786)
res_v(1888, 800, "R19", "10k")
line(1888, 820, 1888, 832)
gnd(1888, 832, "")
line(1920, 724, 1920, 708)
line(1920, 708, 1950, 708)
rect(1950, 694, 70, 28, cls="comp", rx=4, fill="#fff")
text(1985, 712, "BZ1", cls="ref", size=8, anchor="middle")
text(1985, 736, "5 В", cls="val", size=7, anchor="middle")
line(2020, 708, 2050, 708)
net(2056, 708, "+5V", color="#c62828")
line(1950, 708, 1950, 770)
line(2020, 708, 2020, 770)
diode_h(1985, 770, "D7", "1N4148")
line(1972, 770, 1950, 770)
line(2003, 770, 2020, 770)
text(1740, 890, "Активный зуммер 5 В; D7 — обязательно; R19 — 10k база-эмиттер.", cls="note", size=8)

# ------------------------------------------------------------------ U4 IC
card(480, 620, 320, 280, "U4 — LM2917N-8 (DIP-8)", "F/V, W-терминал генератора")
ic(540, 680, 200, 200, "U4", "LM2917N-8",
   [("1", "TACH+"), ("2", "CP1"), ("3", "CP2/IN+"), ("4", "EMIT")],
   [("8", "GND"), ("7", "IN−"), ("6", "V+"), ("5", "COL")])
for py, lab, col in [(738, "W_IN", "#c62828"), (770, "CP1", "#2e7d32"),
                     (802, "CP2", "#2e7d32"), (834, "EMIT", "#2e7d32")]:
    line(516, py, 492, py)
    net(486, py, lab, anchor="end", color=col)
for py, lab, col in [(738, "GND", "#263238"), (770, "EMIT", "#2e7d32"),
                     (802, "V_FV", "#c62828"), (834, "V_FV", "#c62828")]:
    line(764, py, 788, py)
    net(794, py, lab, color=col)
text(500, 880, "Пассивные цепи — в блоке ниже.", cls="note", size=8)

# ------------------------------------------------------------------ U5 IC
card(840, 600, 260, 340, "U5 — LM1815N (DIP-14)", "адаптивный усилитель ABS")
ic(880, 660, 200, 280, "U5", "LM1815",
   [("1", "NC"), ("2", "GND"), ("3", "VIN"), ("4", "NC"), ("5", "MODE"), ("6", "NC"), ("7", "PEAK")],
   [("14", "RC"), ("13", "NC"), ("12", "REF"), ("11", "SEL"), ("10", "OUT"), ("9", "TIMING"), ("8", "VCC")])
for py, lab, col in [(718, "NC", "#616161"), (750, "GND", "#263238"), (782, "ABS_IN", "#c62828"),
                     (814, "NC", "#616161"), (846, "GND", "#263238"), (878, "NC", "#616161"),
                     (910, "PEAK", "#2e7d32")]:
    line(856, py, 832, py)
    net(826, py, lab, anchor="end", color=col)
for py, lab, col in [(718, "RC", "#2e7d32"), (750, "NC", "#616161"), (782, "REF", "#2e7d32"),
                     (814, "GND", "#263238"), (846, "ABS_OUT", "#2e7d32"), (878, "GND", "#263238"),
                     (910, "3V3", "#ef6c00")]:
    line(1104, py, 1128, py)
    net(1134, py, lab, color=col)
text(850, 960, "Mode 3: pin 5/11/9 → GND. Пассивные цепи — ниже.", cls="note", size=8)

# ------------------------------------------------------------------ RPM networks
card(500, 920, 600, 260, "Цепи RPM (U4)")
# row 1: W input
net(520, 970, "W_TERM", color="#c62828")
line(556, 966, 576, 966)
res_h(596, 966, "R8", "10k", w=32)
line(612, 966, 642, 966)
net(648, 966, "W_IN", color="#c62828")
line(620, 966, 620, 995)
line(600, 995, 650, 995)
diode_v(600, 1018, "D4", "18V", zener=True)
line(600, 995, 600, 1010)
line(600, 1026, 600, 1042)
gnd(600, 1042, "")
cap_v(650, 1018, "C7", "10nF")
line(650, 995, 650, 1012)
line(650, 1025, 650, 1042)
gnd(650, 1042, "")
# CP1
net(720, 970, "CP1", color="#2e7d32")
line(720, 976, 720, 1002)
cap_v(720, 1018, "C8", "3.3nF")
line(720, 1025, 720, 1042)
gnd(720, 1042, "")
# CP2
net(800, 970, "CP2", color="#2e7d32")
line(800, 976, 800, 1002)
line(765, 1002, 805, 1002)
res_v(765, 1018, "R9", "100k")
line(765, 1038, 765, 1050)
gnd(765, 1050, "")
cap_v(805, 1018, "C9", "1µF")
line(805, 1002, 805, 1012)
line(805, 1025, 805, 1042)
gnd(820, 1042, "")
# row 2: V+
net(520, 1110, "+12V_F", color="#c62828")
line(556, 1106, 576, 1106)
res_h(596, 1106, "R11", "470Ω 1W", w=42)
line(617, 1106, 647, 1106)
net(653, 1106, "V_FV", color="#c62828")
line(630, 1106, 630, 1132)
line(610, 1132, 650, 1132)
cap_v(610, 1148, "C10", "10µF/16V")
line(610, 1132, 610, 1142)
line(610, 1155, 610, 1170)
gnd(610, 1170, "")
cap_v(650, 1148, "C11", "100nF")
line(650, 1132, 650, 1142)
line(650, 1155, 650, 1170)
gnd(650, 1170, "")
# R10
net(720, 1110, "RPM_OUT", color="#2e7d32")
line(720, 1116, 720, 1130)
res_v(720, 1148, "R10", "10k")
line(720, 1168, 720, 1172)
gnd(720, 1172, "")
# R12 -> A3
net(800, 1088, "RPM_OUT", color="#2e7d32")
line(800, 1092, 800, 1106)
line(800, 1106, 820, 1106)
res_h(840, 1106, "R12", "1k", w=32)
line(856, 1106, 886, 1106)
net(892, 1106, "A3_RPM", color="#2e7d32")
line(870, 1106, 870, 1132)
line(850, 1132, 890, 1132)
cap_v(850, 1148, "C12", "100nF")
line(850, 1132, 850, 1142)
line(850, 1155, 850, 1170)
gnd(850, 1170, "")
diode_v(890, 1148, "D5", "3V3", zener=True)
line(890, 1132, 890, 1142)
line(890, 1155, 890, 1170)
gnd(890, 1170, "")

# ------------------------------------------------------------------ ABS networks
card(1120, 920, 420, 260, "Цепи ABS (U5)")
# row 1
net(1140, 970, "ABS+", color="#c62828")
line(1176, 966, 1196, 966)
res_h(1216, 966, "R13", "18k", w=32)
line(1232, 966, 1262, 966)
net(1268, 966, "ABS_IN", color="#c62828")
text(1140, 1068, "ABS− → GND_CAR", cls="small", size=8)
line(1240, 966, 1240, 996)
cap_v(1240, 1012, "C13", "100pF")
line(1240, 1019, 1240, 1036)
gnd(1240, 1036, "")
# PEAK
net(1320, 970, "PEAK", color="#2e7d32")
line(1320, 976, 1320, 996)
line(1300, 1012, 1340, 1012)
line(1320, 996, 1320, 1006)
res_v(1300, 1028, "R14", "1.6M")
line(1300, 1048, 1300, 1062)
gnd(1300, 1062, "")
cap_v(1340, 1028, "C14", "0.33µF")
line(1340, 1012, 1340, 1022)
line(1340, 1035, 1340, 1050)
gnd(1340, 1050, "")
# REF
net(1430, 970, "REF", color="#2e7d32")
line(1430, 976, 1430, 996)
res_v(1430, 1018, "R16", "5.6k")
line(1430, 1038, 1430, 1052)
gnd(1430, 1052, "")
# row 2
net(1140, 1110, "RC", color="#2e7d32")
line(1140, 1116, 1140, 1130)
res_v(1140, 1148, "R15", "150k")
line(1140, 1168, 1140, 1174)
net(1144, 1174, "3V3", color="#ef6c00")
line(1180, 1116, 1180, 1130)
cap_v(1180, 1148, "C15", "1nF")
line(1180, 1155, 1180, 1170)
gnd(1180, 1170, "")
# output
net(1260, 1088, "ABS_OUT", color="#2e7d32")
line(1260, 1092, 1260, 1106)
line(1260, 1106, 1280, 1106)
res_h(1300, 1106, "R17", "1k", w=32)
line(1316, 1106, 1346, 1106)
net(1352, 1106, "ABS_GPIO6", color="#2e7d32")
line(1340, 1106, 1340, 1132)
line(1320, 1132, 1360, 1132)
cap_v(1320, 1148, "C16", "100nF")
line(1320, 1132, 1320, 1142)
line(1320, 1155, 1320, 1170)
gnd(1320, 1170, "")
diode_v(1360, 1148, "D6", "3V3", zener=True)
line(1360, 1132, 1360, 1142)
line(1360, 1155, 1360, 1170)
gnd(1360, 1170, "")

# ------------------------------------------------------------------ GPS
card(1120, 1200, 420, 240, "J4 — NEO-6M GPS", "3.3 В, 9600 бод, UART2")
rect(1140, 1250, 110, 110, cls="comp", rx=4, fill="#fff")
text(1195, 1268, "NEO-6M", cls="ref", size=9, anchor="middle")
for lab, y in [("VCC", 1290), ("GND", 1308), ("TX", 1326), ("RX", 1344)]:
    text(1150, y, lab, cls="pin", size=8)
    line(1250, y, 1290, y)
    net(1296, y, {"VCC": "3V3", "GND": "GND", "TX": "GPS_TX", "RX": "GPS_RX"}[lab],
        color={"VCC": "#ef6c00", "GND": "#263238", "TX": "#6a1b9a", "RX": "#6a1b9a"}[lab])
text(1140, 1390, "TX GPS → GPIO44 (RX U1); RX GPS ← GPIO43 (TX U1).", cls="note", size=8)
text(1140, 1406, "UART2 делит порт с USB-UART; одновременно не использовать.", cls="note", size=8)

# ------------------------------------------------------------------ opto
card(40, 1200, 660, 660, "OPT1 — 16-канальная оптронная развязка", "PC817, 12 В вход, 3.3 В открытый коллектор")
text(285, 1238, "барьер изоляции", cls="small", size=8, anchor="middle")

# high-side
text(60, 1255, "A — high-side 12 В (лампа/шина +12 В):", cls="note", size=9)
net(70, 1280, "IN_HS", color="#c62828")
line(106, 1276, 126, 1276)
res_h(146, 1276, "R1", "1k 2W", w=40)
line(166, 1276, 200, 1276)
# LED: anode top, cathode bottom
poly([(190, 1288), (210, 1288), (200, 1302)], cls="comp", width=1.4)
line(190, 1302, 210, 1302)
line(200, 1276, 200, 1288)
line(200, 1302, 200, 1320)
gnd(200, 1320, "GND_CAR")
text(164, 1270, "LED1", cls="small", size=7)
# reverse diode D1 across LED
diode_v(235, 1298, "D1", "1N4148", down=False)
line(235, 1280, 235, 1286)
line(235, 1310, 235, 1320)
line(200, 1280, 235, 1280)
line(200, 1320, 235, 1320)
# isolation barrier
parts.append('<line x1="285" y1="1245" x2="285" y2="1535" stroke="#b0bec5" stroke-width="2" stroke-dasharray="8 6"/>')
# phototransistor
line(330, 1280, 330, 1320)
line(330, 1288, 350, 1276)
line(330, 1312, 350, 1324)
line(350, 1276, 350, 1266)
line(350, 1324, 350, 1334)
text(332, 1272, "Q1", cls="small", size=7)
# collector pull-up and output
line(350, 1266, 380, 1266)
line(380, 1266, 380, 1250)
line(380, 1250, 410, 1250)
text(414, 1254, "+3V3", cls="net", size=8, fill="#ef6c00")
res_h(395, 1262, "R2", "10k", w=30)
line(395, 1250, 395, 1254)
line(395, 1270, 395, 1266)
line(380, 1266, 410, 1266)
net(416, 1266, "OUT", color="#0d47a1")
# emitter
line(350, 1334, 380, 1334)
gnd(380, 1334, "GND")

# low-side
text(60, 1405, "B — low-side (замыкание на массу):", cls="note", size=9)
net(70, 1440, "+12V_F", color="#c62828")
line(106, 1436, 126, 1436)
res_h(146, 1436, "R3", "1k 2W", w=40)
line(166, 1436, 200, 1436)
# LED
poly([(190, 1448), (210, 1448), (200, 1462)], cls="comp", width=1.4)
line(190, 1462, 210, 1462)
line(200, 1436, 200, 1448)
line(200, 1462, 200, 1480)
# switch to ground
circle(200, 1480, 2.5, "junction")
line(200, 1480, 220, 1466)
circle(220, 1480, 2.5, "junction")
text(204, 1496, "SW", cls="small", size=7)
line(220, 1480, 220, 1498)
gnd(220, 1498, "GND_CAR")
# reverse diode D2
diode_v(245, 1458, "D2", "1N4148", down=False)
line(245, 1440, 245, 1446)
line(245, 1470, 245, 1480)
line(200, 1440, 245, 1440)
line(200, 1480, 245, 1480)
# output side
line(330, 1440, 330, 1480)
line(330, 1448, 350, 1436)
line(330, 1472, 350, 1484)
line(350, 1436, 350, 1426)
line(350, 1484, 350, 1494)
text(332, 1432, "Q3", cls="small", size=7)
line(350, 1426, 380, 1426)
line(380, 1426, 380, 1410)
line(380, 1410, 410, 1410)
text(414, 1414, "+3V3", cls="net", size=8, fill="#ef6c00")
res_h(395, 1422, "R4", "10k", w=30)
line(395, 1410, 395, 1414)
line(395, 1430, 395, 1426)
line(380, 1426, 410, 1426)
net(416, 1426, "OUT", color="#0d47a1")
line(350, 1494, 380, 1494)
gnd(380, 1494, "GND")

# table
text(60, 1595, "Каналы OPT1:", cls="h", size=10, weight="700")
rows = [
    ("IN1", "масло (low-side)", "OUT1", "GPA0"),
    ("IN2", "заряд (low-side)", "OUT2", "GPA1"),
    ("IN3", "дальний свет (high)", "OUT3", "GPA2"),
    ("IN4", "ближний свет (high)", "OUT4", "GPA3"),
    ("IN5", "поворот L (high)", "OUT5", "GPA4"),
    ("IN6", "поворот R (high)", "OUT6", "GPA5"),
    ("IN7", "свечи накала (low)", "OUT7", "GPA6"),
    ("IN8", "зажигание 15 (high)", "OUT8", "GPA7"),
    ("IN9", "ручник (low)", "OUT9", "GPB0"),
    ("IN10", "масло 1.8 бар (low, опц.)", "OUT10", "GPB1"),
    ("IN11", "неисправность тормозов (low)", "OUT11", "GPB2"),
]
for i, (a, b, c, d) in enumerate(rows):
    y = 1616 + i * 15
    text(62, y, a, cls="pin", size=8)
    text(108, y, b, cls="small", size=8)
    text(300, y, c, cls="pin", size=8)
    text(348, y, d, cls="pin", size=8)
text(60, 1800, "Выход оптрона active-low; внешние 10k pull-up на 3.3 В.", cls="note", size=8)
text(60, 1816, "Если готовый модуль имеет pull-up на 5 В — убрать: 5 В на MCP нельзя.", cls="note", size=8, fill="#b71c1c")
text(60, 1832, "Q1/Q3 — фототранзисторы PC817 внутри OPT1; LED1/LED3 — входные светодиоды. Q2 — 2N2222 зуммера.", cls="note", size=8)
text(60, 1848, "Оптроны разрывают сигнальные цепи; при общем GND buck полной гальванической изоляции нет.", cls="note", size=8)

# ------------------------------------------------------------------ netlist
card(1560, 1200, 800, 660, "Netlist и правила", "ключевые цепи; полный список — wiring.json")
notes = [
    ("+12V_CAR", "J_PWR.1 → F1"),
    ("+12V_F", "F1 → D1 → U6.IN+, R11, R1, D4"),
    ("+5V", "U6.OUT+ → U1.5V, BZ1, C2/C3"),
    ("+3V3", "U1.J1.1 → U2.VDD, U3.VDD, U5.VCC, GPS.VCC, pull-up"),
    ("GND", "U6.OUT− → U1/U2/U3/U5/GPS/опто-выход"),
    ("GND_CAR", "J_PWR.2 → U6.IN−, опто-вход, датчики, ABS−, W−"),
    ("SDA/SCL", "U1.J1.3/4 → U2/U3 (4.7k на U1)"),
    ("A0_DIV/A0_BAT", "R1/R2/D3/C4 → R3 → U2.A0"),
    ("A1_DIV/A1_COOL", "R4/датчик/C5 → R5 → U2.A1"),
    ("A2_DIV/A2_FUEL", "R6/датчик/C6 → R7 → U2.A2"),
    ("A3_RPM", "U4.RPM_OUT/R12/C12/D5 → U2.A3"),
    ("GPIO6", "U5.ABS_OUT/R17/C16/D6 → U1.J2.3"),
    ("GPB7", "U3.GPB7 → R18 → Q2.B"),
    ("GPS_TX/RX", "J4.TX→U1.J3.4 (GPIO44); U1.J3.3→J4.RX"),
    ("GPA0..7", "OUT1..8 опто → U3"),
    ("GPB0..2", "OUT9..11 опто → U3"),
]
for i, (n, d) in enumerate(notes):
    y = 1245 + i * 22
    text(1580, y, n, cls="pin", size=8, fill="#0d47a1")
    text(1680, y, d, cls="small", size=8)
text(1580, 1620, "Правила:", cls="h", size=10, weight="700")
rules = [
    "1. 12 В — только через предохранитель и оптрон; не заводить на GPIO.",
    "2. Логика и I²C — 3.3 В; входы ADS1115 не выше VDD+0.3 В.",
    "3. GND_CAR и GND — одна точка на выходе U6 (звезда).",
    "4. Оптрон: вход 12 В, выход open-collector, pull-up 10k на 3.3 В.",
    "5. U5 LM1815 — 3.3 В; выход 3.3 В безопасен для GPIO6.",
    "6. U4 LM2917 — Rdrop 470Ω 1W; V+ ≈ 7.56 В (внутр. стабилитрон).",
    "7. ABS: square-wave от ECU → U5 не ставить, вход через 10k/оптрон.",
    "8. Калибровки датчиков проверить на автомобиле.",
    "9. Схема не проверена на автомобиле; первый пуск — через предохранитель 2 А.",
]
for i, r in enumerate(rules):
    text(1580, 1642 + i * 17, r, cls="small", size=8)

# ------------------------------------------------------------------ title
rect(1960, 1880, 400, 100, cls="box", rx=4, fill="#fff")
text(1975, 1902, "VW T3 Dash · schematic", cls="h", size=10, weight="700")
text(1975, 1920, "Waveshare ESP32-S3-Touch-LCD-7", cls="small", size=8)
text(1975, 1936, "1.6 D/TD · 12 В · 2026-11-07", cls="small", size=8)
text(1975, 1952, "rev 1 · лист 1/1", cls="small", size=8)

svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="{W}" height="{H}" viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg">
<defs><style>
  .sub {{ font-family: Helvetica, Arial, sans-serif; fill: #546e7a; }}
  .h {{ font-family: Helvetica, Arial, sans-serif; fill: #102027; }}
  .t {{ font-family: Helvetica, Arial, sans-serif; fill: #263238; }}
  .small {{ font-family: Helvetica, Arial, sans-serif; fill: #607d8b; }}
  .note {{ font-family: Helvetica, Arial, sans-serif; fill: #37474f; }}
  .ref {{ font-family: Helvetica, Arial, sans-serif; fill: #263238; font-weight: 700; }}
  .val {{ font-family: Helvetica, Arial, sans-serif; fill: #455a64; }}
  .pin {{ font-family: Helvetica, Arial, sans-serif; fill: #263238; }}
  .net {{ font-family: Helvetica, Arial, sans-serif; }}
  .wire {{ stroke: #263238; }}
  .comp {{ stroke: #263238; fill: #fff; }}
  .box {{ fill: #fff; stroke: #263238; stroke-width: 1.6; }}
  .card {{ fill: #fff; stroke: #90a4ae; stroke-width: 1.2; }}
  .junction {{ fill: #263238; }}
</style></defs>
{chr(10).join(parts)}
</svg>
'''
with open("schematic.svg", "w", encoding="utf-8") as f:
    f.write(svg)
print("wrote schematic.svg")
