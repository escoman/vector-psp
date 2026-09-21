# Vector-06C — эмулятор Вектор-06Ц для PlayStation Portable

PSP-порт эмулятора советского компьютера [Вектор-06Ц], написанный на базе проекта [svofski/vector06sdl](https://github.com/svofski/vector06sdl).

Ядро эмуляции (процессор i8080, память, таймер КР580ВИ53, sound-синтезатор AY-3-8910, контроллер НГМД FD1793, PixelFiller) перенесено из оригинального проекта практически без изменений. Платформенный слой переписан под PSP:

- **Однопоточная архитектура** (по мотивам Android-порта) — без Boost.Thread и SDL2
- **Видео** через PSP GU (`pspgu`/`pspgum`): два фреймбуфера 512×272, текстура 576×288 → 480×272
- **Звук** через PSP Audio Library: callback 44.1 кГц, стерео 16 бит, кольцевой буфер
- **Ввод** через `sceCtrlReadBufferPositive` с маппингом кнопок PSP на сканкоды Вектора

Текущая версия: **1.0.2** (см. `VERSION` в `Makefile`).

## Что нужно для сборки

1. **PSPSDK / тулчейн** (`psp-gcc`, `psp-g++`, `pack-pbp`, `mksfoex` и т.д.)
   любой удобной сборки, например:
   - [psptoolchain](https://github.com/pspdev/psptoolchain) — сборка тулчейна из исходников;
   - Docker-образ [`ubpsp/psp-devkit`](https://hub.docker.com/r/ubpsp/psp-devkit) — готовое окружение.

   Проверка установки — команда `psp-config` должна быть в `PATH` и возвращать путь к SDK:

   ```bash
   psp-config --pspsdk-path
   # /home/alexey/pspdev/psp/sdk
   ```

   Если тулчейн установлен, но `psp-config` не находится, добавьте его в `PATH`:

   ```bash
   export PATH="$HOME/pspdev/bin:$PATH"
   ```

2. **GNU make** и стандартные утилиты: `xxd` (нужен для генерации `boots.c` и `assets/logo_data.c`), `sed`, `cp`.

3. **Для тестирования** (необязательно для сборки): эмулятор [PPSSPP](https://www.ppsspp.org/) или настоящий PSP (CFW/6.61) с доступом к `ms0:`.

Отдельных зависимостей от SDL2/Boost в PSP-порте нет — всё необходимое лежит в репозитории (включая загрузочный ROM `boot/boots.bin`).

## Сборка релиза

Из корня репозитория:

```bash
make clean
make all
```

Результат:

- `v06xpsp.elf` — ELF-бинарник;
- `EBOOT.PBP` — готовый образ для PSP (title `Vector-06c PSP`);
- `release/VECTOR06C/EBOOT.PBP` — копия EBOOT в готовом пакете для распространения.

Сборка релиза по умолчанию: `DEBUG_ENABLED=0`, оптимизация `-O3`.

### Установка на PSP / в PPSSPP

Готовый пакет — папка `release/VECTOR06C/` — копируется на карту памяти в:

```
ms0:/PSP/GAME/VECTOR06C/
├── EBOOT.PBP        # сам эмулятор
└── ROMS/            # .rom/.bin файлы программ (берутся оттуда же)
```

В PPSSPP этот путь обычно выглядит как `~/.config/ppsspp/PSP/GAME/VECTOR06C/`.

### Архив для GitHub Releases

Скрипт `utils/make_release.sh` упаковывает `release/VECTOR06C/` в версионированный
архив `release/VECTOR06C-v<VERSION>.zip` (версия берётся из `VERSION` в Makefile):

```bash
make all                      # сначала собирать EBOOT.PBP
utils/make_release.sh         # -> release/VECTOR06C-v1.0.2.zip
```

Полученный zip прикладывается к релизу на странице `Releases → Draft new release`.

### Автоматический деплой в PPSSPP

В `deploy.mk` задан путь к игре в PPSSPP (`PPSSPP_DIR`), и сборку можно сразу деплоить:

```bash
make deploy-release    # release-сборка (-O3) + копирование в PPSSPP_DIR
make deploy-debug      # debug-сборка (-O2, логи) + деплой
make deploy-profile    # профайлер (-O2 -pg, gmon.out) + деплой
```

Путь можно переопределить: `make deploy-release PPSSPP_DIR=/custom/path`. Если `PPSSPP_DIR` не существует, деплой молча пропускается.

### Варианты сборок

| Команда | Особенности |
|---|---|
| `make` / `make all` | релиз: `-O3`, отладочный вывод выключен |
| `make DEBUG_ENABLED=1` | отладка: `-O2`, `dbglog()` пишет в лог |
| `make PROFILE=1` | профайлер: `-O2 -pg`, `-lpspprof`, генерирует `gmon.out` |
| `make CORE_DEBUG=1` | дополнительно включает отладку ядра эмуляции |

Подробнее по цели сборки — в [MAKEFILE.MD](MAKEFILE.MD).

## Структура проекта

```
├── Makefile          # правила сборки PSPSDK-обвязки, цели all/release
├── deploy.mk         # локальные цели деплоя в PPSSPP
├── src/              # ядро эмулятора + HAL (i8080, memory, board, tv, sound, ...)
├── gui/              # UI на PSP GU: ROM-браузер, главное меню, конфиг, VKBD ...
├── boot/boots.bin    # загрузочный ROM (2 КБ), встраивается в boots.c через xxd
├── assets/           # иконки/скриншоты EBOOT, логотип (logo_data.c генерируется)
├── release/          # выходной пакет: VECTOR06C/ (EBOOT.PBP + ROMS) + zip-архивы релизов
├── utils/            # утилиты: упаковка релиза, анализ звука/профиля
└── test/             # host-тесты отдельных модулей
```

## Примечания

- **Зависимости от заголовков не отслеживаются.** После изменения `.h`-файлов нужно делать `make clean` перед пересборкой, иначе объектные файлы могут остаться устаревшими.
- `boots.c` и `assets/logo_data.c` — **генерируемые файлы** (создаются Makefile'ом из `boot/boots.bin` и `assets/logo16.png` утилитой `xxd -i`). `logo_data.c` в git не хранится (см. `.gitignore`); `boots.c` оставлен в репозитории по традиции оригинала, но при пересборке перезаписывается.
- Предупреждение `psp-fixup-imports: stubs out of order` в конце сборки — известное косметическое явление PSPSDK, на работоспособность EBOOT не влияет.
- Документация порта: [PSP.md](PSP.md) (заметки по портированию), [MAKEFILE.MD](MAKEFILE.MD) (цели сборки), [UTILS.MD](UTILS.MD) (утилиты).

## Лицензия

Оригинальный проект [vector06sdl](https://github.com/svofski/vector06sdl) распространяется под лицензией MIT; этот порт наследует её условия.
