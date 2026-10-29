# D1 Ubuntu 22.10 — HDMI / Wi-Fi baseline PASS

**Дата:** 2026-10-29  
**Образ:** `sipeed_d1_ubuntu22.10_20221116.img`  
**Ядро:** `5.17.0-1003-allwinner`, `riscv64`  
**Плата:** Sipeed LicheeRV D1 Dock, Allwinner D1 / C906

| Проверка | Статус | Подтверждение |
| --- | --- | --- |
| Boot Ubuntu 22.10 | PASS | Вывод login и UART-shell `root@ubuntu:~#` |
| HDMI hardware text console | PASS | Фото загрузки и `ubuntu login:` на физическом мониторе |
| Active HDMI VT | PASS | `cat /sys/class/tty/tty0/active` → `tty1` |
| Framebuffer | PASS | `cat /proc/fb` → `0 sun4i-drmdrmfb` |
| DRM device | PASS | `/dev/dri/card0`, `platform-display-engine-card` |
| Console output | PASS | Фото надписи `HELLO FROM RISC-V D1 VIA SSH` на HDMI после команды записи в `/dev/tty1`; точный исходный сеанс не подтверждён |
| Color framebuffer rendering | PASS | Фото пяти горизонтальных цветных полос на HDMI |
| Wi-Fi | PASS | `wlan0 UP`, `192.168.1.89/24` во время проверки |
| Internet connectivity | PASS | `ping -c 3 1.1.1.1`, 0% потерь, 19.053 мс средняя RTT |
| SSH as `d1user` | PASS | `d1user@ubuntu:~$`; `who` показывал `pts/0` с удалённым адресом `192.168.1.65` |
| `sudo` for `d1user` | FAIL | `d1user is not in the sudoers file` |
| `su` from `d1user` | FAIL | `su: Authentication failure`; причину не установили |
| Graphical desktop | NOT PRESENT in observed session | Xorg/Wayland/LightDM не работали, `DISPLAY` пустой |
| Direct HDMI DOOM, FPS and latency | NOT TESTED | Предыдущий Freedoom запускался на другой Ubuntu 24.04.3 через SSH/X11 |
| Backup microSD | NOT CONFIRMED | До опасных экспериментов обязательно сохранить образ карты |

## Консоли

```text
HDMI: tty1
UART: ttyS0 (root)
SSH: /dev/pts/0 (d1user)
```

## Политика сохранения

Не изменять ядро, DTB, U-Boot, настройки HDMI, драйверы. Не делать `apt upgrade` на эталонной microSD. Прежде чем продолжать, создать отдельную резервную копию и получить исходный файл `d1_hdmi_test.py`. Фото и сам файл пока не включены в репозиторий.

## Документация

- [Пошаговое описание и безопасные диагностические команды](../docs/ubuntu-22.10-hdmi/README.md)
- [Предыдущий эксперимент с Freedoom по SSH/X11](D1_FREEDOOM_RENDER_PASS.md)
