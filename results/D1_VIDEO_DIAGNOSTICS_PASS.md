# D1 VIDEO DIAGNOSTICS — аппаратный отчёт

**Дата:** 29 октября 2026 года  
**Плата:** Sipeed LicheeRV D1 Dock / Allwinner D1 / XuanTie C906  
**Источник:** фактический вывод `scripts/d1-video-diag.sh` из SSH-терминала MobaXterm  
**Итог:** **D1_VIDEO_DIAGNOSTICS_PASS — PASS 10 / FAIL 0 / INFO 0**

> Здесь приведены только данные, подтверждённые текущим запуском. Локальные IP-адреса, MAC, SSID и пароли не публикуются.

## Проверенные компоненты

| Секция | Фактический результат | Статус |
| --- | --- | --- |
| 01 Operating System | Ubuntu 24.04.3 LTS, Linux 6.8.0-60-generic, riscv64 | PASS |
| 02 Real Processor | HART 0, `uarch: thead,c906`, MMU Sv39, `rv64imafdc...` | PASS |
| 03 Physical Memory | Linux RAM 452 MiB; used 150 MiB, available 302 MiB; swap 0 | PASS |
| 04 microSD | `mmcblk0` 30G; `mmcblk0p1` ext4 `/`; `mmcblk0p5` vfat `/boot/efi` | PASS |
| 05 Wi-Fi and IPv4 | RTL8723DS via `rtw_8723ds`, `wlan0` UP, IPv4 assigned, default route | PASS |
| 06 Remote Management | Работающая интерактивная SSH-сессия, `systemd-networkd` active | PASS |
| 07 Toolchain | GCC 13.3.0, Make 4.3, GDB 15.1, Python 3.12.3, objdump | PASS |
| 08 Native Execution | На плате собрана и выполнена программа на C; `123 + 456 = 579` | PASS |
| 09 Summary | `PASS: 10 FAIL: 0 INFO: 0` | PASS |

**Диск:** корневая файловая система `/dev/mmcblk0p1` — 29G, использовано 2.6G, свободно 27G (9%).

## Первый эксперимент выполнения машинного кода RISC-V

Проверочная C-функция:

~~~c
__attribute__((noinline))
uint64_t add(uint64_t a, uint64_t b) { return a + b; }
~~~

Реальный вывод исполняемой программы:

~~~text
C906 EXECUTION: 123 + 456 = 579
[PASS] Native C compilation and execution
~~~

Фрагмент **фактического дизассемблирования**:

~~~asm
00000000000006e0 <add>:
 6e0:  952e    c.add   a0,a1
 6e2:  8082    c.jr    ra
~~~

`c.add a0,a1` выполняет сложение аргументов, `c.jr ra` — возврат из функции. Обе инструкции имеют 16-битную кодировку расширения **RISC-V C**. Поскольку выполнение и дизассемблирование получены на настоящем D1, подтверждена полная цепочка **GCC → машинный код RV64 → выполнение на C906 → проверка результата**.

Этот тест подтверждает нативную компиляцию и исполнение, но **не означает завершения всей LAB-01**, если она будет включать дополнительные тесты ISA, ABI и отладчика.

## Ограничения

- Этот прогон подтвердил Wi-Fi link/IPv4/default route, **но не проверял ping до маршрутизатора или доступность интернета**.
- HDMI, Bluetooth, SPI/I²C/GPIO и JTAG здесь не тестировались.
- Вывод о памяти и объёме диска относится к моменту измерения.
- Скрипт не выводит IP/MAC/SSID/секреты. Для публичных видео это сознательное ограничение.

## Воспроизведение

~~~bash
curl -fL https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/scripts/d1-video-diag.sh -o ~/d1-video-diag.sh
bash ~/d1-video-diag.sh
~~~

Скрипт последовательно печатает разделы, компилирует небольшой временный C-файл и удаляет его после проверки. Для видеозаписи в MobaXterm достаточно повторного запуска `bash ~/d1-video-diag.sh`.

**Контрольная метка:** `D1_VIDEO_DIAGNOSTICS_PASS`.
