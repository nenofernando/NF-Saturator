#include "../Source/DSP/SaturatorCore.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace nfsat;
constexpr double kFs = 192000.0;
constexpr double kPi = 3.14159265358979;

// magnitude of harmonic h of a periodic signal (whole number of periods in the window)
static double harmonic(const std::vector<double>& y, double f0, int h)
{
    double re = 0, im = 0;
    for (size_t n = 0; n < y.size(); ++n)
    {
        const double ph = 2 * kPi * f0 * h * (double) n / kFs;
        re += y[n] * std::cos(ph); im += y[n] * std::sin(ph);
    }
    return 2.0 * std::sqrt(re * re + im * im) / (double) y.size();
}

static std::vector<double> runSine(const Stages& st, double drive, double amp, double f0)
{
    SaturatorCore core; core.prepare(kFs);
    ChannelState cs;
    const double pre = preGainLinear(drive);
    const int period = (int) std::round(kFs / f0);
    const int settle = period * 40, measure = period * 20;
    std::vector<double> out;
    for (int n = 0; n < settle + measure; ++n)
    {
        const double x = amp * std::sin(2 * kPi * f0 * (double) n / kFs);
        const double y = core.processStages(x * pre, st, cs);
        if (n >= settle) out.push_back(y);
    }
    return out;
}

static double thdRatio(const std::vector<double>& y, double f0)
{
    double h = 0; for (int k = 2; k <= 8; ++k) { const double m = harmonic(y, f0, k); h += m * m; }
    return std::sqrt(h) / harmonic(y, f0, 1);
}

int main()
{
    Stages tube{true, false, false}, iron{false, true, false}, solid{false, false, true}, none{false, false, false};

    // Drive 0 is nearly transparent
    for (auto st : {tube, iron, solid})
        assert(thdRatio(runSine(st, 0.0, 0.126, 1000.0), 1000.0) < 0.01);

    // TUBE: even harmonics dominate; SOLID: odd harmonics dominate and no 2nd harmonic
    {
        auto y = runSine(tube, 6.0, 0.3, 1000.0);
        assert(harmonic(y, 1000.0, 2) > harmonic(y, 1000.0, 3));
        assert(harmonic(y, 1000.0, 2) / harmonic(y, 1000.0, 1) > 0.02);
        auto z = runSine(solid, 6.0, 0.5, 1000.0);
        assert(harmonic(z, 1000.0, 3) > 0.01 * harmonic(z, 1000.0, 1));
        assert(harmonic(z, 1000.0, 2) < 1.0e-3 * harmonic(z, 1000.0, 1));
    }

    // IRON: the low band distorts much more than the highs at the same level
    {
        const double lowThd = thdRatio(runSine(iron, 6.0, 0.3, 60.0), 60.0);
        const double highThd = thdRatio(runSine(iron, 6.0, 0.3, 2000.0), 2000.0);
        assert(lowThd > 3.0 * highThd);
    }

    // More drive = more distortion, for every stage
    for (auto st : {tube, iron, solid})
        assert(thdRatio(runSine(st, 8.0, 0.3, 200.0), 200.0) > thdRatio(runSine(st, 2.0, 0.3, 200.0), 200.0));

    // No stage on: passes the signal (only the DC blocker) at 1 kHz
    {
        auto y = runSine(none, 4.0, 0.2, 1000.0);
        const double expected = 0.2 * preGainLinear(4.0);
        assert(std::abs(harmonic(y, 1000.0, 1) - expected) < 0.01 * expected);
    }

    // Level compensation: the reference programme comes out at the same RMS (+-0.5 dB) for all combos / drives
    {
        SaturatorCore core; core.prepare(kFs);
        for (int combo = 0; combo < 8; ++combo)
            for (double drive : {0.0, 2.5, 5.0, 7.5, 10.0})
            {
                Stages st; st.tube = combo & 1; st.iron = combo & 2; st.solid = combo & 4;
                ChannelState cs; double inE = 0, outE = 0;
                const double pre = preGainLinear(drive), comp = compensationGain(drive, st);
                for (int n = 0; n < 19200 + 38400; ++n)
                {
                    const double in = referenceSample(n, kFs);
                    const double out = core.processStages(in * pre, st, cs) * comp;
                    if (n >= 19200) { inE += in * in; outE += out * out; }
                }
                const double db = 10.0 * std::log10(outE / inE);
                assert(std::abs(db) < 0.5);
            }
    }

    // Stability at extremes, and no DC
    {
        SaturatorCore core; core.prepare(kFs);
        Stages all{true, true, true}; ChannelState cs; double sum = 0; int cnt = 0;
        for (int n = 0; n < 192000 * 2; ++n)
        {
            const double x = 10.0 * std::sin(2 * kPi * 50.0 * (double) n / kFs) * ((n / 4000) % 2 ? 1.0 : 0.0);
            const double y = core.processStages(x * preGainLinear(10.0), all, cs);
            assert(std::isfinite(y) && std::abs(y) < 10.0);
            if (n > 192000) { sum += y; ++cnt; }
        }
        (void) sum; (void) cnt;
        ChannelState c2; double dc = 0;
        for (int n = 0; n < 192000; ++n) dc = core.processStages(0.0, all, c2);
        assert(std::abs(dc) < 1.0e-9);
        // tube offset removed: long tone, mean ~ 0
        SaturatorCore c3; c3.prepare(kFs); ChannelState cs3; double mean = 0; int cnt3 = 0;
        for (int n = 0; n < 192000 + 19200; ++n)
        {
            const double y = c3.processStages(0.5 * std::sin(2 * kPi * 1000.0 * (double) n / kFs) * preGainLinear(8.0), Stages{true, false, false}, cs3);
            if (n >= 192000) { mean += y; ++cnt3; }
        }
        mean /= (double) cnt3;
        assert(std::abs(mean) < 2.0e-3);
    }

    std::cout << "NF Saturator DSP tests passed\n";
}
