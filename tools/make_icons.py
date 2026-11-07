#!/usr/bin/env python3
"""Draws the icons of the addon from the mark of the app (a diamond in a diamond in a diamond, with sharp corners), into assets/:

  * quick-access.png        the icon beside the bar of Nexus, 64 by 64, in the gold of the app
  * quick-access-hover.png  the same when the mouse is over it, lighter

    python3 tools/make_icons.py
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "assets"))
GOLD = (0xD9, 0xA8, 0x4E)
LIGHT = (0xF0, 0xD0, 0x8A)


def mark(size, colour):
    """the mark filling most of a clear square: two rings and a filled core, each corner sharp"""
    n = size * 8
    layer = Image.new("L", (n, n), 0)
    draw = ImageDraw.Draw(layer)
    c = n / 2
    r = n * 0.46
    stroke = n * 0.075
    reach = stroke / 2 * 2 ** 0.5

    def points(radius):
        return [(c, c - radius), (c + radius, c), (c, c + radius), (c - radius, c)]

    for radius in (r * 0.93, r * 0.57):
        draw.polygon(points(radius + reach), fill=255)
        draw.polygon(points(radius - reach), fill=0)
    draw.polygon(points(r * 0.24), fill=255)
    img = Image.new("RGBA", (n, n), colour + (0,))
    img.putalpha(layer)
    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    mark(64, GOLD).save(os.path.join(OUT, "quick-access.png"), optimize=True)
    mark(64, LIGHT).save(os.path.join(OUT, "quick-access-hover.png"), optimize=True)
    print("icons written to", OUT)


if __name__ == "__main__":
    main()
