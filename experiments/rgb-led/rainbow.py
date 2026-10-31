#!/usr/bin/env python3
"""Smooth color-wheel animation for LicheeRV D1's Linux multicolor LED.

Uses only the existing /sys/class/leds/rgb:status interface. No modules,
packages, kernel, DTB, HDMI, or background services are changed.
"""

import argparse
import colorsys
import sys
import time
from pathlib import Path

DEFAULT_LED = Path("/sys/class/leds/rgb:status")
LED_COLORS = {"1": "red", "2": "green", "3": "blue"}


def led_order(led_dir):
    index = led_dir / "multi_index"
    if not index.exists():
        print("multi_index is absent; assuming RGB channel order", file=sys.stderr)
        return ("red", "green", "blue")
    names = tuple(LED_COLORS.get(x.lower(), x.lower()) for x in index.read_text().split())
    if len(names) != 3 or set(names) != {"red", "green", "blue"}:
        raise ValueError("Unsupported multi_index: " + " ".join(names))
    return names


def parse_args():
    p = argparse.ArgumentParser(description="Smooth RGB rainbow via Linux LED sysfs")
    p.add_argument("--led-dir", type=Path, default=DEFAULT_LED,
                   help="LED sysfs directory (default: %(default)s)")
    p.add_argument("--period", type=float, default=10.0,
                   help="seconds for one complete rainbow (default: 10)")
    p.add_argument("--fps", type=int, default=20,
                   help="LED color changes per second (default: 20, max: 40)")
    p.add_argument("--brightness", type=int, default=50,
                   help="global brightness percentage 1..100 (default: 50)")
    p.add_argument("--cycles", type=int, default=0,
                   help="stop after N full rainbows; 0 means until Ctrl+C")
    a = p.parse_args()
    if not 2.0 <= a.period <= 120.0:
        p.error("--period must be between 2 and 120 seconds")
    if not 1 <= a.fps <= 40:
        p.error("--fps must be in 1..40")
    if not 1 <= a.brightness <= 100:
        p.error("--brightness must be in 1..100")
    if a.cycles < 0:
        p.error("--cycles must be non-negative")
    return a


def run(args):
    led = args.led_dir
    intensity = led / "multi_intensity"
    brightness = led / "brightness"
    max_brightness = led / "max_brightness"
    for p in (intensity, brightness, max_brightness):
        if not p.is_file():
            raise FileNotFoundError("Required LED attribute not found: " + str(p))

    order = led_order(led)
    maximum = int(max_brightness.read_text().strip())
    if maximum < 1:
        raise ValueError("Invalid max_brightness")
    old_brightness = brightness.read_text().strip()
    old_intensity = intensity.read_text().strip()
    if len(old_intensity.split()) != len(order):
        raise ValueError("Unexpected multi_intensity channel count")

    multi_max = led / "multi_max_intensity"
    limits = tuple(map(int, multi_max.read_text().split())) if multi_max.exists() else (255, 255, 255)
    if len(limits) != len(order) or any(x <= 0 for x in limits):
        raise ValueError("Unsupported multi_max_intensity")
    level = max(1, round(maximum * args.brightness / 100.0))

    print("D1 RGB rainbow: order=%s, brightness=%d%%, period=%.1fs, %d fps" %
          ("/".join(order), args.brightness, args.period, args.fps))
    print("Ctrl+C stops the effect and restores the previous LED color and brightness.")
    try:
        brightness.write_text(str(level) + "\n")
        start = time.monotonic()
        frame = 0
        previous = None
        while True:
            now = time.monotonic()
            elapsed = now - start
            if args.cycles and elapsed >= args.cycles * args.period:
                break
            r, g, b = colorsys.hsv_to_rgb((elapsed / args.period) % 1.0, 1.0, 1.0)
            color = {"red": r, "green": g, "blue": b}
            value = " ".join(str(round(color[name] * limits[i]))
                             for i, name in enumerate(order))
            if value != previous:
                intensity.write_text(value + "\n")
                previous = value
            frame += 1
            delay = start + frame / args.fps - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            else:
                frame = max(frame, int((time.monotonic() - start) * args.fps) + 1)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        # Restore even after Ctrl+C or an error writing an animation frame.
        for path, value in ((intensity, old_intensity), (brightness, old_brightness)):
            try:
                path.write_text(value + "\n")
            except OSError as exc:
                print("Cannot restore %s: %s" % (path, exc), file=sys.stderr)
        print("Previous LED settings restored (if the writes succeeded).")


def main():
    try:
        return run(parse_args())
    except (OSError, ValueError) as exc:
        print("D1 RGB ERROR: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
