# NF Saturator -- notes for Claude

Standalone repo for the **NF Saturator** plug-in (NF Audio Tools). Same family look as NF Glue / NF Q3 (own copies of the
look-and-feel and artwork).

## Mandatory: version consistency
**Single source of truth: `CMakeLists.txt` line 2** (`project(NFSaturator VERSION X.Y.Z ...)`). The UI footer reads
`JucePlugin_VersionString`; installer scripts must read the version from `CMakeLists.txt` (no manual edits).

## Owner's rules
- No GitHub Actions / no uploads: installers are built locally (DMG on the owner's Mac, `.exe` on the partner's PC).
  Never commit `.dmg`, `.exe`, `.pkg` or plug-in bundles.
- AAX must be PACE-signed (`wraptool`, PACE account `nenofernando`, local codesign identity
  "NF Audio Tools AAX Local Signing"); the AAX SDK is in `~/Documents`. No Apple Developer account for now.
- Identifiers: bundle `com.nfaudiotools.nfsaturator`, manufacturer `Nfat`, plug-in code `Nfsa`.
- Never use console/brand names (Neve, SSL, API...) in the product: the valves emulate the CHARACTER of circuit types
  (TUBE / IRON / SOLID) and must never be described as replicas of a specific unit.

## Design notes
- Fixed 1200x400 layout scaled uniformly; Drive left, three valves centre, Output right (knobs same size). No meter:
  the valve glow follows the signal (`driveActivity`).
- DSP is JUCE-free in `Source/DSP/SaturatorCore.h` (tested in `Tests/SaturatorTests.cpp`); the plug-in runs it at 4x
  oversampling (latency reported). Drive is level-compensated with a table measured with the same stages.
