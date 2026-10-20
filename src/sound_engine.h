#pragma once

#include <string>

/* Sound engine selector (config.ini: sound_engine, Config window).
 *
 *   Off  - no audio generation at all. The guest КР580ВИ53 still ticks
 *          (Board drives it from CPU time for IN 0x08..0x0B and for the
 *          interrupt timing), only Soundnik produces nothing. This is
 *          the zero point for measuring how much of the frame budget
 *          the sound subsystem eats.
 *   Old  - the reference kernel's pipeline (vector06sdl/v06c-emu):
 *          the 8253 is stepped once per sound clock and its OUT pin is
 *          sampled as a 1-clock snapshot, the AY is stepped at
 *          ~218 kHz and box-averaged over the output period, and the
 *          mixed stream is decimated by a box average. Semantics are
 *          kept verbatim so the two engines can be compared both by
 *          ear and by CPU cost.
 *   New  - the block pipeline: timestamped chip writes are queued as
 *          events and replayed once per frame against a chip mirror,
 *          with the timer integrated as a duty cycle over the sample
 *          period and the AY decimated by a windowed-sinc FIR.
 *
 * The post-filter kernels this module used to provide (the cubic /
 * gaussian / sinc reconstruction in the audio callback) are gone: the
 * reconstruction is fixed to the two-point linear one, and the quality
 * question now lives in the engine choice and its own filters. */
enum class SoundEngine
{
    Off,
    Old,
    New
};

/* config.ini string -> enum. Case-insensitive; an unknown value falls
 * back to `fallback` and returns false, so the caller can log it. */
bool engine_from_name(const std::string & s, SoundEngine fallback,
                      SoundEngine & out);
const char * engine_name(SoundEngine e);
