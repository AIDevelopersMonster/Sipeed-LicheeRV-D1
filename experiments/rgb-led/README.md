# Smooth RGB rainbow — Sipeed LicheeRV D1 Dock

The operator physically confirmed the board's multicolor LED can light
through Linux sysfs (`rgb:status`). This example smoothly changes its
hue using the existing kernel LED driver, without installing packages or
changing boot files.

## Quick start (Ubuntu 22.10, root UART)

```sh
curl -fL https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/rgb-led/rainbow.py -o /tmp/d1-rainbow.py
python3 /tmp/d1-rainbow.py --period 10 --fps 20 --brightness 50
```

Press **Ctrl+C** to restore the previous color and brightness. The script
runs in the foreground; it does not install a service. Actual smoothness
depends on LED driver/SPI overhead and needs operator confirmation.

One full color wheel in four seconds, stopping automatically after three:
```sh
python3 /tmp/d1-rainbow.py --period 4 --fps 25 --brightness 35 --cycles 3
```

Parameters:

- `--period`: seconds for one rainbow (2–120).
- `--fps`: color writes per second (1–40, default 20).
- `--brightness`: LED class brightness, 1–100% (default 50).
- `--cycles`: 0 runs until Ctrl+C (default), N runs exactly N revolutions.
- `--led-dir`: override the LED class path (default `/sys/class/leds/rgb:status`).

The script checks `brightness`, `max_brightness`, `multi_intensity`
and optional `multi_index`. On kernels exposing different channel order,
it interprets `multi_index` rather than assuming RGB. `multi_intensity`
scales individual color channel values; `brightness` controls the
global LED brightness. No writes are made to the HDMI, GPU, audio, kernel
modules or DTB. If the sysfs driver cannot update rapidly, lower `--fps`.

Important: The observed physical LED lighting was confirmed with manual
sysfs writes; the animation script itself was tested locally against a
mock sysfs directory, **not yet verified on the actual Dock**.
