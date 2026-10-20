#include "sound_engine.h"

#include <cctype>

bool engine_from_name(const std::string & s, SoundEngine fallback,
                      SoundEngine & out)
{
    std::string t = s;
    for (size_t i = 0; i < t.size(); ++i) {
        t[i] = (char)tolower((unsigned char)t[i]);
    }
    if (t == "off") {
        out = SoundEngine::Off;
        return true;
    }
    /* "original" is the current name; "old" is kept as a legacy alias
     * so existing config.ini files keep working. */
    if (t == "original" || t == "old") {
        out = SoundEngine::Old;
        return true;
    }
    /* "optimized" is the current name; "new" is kept as a legacy alias. */
    if (t == "optimized" || t == "new") {
        out = SoundEngine::New;
        return true;
    }
    out = fallback;
    return false;
}

const char * engine_name(SoundEngine e)
{
    switch (e) {
        case SoundEngine::Off: return "off";
        case SoundEngine::Old: return "original";
        case SoundEngine::New: return "optimized";
        default:               return "optimized";
    }
}
