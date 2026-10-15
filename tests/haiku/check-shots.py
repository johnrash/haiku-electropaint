#!/usr/bin/env python3
"""Checks the screenshots from vm-test.sh: ElectroPaint's squares are fully
saturated colours on black, so the area the saver draws into must contain
plenty of saturated pixels in more than one hue.

usage: check-shots.py SHOT-DIR
"""
import colorsys
import os
import re
import sys

from PIL import Image


def colourful(image, box):
    region = image.convert("RGB").crop(box)
    count = 0
    hues = set()
    for r, g, b in region.get_flattened_data():
        if max(r, g, b) - min(r, g, b) > 60 and max(r, g, b) > 80:
            count += 1
            h, _, _ = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            hues.add(int(h * 12))
    return count, len(hues)


def preview_box(image, shots):
    """The preview's screen in the preferences window: the window frame
    comes from hey (vm-test.sh), the screen is the area of long runs of black
    pixels in it (text has no long runs; its subpixel colour fringes must not
    count)."""
    with open(os.path.join(shots, "preferences-frame.txt")) as file:
        numbers = [float(n) for n in re.findall(r"-?\d+\.\d+", file.read())]
    left, top, right, bottom = [int(n) for n in numbers[:4]]
    pixels = image.convert("RGB").load()
    box = None
    for y in range(top, bottom + 1):
        run = 0
        for x in range(left, right + 1):
            run = run + 1 if max(pixels[x, y]) < 8 else 0
            if run >= 60:
                if box is None:
                    box = [x, y, x, y]
                box = [min(box[0], x - run + 1), min(box[1], y),
                    max(box[2], x), max(box[3], y)]
    if box is None:
        return (0, 0, 0, 0)
    return (box[0], box[1], box[2] + 1, box[3] + 1)


def main():
    shots = sys.argv[1]
    failed = 0

    checks = [
        # saverhost's window content: 640x480 at (80, 80)
        ("window.png", lambda image: (80, 80, 720, 560), 500),
        # screen_blanker covers the whole screen
        ("screen_blanker.png", lambda image: (0, 0) + image.size, 2000),
        # the preview in the preferences window
        ("preferences.png", lambda image: preview_box(image, shots), 20),
    ]
    for name, box, minimum in checks:
        path = os.path.join(shots, name)
        if not os.path.exists(path):
            print("FAIL  %s missing" % name)
            failed += 1
            continue
        image = Image.open(path)
        area = box(image)
        count, hues = colourful(image, area)
        ok = count >= minimum and hues >= 2
        print("%s  %s %s: %d saturated pixels in %d hues (want >= %d, >= 2)"
              % ("ok   " if ok else "FAIL ", name, area, count, hues, minimum))
        failed += 0 if ok else 1

    return 1 if failed else 0


sys.exit(main())
