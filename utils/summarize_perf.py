#!/usr/bin/env python3
"""summarize_perf.py - свести perf.log AUTOSELECT-прогона в одну таблицу.

Прогон делает utils/run_60sec_kill; perf.log лежит рядом с EBOOT в
каталоге игры и содержит по две строки на секунду:

  PERF loop=.. mach=.. exec=.. snd=.. render=.. cpu=.. fill=.. fastfb=..
       sync=.. vbl=.. flush=.. ms
  SND  ev=.. tmr=.. ay=.. mix=.. samples=.. aysteps=..

Значения - миллисекунды в формате целые.миллисекунды (%u.%03u), накопленные
за окно в 1 секунду и обнулённые после записи.

Что в этих цифрах точно, а что приблизительно:
  mach, loop        - счётчики, точные
  exec, snd, render - замеряются вокруг вызова целиком, точные
  fastfb            - то же, что exec/snd, точный
  cpu, fill         - ОЦЕНКА: шаг меньше разрешения часов (1 мкс), поэтому
                      пропорция берётся из блока шагов и нормируется на
                      измеренный промежуток; сумма cpu+fill не может
                      превысить exec, но деление внутри него - приближение
  ev, tmr, ay, mix  - точные подэтапы process_frame (AUTOSELECT-сборка)

Заголовок окна (ms/frame) - доля от 20 мс машинного кадра:
  <поле>/mach, а "занято" = (exec + snd + render + fastfb)/mach.

Использование:
  ./utils/summarize_perf.py [perf.log ...]
  ./utils/summarize_perf.py ~/snap/.../VECTOR06C/perf.log
"""

import re
import sys
import os

GAME_DIR = os.path.expanduser(
    "~/snap/ppsspp-emu/common/.config/ppsspp/PSP/GAME/VECTOR06C")

# поля PERF-строки: имя -> (итог в мс на окно, делить ли на mach)
PERF_FIELDS = ["loop", "mach", "exec", "snd", "render", "cpu", "fill",
               "fastfb", "sync", "vbl", "flush"]
SND_FIELDS = ["ev", "tmr", "ay", "mix", "samples", "aysteps"]

# точные поля vs оценочные - чтобы в отчёте их не путать
EXACT = {"loop", "mach", "exec", "snd", "render", "fastfb",
         "ev", "tmr", "ay", "mix", "samples", "aysteps"}

NUM = r"(\d+)\.(\d{3})"


def to_ms(whole, frac):
    return int(whole) + int(frac) / 1000.0


def parse(path):
    """-> (список окон PERF, список окон SND)"""
    perf, snd = [], []
    with open(path, "r", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("PERF"):
                win = {}
                for name in PERF_FIELDS:
                    m = re.search(r"\b%s=(%s)" % (name, NUM), line)
                    if m:
                        win[name] = to_ms(m.group(1), m.group(2))
                    else:
                        # mach/loop - целые счётчики без дробной части
                        m = re.search(r"\b%s=(\d+)" % name, line)
                        win[name] = float(m.group(1)) if m else None
                perf.append(win)
            elif line.startswith("SND"):
                win = {}
                for name in SND_FIELDS:
                    m = re.search(r"\b%s=(%s)" % (name, NUM), line)
                    if m:
                        win[name] = to_ms(m.group(1), m.group(2))
                    else:
                        m = re.search(r"\b%s=(\d+)" % name, line)
                        win[name] = float(m.group(1)) if m else None
                snd.append(win)
    return perf, snd


def median(values):
    values = sorted(values)
    n = len(values)
    if n == 0:
        return 0.0
    mid = n // 2
    return values[mid] if n % 2 else 0.5 * (values[mid - 1] + values[mid])


def summarize(name, windows):
    """Печать сводки по окнам одного типа. Машинный кадр нормализуем по
    медиане, а не по среднему: отдельные просадки mach искают среднее."""
    valid = [w for w in windows if w.get("mach")]
    if not valid:
        print("%s: нет ни одного окна с mach > 0" % name)
        return

    mach = median([w["mach"] for w in valid])
    print("== %s ==" % name)
    print("   окон: %d, медианный mach: %.1f кадров/с (ном. 50)"
          % (len(valid), mach))

    per_field = {}
    for field in PERF_FIELDS + SND_FIELDS:
        vals = [w[field] for w in valid if w.get(field) is not None]
        if not vals:
            continue
        total = sum(vals)
        per_field[field] = (total, median(vals))

    print("   %-8s %10s %10s %8s  %s"
          % ("поле", "сумма,с", "мс/кадр", "% 20мс", ""))
    for field in PERF_FIELDS + SND_FIELDS:
        if field not in per_field:
            continue
        total, med = per_field[field]
        if field in ("loop", "mach", "samples", "aysteps"):
            print("   %-8s %10.0f %10s %8s  %s" % (field, med, "-", "-",
                                                  "счётчик"))
            continue
        per_frame = med / mach
        share = 100.0 * per_frame / 20.0
        note = "" if field in EXACT else "оценка"
        print("   %-8s %10.2f %10.3f %7.1f%%  %s"
              % (field, total, per_frame, share, note))

    def per_frame_of(field):
        total, med = per_field.get(field, (0, 0.0))
        return med / mach

    busy = sum(per_frame_of(f) for f in ("exec", "snd", "render", "fastfb"))
    print("   занято на кадр: exec+snd+render+fastfb = %.3f мс (%.1f%% от 20 мс)"
          % (busy, 100.0 * busy / 20.0))

    if "cpu" in per_field and "fill" in per_field:
        s = per_frame_of("cpu") + per_frame_of("fill") + per_frame_of("fastfb")
        e = per_frame_of("exec")
        verdict = "сходится" if s <= e + 1e-6 else "ПРЕВЫШАЕТ exec - некорректно"
        print("   проверка: cpu+fill+fastfb = %.3f мс vs exec = %.3f мс -> %s"
              % (s, e, verdict))
    print()


def main(argv):
    args = argv[1:]
    if not args:
        default = os.path.join(GAME_DIR, "perf.log")
        args = [default] if os.path.exists(default) else []
    if not args:
        print(__doc__)
        print("perf.log не найден: укажите путь аргументом")
        return 1
    for path in args:
        perf, snd = parse(path)
        print("### %s" % path)
        if not perf:
            print("   нет PERF-строк (это AUTOSELECT-сборка?)\n")
            continue
        summarize("PERF", perf)
        if snd:
            summarize("SND", snd)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
