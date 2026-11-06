#!/usr/bin/env python3
"""Rasterise an SVG into an LVGL 8 TRUE_COLOR_ALPHA C array.

The dashboard icons are trimmed to their visible bounds, scaled to a fixed
width and stored as 3 bytes per pixel: RGB565 (little-endian) + alpha.

Requires rsvg-convert (librsvg) on PATH. Uses only the Python standard library.

Usage:
    scripts/svg_to_icon.py icon.svg icon_name [--width 30] [--out file.c]

Prints the C block to stdout when --out is omitted.
"""

import argparse
import struct
import subprocess
import sys
import tempfile
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")

    pos = 8
    width = height = None
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, depth, color, comp, filt, interlace = struct.unpack(
                ">IIBBBBB", chunk)
            if depth != 8 or color != 6 or interlace != 0:
                raise ValueError("expected 8-bit RGBA non-interlaced PNG")
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break

    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    pixels = bytearray(width * height * 4)
    prev = bytearray(stride)
    pos = 0
    for y in range(height):
        ftype = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ftype == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                b = prev[i]
                c = prev[i - 4] if i >= 4 else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        elif ftype != 0:
            raise ValueError("unsupported PNG filter %d" % ftype)
        pixels[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, pixels


def trim(width, height, pixels, threshold=8):
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        row = y * width * 4
        for x in range(width):
            if pixels[row + x * 4 + 3] > threshold:
                if x < min_x:
                    min_x = x
                if x > max_x:
                    max_x = x
                if y < min_y:
                    min_y = y
                if y > max_y:
                    max_y = y
    if max_x < 0:
        raise ValueError("image is fully transparent")
    return min_x, min_y, max_x + 1, max_y + 1


def resize(width, height, pixels, box, out_width):
    x0, y0, x1, y1 = box
    sw, sh = x1 - x0, y1 - y0
    out_height = max(1, int(round(sh * out_width / float(sw))))
    out = bytearray(out_width * out_height * 4)
    for oy in range(out_height):
        sy0 = y0 + oy * sh // out_height
        sy1 = max(sy0 + 1, y0 + (oy + 1) * sh // out_height)
        for ox in range(out_width):
            sx0 = x0 + ox * sw // out_width
            sx1 = max(sx0 + 1, x0 + (ox + 1) * sw // out_width)
            r = g = b = a = n = 0
            for sy in range(sy0, sy1):
                row = sy * width * 4
                for sx in range(sx0, sx1):
                    p = row + sx * 4
                    r += pixels[p]
                    g += pixels[p + 1]
                    b += pixels[p + 2]
                    a += pixels[p + 3]
                    n += 1
            q = (oy * out_width + ox) * 4
            out[q] = r // n
            out[q + 1] = g // n
            out[q + 2] = b // n
            out[q + 3] = a // n
    return out_width, out_height, out


def to_c(name, width, height, pixels):
    body = []
    for i in range(0, len(pixels), 4):
        r, g, b, a = pixels[i:i + 4]
        color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        body.append("0x%02x, 0x%02x, 0x%02x" % (color & 0xFF, color >> 8, a))
    lines = []
    for i in range(0, len(body), 10):
        lines.append("    " + ", ".join(body[i:i + 10]) + ",")
    return (
        "static const uint8_t %s_map[%d] = {\n%s\n};\n\n"
        "const lv_img_dsc_t %s = {\n"
        "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n"
        "    .header.always_zero = 0,\n"
        "    .header.w = %d,\n"
        "    .header.h = %d,\n"
        "    .data_size = %d,\n"
        "    .data = %s_map,\n"
        "};\n"
        % (name, width * height * 3, "\n".join(lines), name,
           width, height, width * height * 3, name)
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("svg")
    ap.add_argument("name")
    ap.add_argument("--width", type=int, default=30)
    ap.add_argument("--out")
    args = ap.parse_args()

    with tempfile.NamedTemporaryFile(suffix=".png") as tmp:
        subprocess.run(
            ["rsvg-convert", "-w", "600", args.svg, "-o", tmp.name],
            check=True)
        width, height, pixels = read_png(tmp.name)

    box = trim(width, height, pixels)
    width, height, pixels = resize(width, height, pixels, box, args.width)
    text = to_c(args.name, width, height, pixels)
    if args.out:
        with open(args.out, "w") as f:
            f.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
