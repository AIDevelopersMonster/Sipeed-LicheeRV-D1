# Sipeed LicheeRV D1

RISC-V development on Allwinner D1 / XuanTie C906.

## Documented systems

- [Ubuntu 24.04.3 setup and toolchain](docs/ubuntu-setup/README.md) — kernel 6.8; HDMI not verified.
- [Ubuntu 22.10 Sipeed HDMI setup (RU)](docs/ubuntu-22.10-hdmi/README.md) — kernel 5.17.0-1003-allwinner; physical HDMI and color output verified.
- [Ubuntu 22.10 hardware test receipt](results/D1_UBUNTU22_HDMI_WIFI_BASELINE_PASS.md)
- [Read-only HDMI inventory](scripts/d1-hdmi-inventory.sh)
- [Freedoom via remote X11](results/D1_FREEDOOM_RENDER_PASS.md) — graphical output PASS, playable latency FAIL.

## Hardware

- Sipeed LicheeRV D1 Dock, 512 MiB DDR3, 32 GB microSD, RTL8723DS Wi-Fi.

## Policy

Protect the functioning Ubuntu 22.10 HDMI configuration: no kernel, DTB, bootloader or display-driver changes without a separate microSD backup. The original framebuffer color-test script d1_hdmi_test.py and photos have not yet been archived in this repository.
