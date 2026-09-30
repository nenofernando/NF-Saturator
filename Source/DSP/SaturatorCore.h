#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// NF Saturator core (JUCE-free, so the unit tests build it alone).
//
// Three "valves", each an emulation of the CHARACTER of a kind of analogue circuit (not of any specific unit):
//   TUBE  - triode-style: asymmetric soft saturation -> even harmonics (warmth)
//   IRON  - transformer-style: saturates the low band first + gentle core compression (body / weight)
//   SOLID - transistor / op-amp style: tight, symmetric, sharper knee -> odd harmonics (bite / presence)
// Any combination can be on; they run in series (IRON -> TUBE -> SOLID). Every stage has unity small-signal
// gain, and Drive is level-compensated (a table measured with the very same stages), so Drive changes the
// character, not the loudness.
//
// The core is meant to run oversampled (the plug-in uses 4x): `fs` in prepare() is the INTERNAL sample rate.
namespace nfsat
{
constexpr double kMaxDrive = 10.0;
constexpr double kDriveDbPerUnit = 3.0;   // Drive 0..10  ->  -12 .. +18 dB in front of the stages
constexpr double kDriveOffsetDb  = -12.0;

inline double preGainLinear(double drive)
{
    return std::pow(10.0, (std::clamp(drive, 0.0, kMaxDrive) * kDriveDbPerUnit + kDriveOffsetDb) / 20.0);
}

struct Stages { bool tube = true, iron = true, solid = false;
    int index() const { return (tube ? 1 : 0) | (iron ? 2 : 0) | (solid ? 4 : 0); } };

struct ChannelState
{
    double ironLow = 0.0;          // one-pole low band used by IRON
    double dcIn = 0.0, dcOut = 0.0;// DC blocker
};

class SaturatorCore
{
public:
    void prepare(double internalSampleRate)
    {
        fs = internalSampleRate;
        ironCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979 * 120.0 / fs);   // ~120 Hz low band
        dcCoeff = std::exp(-2.0 * 3.14159265358979 * 12.0 / fs);           // ~12 Hz high-pass
    }

    // One sample through the enabled stages; input is already multiplied by the pre-gain.
    double processStages(double x, const Stages& st, ChannelState& s) const
    {
        if (st.iron)
        {
            // Transformer: the low band saturates first, the rest passes; then mild core saturation.
            s.ironLow += ironCoeff * (x - s.ironLow);
            const double high = x - s.ironLow;
            constexpr double gLow = 2.8;
            const double lowSat = std::tanh(gLow * s.ironLow) / gLow;
            const double y = high + lowSat;
            constexpr double gCore = 0.4;
            x = std::tanh(gCore * y) / gCore;
        }
        if (st.tube)
        {
            // Triode-like: biased tanh -> asymmetric clipping, unity small-signal gain, DC removed afterwards.
            constexpr double k = 1.6, bias = 0.18;
            const double t = std::tanh(k * (x + bias)) - std::tanh(k * bias);
            const double gain = k * (1.0 - std::tanh(k * bias) * std::tanh(k * bias));
            x = t / gain;
        }
        if (st.solid)
        {
            // Transistor / op-amp: symmetric, sharper knee (p = 4).
            const double x2 = x * x;
            x = x / std::sqrt(std::sqrt(1.0 + x2 * x2));
        }
        // DC blocker (the tube stage creates offset)
        const double y = x - s.dcIn + dcCoeff * s.dcOut;
        s.dcIn = x; s.dcOut = y;
        return y;
    }

    double fs = 192000.0;

private:
    double ironCoeff = 0.0, dcCoeff = 0.0;
};

// ---- Level compensation --------------------------------------------------------------------------
// gain that makes the RMS of a reference programme (100 Hz + 1 kHz + 4 kHz sines, -18 dBFS RMS) equal in and out.
constexpr int kCompSteps = 101;

inline double referenceSample(int n, double fs)
{
    constexpr double pi = 3.14159265358979;
    const double t = (double) n / fs;
    const double a = std::pow(10.0, -18.0 / 20.0) * std::sqrt(2.0 / 3.0) * std::sqrt(2.0);
    return a * (std::sin(2 * pi * 100.0 * t) * 0.9 + std::sin(2 * pi * 1000.0 * t) * 0.7 + std::sin(2 * pi * 4000.0 * t) * 0.45) / 1.1;
}

struct CompTable
{
    std::array<std::array<double, kCompSteps>, 8> gain{};   // [stage combination][drive step] linear gain
    CompTable()
    {
        SaturatorCore core; core.prepare(192000.0);
        const int settle = 19200, measure = 38400;
        for (int combo = 0; combo < 8; ++combo)
        {
            Stages st; st.tube = combo & 1; st.iron = combo & 2; st.solid = combo & 4;
            for (int i = 0; i < kCompSteps; ++i)
            {
                const double pre = preGainLinear(kMaxDrive * (double) i / (double)(kCompSteps - 1));
                ChannelState cs; double inE = 0.0, outE = 0.0;
                for (int n = 0; n < settle + measure; ++n)
                {
                    const double in = referenceSample(n, core.fs);
                    const double out = core.processStages(in * pre, st, cs);
                    if (n >= settle) { inE += in * in; outE += out * out; }
                }
                gain[(size_t) combo][(size_t) i] = outE > 1.0e-18 ? std::clamp(std::sqrt(inE / outE), 0.03, 40.0) : 1.0;
            }
        }
    }
};

inline const CompTable& compTable() { static const CompTable t; return t; }

inline double compensationGain(double drive, const Stages& st)
{
    const auto& g = compTable().gain[(size_t) st.index()];
    const double pos = std::clamp(drive, 0.0, kMaxDrive) / kMaxDrive * (double)(kCompSteps - 1);
    const int i = std::min((int) pos, kCompSteps - 2);
    const double f = pos - (double) i;
    return g[(size_t) i] * (1.0 - f) + g[(size_t) i + 1] * f;
}
}
