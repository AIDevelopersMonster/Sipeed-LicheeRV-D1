# Sipeed LicheeRV D1 Dock — от распаковки Ubuntu до нативной среды разработки RISC-V

**Отчёт об аппаратном эксперименте, 29 октября 2026 года.** Windows / Ubuntu 24.04.3 LTS / Allwinner D1 / XuanTie C906.

> Документ фиксирует реально пройденный путь и подтверждённые результаты. Тест C-программы LAB-01 пока предложен, но не подтверждён запуском на оборудовании.

## 1. Цель и стенд

Цель: подготовить Sipeed LicheeRV D1 Dock как самостоятельный Linux-компьютер RISC-V для изучения процессора и системного программирования.

| Компонент | Конфигурация |
| --- | --- |
| SoC | Allwinner D1, T-Head XuanTie C906 |
| CPU | 1 HART, RISC-V 64-bit |
| MMU | Sv39 |
| ОЗУ | 512 МиБ DDR3 |
| microSD | 32 ГБ |
| Wi-Fi | RTL8723DS (SDIO) |
| Хост | Windows, PowerShell |
| Диагностика | USB–UART 3,3 В, COM4, 115200 8N1 |
| ОС | Ubuntu 24.04.3 LTS Server, Linux 6.8.0-60-generic |

Это настоящий микропроцессор C906, **не FPGA** и не программная эмуляция.

## 2. Получение и распаковка Ubuntu

Официальный образ Canonical: `ubuntu-24.04.3-preinstalled-server-riscv64+licheerv.img.xz`.

[Документация Canonical для LicheeRV Dock](https://ubuntu.com/hardware/docs/boards/how-to/ubuntu_supported/sipeed-licheerv-dock/).

Распаковка 7-Zip на Windows (пути примерные):

~~~powershell
& "C:\Program Files\7-Zip\7z.exe" x `
  "C:\Users\CHUWI\Downloads\ubuntu-24.04.3-preinstalled-server-riscv64+licheerv.img.xz" `
  "-oC:\Tools\LicheeRV"
~~~

Фактический размер IMG: **4 831 838 208 байт**. Локально измеренный SHA-256 распакованного файла:

~~~text
09BE3CB6EDB091454EA88867D3C4CE35D43C0198AD545907EB7010CC57253E2E
~~~

Это **хеш нашего экземпляра**, а не опубликованная контрольная сумма Canonical. Проверка файла:

~~~powershell
Get-FileHash "C:\Tools\LicheeRV\ubuntu-24.04.3-preinstalled-server-riscv64+licheerv.img" -Algorithm SHA256
~~~

## 3. Запись и проверка microSD

Вначале PhoenixCard не заработал. balenaEtcher сообщал об ошибке обработки образа и предупреждении `Missing partition table`. Результат первого процесса записи не соответствовал исходному образу. Вывод: **не доверять только сообщению записывающей программы**.

В IMG были обнаружены защитная MBR (тип `0xEE`), GPT (`EFI PART`) и загрузочная сигнатура Allwinner `eGON.BT0` в области SPL по смещению **131072 байта (128 КиБ)**. После повторной записи на физической microSD чтение того же участка дало:

~~~text
BYTES=512
MAGIC=eGON.BT0
~~~

Для проверки на Windows открыть PowerShell **от администратора**, сначала проверить номер карты:

~~~powershell
Get-Disk | Format-Table Number,FriendlyName,Size,PartitionStyle
~~~

Пример **только чтения** (номер `1` заменить на номер своей microSD):

~~~powershell
$disk = 1
Get-Disk -Number $disk | Format-List Number,FriendlyName,Size,BusType
$fs = $null
try {
    $fs = [IO.File]::Open(
        "\\.\PhysicalDrive$disk",
        [IO.FileMode]::Open,
        [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite
    )
    $fs.Position = 131072
    $buffer = [byte[]]::new(512)
    $n = $fs.Read($buffer, 0, 512)
    if ($n -ne 512) { throw "Incomplete read: $n" }
    $magic = [Text.Encoding]::ASCII.GetString($buffer, 4, 8)
    "SPL MAGIC: [$magic]"
}
finally {
    if ($null -ne $fs) { $fs.Dispose() }
}
~~~

Критерий: `SPL MAGIC: [eGON.BT0]`. Наличие сигнатуры доказывает правильность этого фрагмента карты, но ещё не гарантирует полную загрузку Ubuntu.

## 4. FEL и XFEL — диагностика до старта Linux

Сначала Windows обнаруживала плату как `USB VID_1F3A&PID_EFE8`: это **Allwinner USB FEL ROM**, а не COM-порт. Для доступа был установлен WinUSB через Zadig и использован [XFEL](https://github.com/xboot/xfel).

~~~powershell
& "C:\Tools\xfel\xfel.exe" version
~~~

Реальный ответ:

~~~text
AWUSBFEX ID=0x00185900(D1/F133) dflag=0x44 dlength=0x08 scratchpad=0x00045000
~~~

Выполнены успешные чтение SRAM, инициализация DDR, запись/чтение тестового слова `0xA55A1234` по адресу `0x40010000` с восстановлением исходного значения. Это подтверждает доступ к процессору и памяти, но **не** работу SD-загрузчика.

## 5. UART: подтверждение всей цепочки загрузки

Подключить адаптер **с логическими уровнями 3,3 В**:

| Dock UART0 | USB–UART |
| --- | --- |
| TX | RX |
| RX | TX |
| GND | GND |

Не соединять питание адаптера с платой: Dock питается отдельно от USB Type-C. В Windows получен `COM4`. Использован PuTTY в режиме Serial `115200 8N1`, Flow Control None.

Из реального UART-журнала:

~~~text
U-Boot SPL 2024.01-rc1
Trying to boot from MMC1

OpenSBI v1.3
Platform Name: Sipeed Lichee RV Dock
Platform HART Count: 1

U-Boot 2024.01-rc1
DRAM: 512 MiB

efi/boot/bootriscv64.efi
GNU GRUB version 2.12

Loading Linux 6.8.0-60-generic ...
EFI stub: Booting Linux Kernel...
mmc0: new high speed SDHC card
mmcblk0: ... 30.0 GiB
mmcblk0: p1 p2 p3 p4 p5 p6
EXT4-fs (mmcblk0p1): mounted filesystem
~~~

Итог:

~~~text
Ubuntu 24.04.3 LTS ubuntu ttyS0
ubuntu login:
~~~

Первоначальный логин образа: `ubuntu` / `ubuntu`; после первого входа система может требовать смену пароля. Поздние сообщения `cloud-init` могут попадать поверх приглашения входа — достаточно нажать Enter.

**UBUNTU_LOGIN_PASS.**

## 6. Настоящий RISC-V под Linux

Команды:

~~~bash
uname -a
cat /proc/cpuinfo
free -h
lsblk
df -h /
~~~

Результаты:

~~~text
processor : 0
hart      : 0
isa       : rv64imafdc_zicntr_zicsr_zifencei_zihpm
mmu       : sv39
uarch     : thead,c906
~~~

Linux: `6.8.0-60-generic`, `riscv64`. При физических 512 МиБ память, учитываемая Linux, около 452 МиБ. Корень — ext4 на `mmcblk0p1`; занято примерно 2,1 ГБ из 29 ГБ, доступно около 27 ГБ. EFI — раздел `mmcblk0p5`.

## 7. Wi-Fi RTL8723DS без стороннего драйвера

Начало: `wlan0 DOWN`, маршрута нет.

~~~bash
lsmod | grep -E '8723|rtw88'
modinfo rtw88_8723ds
sudo dmesg | grep -Ei 'rtw88|8723|firmware|wlan' | tail -35
~~~

Подтверждены модули `rtw88_8723ds`, `rtw88_8723d`, `rtw88_sdio`, `rtw88_core`, `mac80211`, `cfg80211`.

~~~text
rtw_8723ds mmc1:0001:1: Firmware version 48.0.0, H2C version 0
~~~

Сообщение об отсутствии `rtl_bt/rtl8723ds_fw.bin` касалось **Bluetooth** и не препятствовало подключению Wi-Fi.

Активны `systemd-networkd` и `wpa_supplicant`, установлен Netplan. Мы создали **отдельный** файл `/etc/netplan/90-wifi.yaml`, не меняя `50-cloud-init.yaml`:

~~~yaml
network:
  version: 2
  renderer: networkd
  wifis:
    wlan0:
      dhcp4: true
      access-points:
        "YOUR_WIFI_SSID":
          password: "YOUR_WIFI_PASSWORD"
~~~

**Обязательно заменить SSID и пароль на реальные данные своей сети 2,4 ГГц.** Не публиковать этот файл с настоящими реквизитами.

~~~bash
sudo chmod 600 /etc/netplan/90-wifi.yaml
sudo netplan generate
sudo netplan apply
ip -br addr show wlan0
ip route
~~~

Фактический результат:

~~~text
wlan0 UP 192.168.1.89/24 metric 600
default via 192.168.1.254 dev wlan0
~~~

**D1_WIFI_CONNECTED_PASS**. Главная методическая ошибка первоначальной диагностики: попытка исследовать подключение **до задания настоящих SSID и пароля**.

## 8. SSH с Windows

В PowerShell компьютера, находящегося в той же локальной сети:

~~~powershell
ssh ubuntu@192.168.1.89
~~~

После подтверждения SSH-ключа устройства и ввода пароля получено приглашение:

~~~text
Welcome to Ubuntu 24.04.3 LTS (GNU/Linux 6.8.0-60-generic riscv64)
ubuntu@ubuntu:~$
~~~

~~~bash
hostname
uname -m
~~~

Результат:

~~~text
ubuntu
riscv64
~~~

**D1_SSH_PASS**. IP-адрес выдан DHCP и может изменяться. Для постоянной работы разумно закрепить DHCP-адрес в маршрутизаторе и настроить SSH-ключи.

## 9. Нативные средства разработки

На D1 через SSH:

~~~bash
sudo apt update
sudo apt install -y build-essential gdb binutils
gcc --version
make --version
gdb --version
~~~

Фактические версии:

| Программа | Версия |
| --- | --- |
| GCC | 13.3.0 |
| GNU Make | 4.3 (`riscv64-unknown-linux-gnu`) |
| GDB | 15.1 |
| ОС | Ubuntu 24.04.3 LTS |
| ISA | riscv64 |

**D1_NATIVE_TOOLCHAIN_PASS.** Теперь можно собирать и отлаживать код непосредственно на реальном C906, без обязательной кросс-компиляции на Windows.

### Предложение для LAB-01 (не выдавать за выполненный эксперимент)

~~~c
#include <stdio.h>
#include <stdint.h>

__attribute__((noinline))
uint64_t add(uint64_t a, uint64_t b) {
    return a + b;
}

int main(void) {
    uint64_t result = add(123, 456);
    printf("Sipeed LicheeRV D1\n");
    printf("CPU: RISC-V C906\n");
    printf("123 + 456 = %llu\n", (unsigned long long)result);
    return result == 579 ? 0 : 1;
}
~~~

~~~bash
gcc -O2 -g -o lab01 main.c
./lab01
echo "EXIT_CODE=$?"
objdump -d -M no-aliases lab01 | grep -A 12 '<add>:'
~~~

Этот тест **предложен**, но его выполнение пока не зафиксировано в аппаратном журнале.

## 10. Достигнутые контрольные точки и ограничения

| Проверка | Результат |
| --- | --- |
| Физическое чтение SPL на microSD | PASS |
| FEL / XFEL / DDR | PASS |
| SPL → OpenSBI → U-Boot → GRUB → Linux | PASS |
| Ubuntu login и SD/ext4 | PASS |
| RTL8723DS Wi-Fi, DHCP | PASS |
| SSH через Wi-Fi | PASS |
| GCC 13.3 / Make 4.3 / GDB 15.1 | PASS |
| HDMI | Не подтверждено: GRUB сообщал `no suitable video mode found` |
| Bluetooth | Не проверен; файл прошивки отсутствовал |
| GPIO, SPI, I²C, JTAG | Не проверены |
| Бенчмарки C906 | Не проводились |
| Первый запуск C-программы LAB-01 | Ещё не подтверждён |

## Вывод

Начав с неизвестного USB-устройства, проблем записи microSD и отсутствия изображения HDMI, мы завершили стенд работающим Linux и нативной средой разработки.

**Проверенная цепочка:**

~~~text
microSD → SPL → OpenSBI → U-Boot → GRUB → Linux → Wi-Fi → SSH → GCC/GDB
~~~

Каждый уровень проверяли отдельно: исходный IMG, байты физической SD, FEL, UART, Linux, прошивку Wi-Fi, конфигурацию сети, DHCP и SSH. Именно **аппаратные доказательства вместо предположений** сделали развёртывание воспроизводимым.

Дальнейшие направления — исследование ISA C906, системные вызовы, память, прерывания и периферия; отдельно интересно сравнение физического C906 с PicoRV32 на Tang Nano 9K.

## Источники

- [Canonical: Ubuntu on Sipeed LicheeRV Dock](https://ubuntu.com/hardware/docs/boards/how-to/ubuntu_supported/sipeed-licheerv-dock/)
- [Canonical RISC-V images](https://ubuntu.com/download/risc-v/canonical-built)
- [Sipeed LicheeRV Dock Wiki](https://wiki.sipeed.com/hardware/en/lichee/RV/Dock.html)
- [XFEL](https://github.com/xboot/xfel)

*Основано на реальных измерениях и логах 28–29 октября 2026 года. Результаты относятся к конкретной плате и проверенному образу Ubuntu 24.04.3 LTS.*
