#pragma once
// Factory presets for NF Saturator (JUCE-free so the unit tests can validate every value).
// They are starting points, not tuned by ear: Drive is level-compensated, so Output stays near 0 dB.
namespace nfsat
{
struct FactoryPreset
{
    const char* category;
    const char* name;
    float drive;                       // 0..10
    bool tube, iron, solid;            // valves on/off
    float tubeWarm, ironWarm, solidWarm;   // 0..1
    float outputDb;                    // -12..+12
    float mix = 1.0f;                  // 0..1: 1 = all saturated, less = parallel with the dry signal
};

inline constexpr FactoryPreset kFactoryPresets[] = {
    // category  name                   drive  tube   iron   solid   tubeW  ironW  solidW  out
    { "Voice",   "Vocal Warm",           3.5f, true,  true,  false,  0.30f, 0.20f, 0.0f,  0.5f },
    { "Voice",   "Vocal Silk",           2.5f, true,  false, false,  0.15f, 0.0f,  0.0f,  0.0f },
    { "Voice",   "Vocal Presence",       4.0f, true,  false, true,   0.10f, 0.0f,  0.30f, 0.0f },
    { "Voice",   "Vocal Fat",            5.0f, true,  true,  false,  0.40f, 0.50f, 0.0f,  0.0f },
    { "Voice",   "Vocal Rock",           6.0f, true,  false, true,   0.50f, 0.0f,  0.40f, 0.0f },
    { "Voice",   "Vocal Pop",            3.0f, true,  false, true,   0.20f, 0.0f,  0.10f, 0.0f },
    { "Voice",   "Male Voice",           4.0f, true,  true,  false,  0.30f, 0.35f, 0.0f,  0.0f },
    { "Voice",   "Female Voice",         3.0f, true,  false, true,   0.20f, 0.0f,  0.20f, 0.0f },
    { "Voice",   "Vocal Parallel",       6.0f, true,  true,  true,   0.30f, 0.30f, 0.20f, 0.0f,  0.40f },
    { "Voice",   "Vocal Radio",          5.0f, true,  true,  true,   0.20f, 0.30f, 0.30f, 0.0f },
    { "Mix",     "Mix Glue",             2.0f, true,  true,  false,  0.10f, 0.20f, 0.0f,  0.0f },
    { "Mix",     "Mix Warm",             3.0f, true,  true,  false,  0.25f, 0.30f, 0.0f,  0.0f },
    { "Mix",     "Mix Punch",            3.5f, false, true,  true,   0.0f,  0.20f, 0.20f, 0.0f },
    { "Mix",     "Mix Tape Weight",      3.0f, true,  true,  false,  0.30f, 0.60f, 0.0f,  0.0f },
    { "Mix",     "Mix Bus Air",          2.0f, true,  false, true,   0.10f, 0.0f,  0.25f, 0.0f },
    { "Mix",     "Mix Heavy",            5.0f, true,  true,  true,   0.40f, 0.50f, 0.20f, -0.5f },
    { "Master",  "Master Gentle",        1.5f, true,  true,  false,  0.10f, 0.10f, 0.0f,  0.0f },
    { "Master",  "Master Warm",          2.5f, true,  true,  false,  0.20f, 0.30f, 0.0f,  0.0f },
    { "Master",  "Master Tight",         2.0f, false, true,  true,   0.0f,  0.15f, 0.15f, 0.0f },
    { "Guitar",  "Guitar Crunch",        6.0f, true,  false, true,   0.50f, 0.0f,  0.30f, -1.0f },
    { "Guitar",  "Guitar Fat",           4.5f, true,  true,  false,  0.30f, 0.40f, 0.0f,  0.0f },
    { "Drums",   "Drums Push",           8.0f, true,  true,  true,   0.0f,  0.0f,  0.0f,  0.0f },
    { "Drums",   "Drums Parallel",       9.0f, true,  true,  true,   0.20f, 0.30f, 0.20f, 0.0f,  0.45f },
    { "Drums",   "Drums Punch",          4.0f, false, true,  true,   0.0f,  0.40f, 0.30f, 0.0f },
    { "Drums",   "Drum Bus Glue",        3.0f, true,  true,  false,  0.20f, 0.50f, 0.0f,  0.0f },
    { "Drums",   "Kick & Bass Weight",   4.0f, true,  true,  false,  0.20f, 1.00f, 0.0f,  0.0f },
};
inline constexpr int kNumFactoryPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
}
