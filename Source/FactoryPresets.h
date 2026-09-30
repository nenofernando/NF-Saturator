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
    // Punch recipe: more Drive (saturation), IRON for weight, SOLID for attack, TUBE a bit less, and a parallel MIX
    // (dry transient + saturated body) where it helps. Output is level-compensated, so it stays near 0 dB.
    // category  name                   drive  tube   iron   solid   tubeW  ironW  solidW  out    mix
    { "Voice",   "Vocal Warm",           5.0f, true,  true,  false,  0.30f, 0.25f, 0.0f,  0.0f,  1.0f },
    { "Voice",   "Vocal Silk",           3.5f, true,  false, true,   0.20f, 0.0f,  0.15f, 0.0f,  0.85f },
    { "Voice",   "Vocal Presence",       5.5f, true,  false, true,   0.15f, 0.0f,  0.45f, 0.0f,  0.80f },
    { "Voice",   "Vocal Fat",            6.5f, true,  true,  false,  0.40f, 0.60f, 0.0f, -0.5f,  1.0f },
    { "Voice",   "Vocal Rock",           7.5f, true,  true,  true,   0.55f, 0.15f, 0.50f, -1.0f, 1.0f },
    { "Voice",   "Vocal Pop",            4.5f, true,  false, true,   0.25f, 0.0f,  0.25f, 0.0f,  0.80f },
    { "Voice",   "Male Voice",           5.5f, true,  true,  true,   0.30f, 0.45f, 0.20f, 0.0f,  1.0f },
    { "Voice",   "Female Voice",         4.5f, true,  false, true,   0.20f, 0.0f,  0.30f, 0.0f,  0.85f },
    { "Voice",   "Vocal Parallel",       8.0f, true,  true,  true,   0.35f, 0.35f, 0.30f, 0.0f,  0.45f },
    { "Voice",   "Vocal Radio",          7.0f, true,  true,  true,   0.20f, 0.35f, 0.50f, 0.0f,  1.0f },
    { "Mix",     "Mix Glue",             3.5f, true,  true,  true,   0.15f, 0.30f, 0.10f, 0.0f,  0.90f },
    { "Mix",     "Mix Warm",             4.5f, true,  true,  false,  0.30f, 0.40f, 0.0f,  0.0f,  0.90f },
    { "Mix",     "Mix Punch",            6.0f, false, true,  true,   0.0f,  0.40f, 0.40f, 0.0f,  0.85f },
    { "Mix",     "Mix Tape Weight",      4.5f, true,  true,  false,  0.30f, 0.75f, 0.0f,  0.0f,  1.0f },
    { "Mix",     "Mix Bus Air",          3.5f, true,  false, true,   0.15f, 0.0f,  0.40f, 0.0f,  0.80f },
    { "Mix",     "Mix Heavy",            7.0f, true,  true,  true,   0.45f, 0.60f, 0.35f, -1.0f, 1.0f },
    { "Master",  "Master Gentle",        2.5f, true,  true,  false,  0.10f, 0.15f, 0.0f,  0.0f,  0.90f },
    { "Master",  "Master Warm",          3.5f, true,  true,  false,  0.25f, 0.40f, 0.0f,  0.0f,  0.85f },
    { "Master",  "Master Tight",         3.5f, false, true,  true,   0.0f,  0.25f, 0.30f, 0.0f,  0.80f },
    { "Guitar",  "Guitar Crunch",        8.0f, true,  false, true,   0.55f, 0.0f,  0.45f, -1.5f, 1.0f },
    { "Guitar",  "Guitar Fat",           6.5f, true,  true,  true,   0.35f, 0.50f, 0.20f, -0.5f, 1.0f },
    { "Drums",   "Drums Push",           8.0f, true,  true,  true,   0.0f,  0.0f,  0.0f,  0.0f,  1.0f },
    { "Drums",   "Drums Parallel",      10.0f, true,  true,  true,   0.30f, 0.40f, 0.30f, 0.0f,  0.55f },
    { "Drums",   "Drums Punch",          6.5f, false, true,  true,   0.0f,  0.55f, 0.50f, 0.0f,  1.0f },
    { "Drums",   "Drum Bus Glue",        5.0f, true,  true,  true,   0.25f, 0.60f, 0.25f, 0.0f,  0.90f },
    { "Drums",   "Kick & Bass Weight",   6.0f, true,  true,  true,   0.25f, 1.00f, 0.15f, 0.0f,  1.0f },
};
inline constexpr int kNumFactoryPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
}
