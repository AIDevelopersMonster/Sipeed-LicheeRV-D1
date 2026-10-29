# D1_DOOM_HDMI_RENDER_PASS — первое изображение Doom на физическом HDMI

**Дата:** 29 октября 2026 года  
**Оборудование:** Sipeed LicheeRV D1 Dock, Allwinner D1 / RISC-V XuanTie C906  
**ОС:** Ubuntu 22.10, kernel `5.17.0-1003-allwinner`, `riscv64`  
**Видеовыход:** напрямую в Linux framebuffer `/dev/fb0` (`sun4i-drmdrmfb`), физический HDMI, без X11  
**Статус:** **`D1_DOOM_HDMI_RENDER_PASS`** (видна заставка), `D1_DOOM_HDMI_GAMEPLAY_PENDING`.

## Подтверждённое наблюдение

На D1 нативно скомпилирован и запущен DoomGeneric с нашей реализацией `DG_*` для framebuffer и свободными игровыми ресурсами Freedoom 0.13.0.

Оператор видел на физическом HDMI **заставку Doom**. Уточнение оператора: **мерцала не сама заставка; поверх/на фоне заставки мелькал мелкий текст, который трудно разобрать**. Первоначальная формулировка о мерцании заставки была скорректирована оператором. Читаемость текста и его источник пока не установлены; не выдавать предположение о `tty1` за установленный диагноз.

## Запуск

В SSH под `d1user`:

```bash
cd ~/d1-doom-fb/upstream/doomgeneric
timeout --signal=INT 30s ./doomgeneric \
  -iwad /home/d1user/d1-doom-fb/freedoom1.wad \
  -nosound -warp 1 1
```

Предварительные условия: `d1user` входит в группу `video`, доступ к `/dev/fb0` уже PASS; UART `root` остаётся открытым.

## Журнал запуска (фрагмент)

```text
I_Init: Setting up machine state.
M_Init: Init miscellaneous info.
R_Init: Init DOOM refresh daemon - .......................................
P_Init: Init Playloop state.
S_Init: Setting up sound.
D_CheckNetGame: Checking network game status.
startskill 2 deathmatch: 0 startmap: 1 startepisode: 1
player 1 of 1 (1 nodes)
Emulating the behavior of the 'Ultimate Doom' executable.
HU_Init: Setting up heads up display.
ST_Init: Init status bar.
I_InitGraphics: framebuffer: x_res: 640, y_res: 400, x_virtual: 640, y_virtual: 400, bpp: 32
I_InitGraphics: framebuffer: RGBA: 8888, red_off: 16, green_off: 8, blue_off: 0, transp_off: 24
I_InitGraphics: DOOM screen size: w x h: 320 x 200
I_InitGraphics: Auto-scaling factor: 2
D1_DOOM_FB_FPS approx 38.9 frames/s
D1_DOOM_FB_FPS approx 44.7 frames/s
D1_DOOM_FB_FPS approx 44.8 frames/s
D1_DOOM_FB_FPS approx 44.6 frames/s
D1_DOOM_FB_FPS approx 44.6 frames/s
D1_DOOM_FB_EXIT: resources released, previous framebuffer area restored.
```

Техническая интерпретация:

- Внутренний игровой буфер `640 x 400` = 2x масштаб исходного Doom `320 x 200`. **Физический framebuffer остаётся 1024 x 600, 32 bpp, stride 4096**. Эти два размера не противоречат друг другу: программа рисует прямоугольник 640x400 в центре физического экрана.
- Лог измерил частоту **вызовов графической отрисовки**, примерно 44.6–44.8 кадров/с после старта. Это **не гарантированный FPS сложной игровой сцены**, не измерение задержки управления.
- Штатное завершение обработало освобождение ресурсов и восстановление затронутого прямоугольника framebuffer.
- Видна заставка на HDMI; конкретная 3D-сцена и играбельность в этом запуске **пока не подтверждены**.

## Возможная причина мелкого мерцающего текста

Активная виртуальная консоль `tty1` использует тот же `fbcon` framebuffer, что и порт Doom. Ввиду того, что backend не переводит консоль в `KD_GRAPHICS`, мигание текстового курсора или перерисовка консоли **возможны** поверх игровых кадров. Другая возможность — собственный мелкий текст интерфейса Freedoom. Пока это гипотезы.

### Минимальный безопасный тест без переключения видеорежима

В существующей UART-консоли **root**:

```bash
printf '\033[2J\033[H\033[?25l' > /dev/tty1
```

Команда очищает **текстовую консоль** и скрывает курсор, **не меняет HDMI-драйвер, разрешение или kernel mode**. После этого повторить запуск Doom в SSH, сравнить текстовые артефакты. Завершив тест, восстановить видимость курсора из UART:

```bash
printf '\033[?25h' > /dev/tty1
```

Экран после очистки может остаться без приглашения `login:`; физическая клавиша Enter в `tty1` может запросить новое приглашение. UART/SSH остаются доступны. Не применять переключение VT или `KDSETMODE` до проверки этого менее инвазивного подхода.

## Контроль следующего этапа

- [ ] Исключить или подтвердить вмешательство `fbcon`/курсора.
- [ ] Сделать фотографию кадра, чтобы прочитать источник мелькающего текста без зависимости от зрения оператора.
- [ ] Проверить появление **игрового уровня**, управление с SSH и реальные показатели.
- [ ] При необходимости увеличить масштаб прямоугольника отрисовки до 960x600, предварительно оценив влияние на CPU/FPS.
- [ ] Сохранить исходник `/home/d1user/d1_hdmi_test.py` (пока присутствует только на плате).

Исходники: [`experiments/doom-fb/`](../experiments/doom-fb/). Сборка: [`D1_DOOM_BUILD_PASS`](D1_DOOM_BUILD_PASS.md).

**Никаких обновлений ядра, изменения Device Tree, видео-драйвера или загрузчика в ходе эксперимента не проводилось.**
