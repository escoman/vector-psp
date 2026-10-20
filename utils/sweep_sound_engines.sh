#!/bin/bash
#
# sweep_sound_engines.sh - прогон sound_engine = off/new/old по очереди в
# PPSSPP (AUTOSELECT-сборка, ~60 с на вариант).
#
# Для каждого варианта переписывает config.ini в каталоге игры, запускает
# run_60sec_kill и складывает результаты в engine_results/<вариант>/:
#   perf.log          - FPS/CPU/тайминги по секундам
#   debug.log         - snd_cb fill/step/underrun + статистика
#                       (только при RECORD=1, debug-сборка)
#   psp_internal.wav  - звук на выходе генератора (только RECORD=1)
#   psp_callback.wav  - звук, отданный PSP audio (только RECORD=1)
#
# ROM берётся из autoselect.txt в каталоге игры - смените его перед прогоном,
# чтобы прогнать матрицу на другом ROM.
#
# Использование:
#   ./utils/sweep_sound_engines.sh              # perf-прогон (release)
#   RECORD=1 ./utils/sweep_sound_engines.sh     # прогон с записью WAV
#   ENGINES="off old" ./utils/sweep_sound_engines.sh   # только часть матрицы

set -u

HERE="$(cd "$(dirname "$0")/.." && pwd)"
GD="$HOME/snap/ppsspp-emu/common/.config/ppsspp/PSP/GAME/VECTOR06C"
OUT="$HERE/engine_results"
RECORD="${RECORD:-0}"
ENGINES="${ENGINES:-off new old}"

mkdir -p "$OUT"

run_one() {
    local NAME="$1" ENGINE="$2"

    echo "=== sound_engine=$NAME ==="
    cat > "$GD/config.ini" <<EOF
border = true
fps = true
fast_framebuffer = true
sound_record = $( [ "$RECORD" = "1" ] && echo true || echo false )
sound_buffer_ms = 40
sound_engine = $ENGINE
worker_priority = 0x20
main_priority = 0x18
EOF

    if [ "$RECORD" = "1" ]; then
        # старые записи не должны смешиваться с новыми
        rm -f "$GD/psp_internal.wav" "$GD/psp_callback.wav"
    fi
    rm -f "$GD/debug.log" "$GD/perf.log"

    RUN_SECONDS="${RUN_SECONDS:-65}" "$HERE/utils/run_60sec_kill" \
        "$GD/ppsspp_$NAME.log"

    local D="$OUT/$NAME"
    mkdir -p "$D"
    cp -f "$GD/perf.log" "$D/" 2>/dev/null
    cp -f "$GD/debug.log" "$D/" 2>/dev/null
    if [ "$RECORD" = "1" ]; then
        cp -f "$GD/psp_internal.wav" "$D/" 2>/dev/null
        cp -f "$GD/psp_callback.wav" "$D/" 2>/dev/null
    fi
    if [ -f "$D/perf.log" ]; then
        "$HERE/utils/summarize_perf.py" "$D/perf.log" | tee "$D/summary.txt"
    fi
}

for VARIANT in $ENGINES; do
    case "$VARIANT" in
        off)  run_one "off" "off" ;;
        new)  run_one "new" "new" ;;
        old)  run_one "old" "old" ;;
        *) echo "sweep_sound_engines: unknown variant '$VARIANT'" >&2 ;;
    esac
done

echo "results in $OUT"
