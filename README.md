# NF Saturator

Simple valve-style saturator by **NF Audio Tools** (Nenno Fernando) for vocals, drums and everything else.
VST3, AU (macOS) and AAX (Pro Tools).

- **Drive** (0-10): how hard the "valves" are pushed. Level-compensated: Drive changes the character, not the loudness.
- **Three valves** (each on/off, any combination): **TUBE** (triode-style warmth, even harmonics), **IRON** (transformer-style
  weight, saturates the lows first), **SOLID** (transistor / op-amp style bite, odd harmonics). They emulate the *character*
  of these kinds of circuits, not any specific unit.
- **Output** (-12..+12 dB), Power, preset save/load. 4x oversampling.

![preview](Docs/preview.png)

## Quick VST3 build on macOS (to test in a DAW)
```
cmake -S . -B build -G Xcode
cmake --build build --config Release --target NFSaturator_VST3
```
Then copy `NF Saturator.vst3` (inside `build/NFSaturator_artefacts/Release/VST3/`) to `/Library/Audio/Plug-Ins/VST3/`.

## Tests
```
g++ -std=c++17 -Wall -Wextra Tests/SaturatorTests.cpp -o dsp_tests && ./dsp_tests
```
