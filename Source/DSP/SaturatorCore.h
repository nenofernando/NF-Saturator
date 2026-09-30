#pragma once
#include <algorithm>
#include <cmath>

// NF Saturator core (JUCE-free, so the unit tests and the table generator build it alone).
//
// Three "valves", each an emulation of the CHARACTER of a kind of analogue circuit (not of any specific unit):
//   TUBE  - triode-style: asymmetric soft saturation -> even harmonics (warmth)
//   IRON  - transformer-style: saturates the low band first + gentle core compression (body / weight)
//   SOLID - transistor / op-amp style: tight, symmetric, sharper knee -> odd harmonics (bite / presence)
// Any combination can be on; they run in series (IRON -> TUBE -> SOLID). Every stage has unity small-signal
// gain, and Drive is level-compensated (see CompLookup.h), so Drive changes the character, not the loudness.
//
// Each valve also has a "warmth" amount 0..1 (drag up on the valve): 0 = the base character,
// 1 = the hottest version of that character:
//   TUBE  : more bias / more gain  -> more asymmetry, more 2nd harmonic
//   IRON  : lower-frequency band saturates harder, more core saturation, top end rolls off (darker, heavier)
//   SOLID : the knee softens (p 4 -> 2) and a little asymmetry appears -> from "bite" towards "warm"
//
// The core is meant to run oversampled (the plug-in uses 4x): `fs` is the INTERNAL sample rate.
namespace nfsat
{
constexpr double kMaxDrive = 10.0;
constexpr double kDriveDbPerUnit = 3.0;   // Drive 0..10  ->  -12 .. +18 dB in front of the stages
constexpr double kDriveOffsetDb  = -12.0;
constexpr double kPi = 3.14159265358979;

inline double preGainLinear(double drive)
{
    return std::pow(10.0, (std::clamp(drive, 0.0, kMaxDrive) * kDriveDbPerUnit + kDriveOffsetDb) / 20.0);
}

struct Stages
{
    bool tube = true, iron = true, solid = false;
    double tubeAmt = 0.0, ironAmt = 0.0, solidAmt = 0.0;   // warmth of each valve, 0..1
};

struct StageParams
{
    bool tube = false, iron = false, solid = false;
    double ironCoeff = 0, ironGLow = 2.8, ironGCore = 0.4, ironHfCoeff = 0;
    double tubeK = 1.6, tubeBias = 0.18, tubeT0 = 0, tubeNorm = 1;
    double solidMix = 0, solidBias = 0, solidT0 = 0;
    double dcCoeff = 0;
};

struct ChannelState
{
    double ironLow = 0.0, ironHf = 0.0;   // IRON: low band and top-end roll-off
    double dcIn = 0.0, dcOut = 0.0;       // DC blocker
};

inline double solidShape(double x, double mix)
{
    const double x2 = x * x;
    const double p2 = x / std::sqrt(1.0 + x2);
    const double p4 = x / std::sqrt(std::sqrt(1.0 + x2 * x2));
    return p4 + mix * (p2 - p4);
}

inline StageParams makeParams(const Stages& st, double fs)
{
    StageParams p;
    p.tube = st.tube; p.iron = st.iron; p.solid = st.solid;
    const double w_i = std::clamp(st.ironAmt, 0.0, 1.0), w_t = std::clamp(st.tubeAmt, 0.0, 1.0), w_s = std::clamp(st.solidAmt, 0.0, 1.0);
    p.ironCoeff = 1.0 - std::exp(-2.0 * kPi * (120.0 + 100.0 * w_i) / fs);
    p.ironGLow = 2.8 + 2.2 * w_i;
    p.ironGCore = 0.4 + 0.3 * w_i;
    p.ironHfCoeff = 1.0 - std::exp(-2.0 * kPi * (60000.0 - 52000.0 * w_i) / fs);
    p.tubeK = 1.6 + 0.8 * w_t;
    p.tubeBias = 0.18 + 0.30 * w_t;
    p.tubeT0 = std::tanh(p.tubeK * p.tubeBias);
    p.tubeNorm = p.tubeK * (1.0 - p.tubeT0 * p.tubeT0);
    p.solidMix = w_s;
    p.solidBias = 0.06 * w_s;
    p.solidT0 = solidShape(p.solidBias, p.solidMix);
    p.dcCoeff = std::exp(-2.0 * kPi * 12.0 / fs);   // ~12 Hz high-pass (the tube stage creates offset)
    return p;
}

// One sample through the enabled stages; the input is already multiplied by the pre-gain.
inline double processStages(double x, const StageParams& p, ChannelState& s)
{
    if (p.iron)
    {
        s.ironLow += p.ironCoeff * (x - s.ironLow);
        const double high = x - s.ironLow;
        const double y = high + std::tanh(p.ironGLow * s.ironLow) / p.ironGLow;
        const double core = std::tanh(p.ironGCore * y) / p.ironGCore;
        s.ironHf += p.ironHfCoeff * (core - s.ironHf);
        x = s.ironHf;
    }
    if (p.tube)
        x = (std::tanh(p.tubeK * (x + p.tubeBias)) - p.tubeT0) / p.tubeNorm;
    if (p.solid)
        x = solidShape(x + p.solidBias, p.solidMix) - p.solidT0;
    const double y = x - s.dcIn + p.dcCoeff * s.dcOut;
    s.dcIn = x; s.dcOut = y;
    return y;
}

// Reference programme (100 Hz + 1 kHz + 4 kHz sines, -18 dBFS RMS) used to level-match Drive.
inline double referenceSample(int n, double fs)
{
    const double t = (double) n / fs;
    const double a = std::pow(10.0, -18.0 / 20.0) * std::sqrt(2.0 / 3.0) * std::sqrt(2.0);
    return a * (std::sin(2 * kPi * 100.0 * t) * 0.9 + std::sin(2 * kPi * 1000.0 * t) * 0.7 + std::sin(2 * kPi * 4000.0 * t) * 0.45) / 1.1;
}

// Measures the gain (dB) that makes the reference programme come out at the same RMS as it went in.
inline double measureCompensationDb(double drive, const Stages& st)
{
    constexpr double fs = 192000.0;
    const StageParams p = makeParams(st, fs);
    const double pre = preGainLinear(drive);
    ChannelState cs; double inE = 0.0, outE = 0.0;
    const int settle = 19200, measure = 38400;
    for (int n = 0; n < settle + measure; ++n)
    {
        const double in = referenceSample(n, fs);
        const double out = processStages(in * pre, p, cs);
        if (n >= settle) { inE += in * in; outE += out * out; }
    }
    return outE > 1.0e-18 ? std::clamp(10.0 * std::log10(inE / outE), -30.0, 32.0) : 0.0;
}
}
