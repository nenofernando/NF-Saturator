#include "../Source/DSP/CompLookup.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace nfsat;
constexpr double kFs = 192000.0;

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

static std::vector<double> runSine(const Stages& st, double drive, double amp, double f0, int settlePeriods = 40)
{
    const StageParams p = makeParams(st, kFs);
    ChannelState cs;
    const double pre = preGainLinear(drive);
    const int period = (int) std::round(kFs / f0);
    std::vector<double> out;
    for (int n = 0; n < period * (settlePeriods + 20); ++n)
    {
        const double y = processStages(amp * std::sin(2 * kPi * f0 * (double) n / kFs) * pre, p, cs);
        if (n >= period * settlePeriods) out.push_back(y);
    }
    return out;
}

static double thdRatio(const std::vector<double>& y, double f0)
{
    double h = 0; for (int k = 2; k <= 8; ++k) { const double m = harmonic(y, f0, k); h += m * m; }
    return std::sqrt(h) / harmonic(y, f0, 1);
}

static Stages only(bool t, bool i, bool s, double wt = 0, double wi = 0, double ws = 0)
{ Stages st; st.tube = t; st.iron = i; st.solid = s; st.tubeAmt = wt; st.ironAmt = wi; st.solidAmt = ws; return st; }

int main()
{
    const Stages tube = only(1, 0, 0), iron = only(0, 1, 0), solid = only(0, 0, 1), none = only(0, 0, 0);

    // Drive 0 is nearly transparent
    for (auto st : { tube, iron, solid })
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

    // More drive = more distortion, for every valve
    for (auto st : { tube, iron, solid })
        assert(thdRatio(runSine(st, 8.0, 0.3, 200.0), 200.0) > thdRatio(runSine(st, 2.0, 0.3, 200.0), 200.0));

    // No valve on: passes the signal (only the DC blocker) at 1 kHz
    {
        auto y = runSine(none, 4.0, 0.2, 1000.0);
        const double expected = 0.2 * preGainLinear(4.0);
        assert(std::abs(harmonic(y, 1000.0, 1) - expected) < 0.01 * expected);
    }

    // ---- WARMTH (drag up on a valve): each valve changes character in its own way, monotonically
    {
        // TUBE: 2nd harmonic grows with warmth
        double prev = -1.0;
        for (double w : { 0.0, 0.25, 0.5, 0.75, 1.0 })
        {
            auto y = runSine(only(1, 0, 0, w), 6.0, 0.3, 1000.0);
            const double h2 = harmonic(y, 1000.0, 2) / harmonic(y, 1000.0, 1);
            assert(h2 > prev); prev = h2;
        }
        // IRON: low-band distortion grows, top end rolls off (10 kHz gets quieter)
        const double lowBase = thdRatio(runSine(only(0, 1, 0, 0, 0.0), 6.0, 0.3, 60.0), 60.0);
        const double lowHot  = thdRatio(runSine(only(0, 1, 0, 0, 1.0), 6.0, 0.3, 60.0), 60.0);
        assert(lowHot > 1.2 * lowBase);
        const double hfBase = harmonic(runSine(only(0, 1, 0, 0, 0.0), 0.0, 0.1, 10000.0), 10000.0, 1);
        const double hfHot  = harmonic(runSine(only(0, 1, 0, 0, 1.0), 0.0, 0.1, 10000.0), 10000.0, 1);
        assert(hfHot < 0.8 * hfBase);
        // IRON head bump: small-signal gain at 70 Hz rises by >= 3 dB with warmth, while 1 kHz stays put
        {
            auto amp70 = [&](double w, double f) { return harmonic(runSine(only(0, 1, 0, 0, w), 0.0, 0.01, f, 100), f, 1); };
            const double bump = 20.0 * std::log10(amp70(1.0, 70.0) / amp70(0.0, 70.0));
            const double mid  = 20.0 * std::log10(amp70(1.0, 1000.0) / amp70(0.0, 1000.0));
            assert(bump > 3.0 && bump < 6.5);
            assert(std::abs(mid) < 0.6);
        }
        // SOLID: warmth adds 2nd harmonic and rounds off the top (less 5th/7th at high drive)
        auto s0 = runSine(only(0, 0, 1, 0, 0, 0.0), 10.0, 0.5, 1000.0);
        auto s1 = runSine(only(0, 0, 1, 0, 0, 1.0), 10.0, 0.5, 1000.0);
        assert(harmonic(s1, 1000.0, 2) / harmonic(s1, 1000.0, 1) > harmonic(s0, 1000.0, 2) / harmonic(s0, 1000.0, 1) + 0.005);
        assert(harmonic(s1, 1000.0, 7) / harmonic(s1, 1000.0, 1) < harmonic(s0, 1000.0, 7) / harmonic(s0, 1000.0, 1));
    }

    // ---- Level compensation table
    // (a) the table matches a live measurement at grid points (catches a stale CompTableData.h)
    for (int t : { 0, 2, 4, 5 }) for (int i : { 0, 3, 5 }) for (int s : { 1, 4, 5 })
        for (double drive : { 0.0, 5.0, 10.0 })
        {
            Stages st = only(t < 5, i < 5, s < 5, t < 5 ? t * 0.25 : 0.0, i < 5 ? i * 0.25 : 0.0, s < 5 ? s * 0.25 : 0.0);
            const double live = measureCompensationDb(drive, st);
            const double table = 20.0 * std::log10(compensationGain(drive, st));
            assert(std::abs(live - table) < 0.05);
        }
    // (b) between grid points (any warmth, any drive) the error stays small
    {
        std::srand(7);
        for (int k = 0; k < 40; ++k)
        {
            Stages st = only(std::rand() & 1, std::rand() & 1, std::rand() & 1, (std::rand() % 101) / 100.0, (std::rand() % 101) / 100.0, (std::rand() % 101) / 100.0);
            const double drive = (std::rand() % 101) / 10.0;
            const double live = measureCompensationDb(drive, st);
            const double table = 20.0 * std::log10(compensationGain(drive, st));
            assert(std::abs(live - table) < 0.6);
        }
    }

    // ---- Stability at extremes (everything hot), and no DC
    {
        const Stages hot = only(1, 1, 1, 1.0, 1.0, 1.0);
        const StageParams p = makeParams(hot, kFs); ChannelState cs;
        for (int n = 0; n < 192000 * 2; ++n)
        {
            const double x = 10.0 * std::sin(2 * kPi * 50.0 * (double) n / kFs) * ((n / 4000) % 2 ? 1.0 : 0.0);
            const double y = processStages(x * preGainLinear(10.0), p, cs);
            assert(std::isfinite(y) && std::abs(y) < 10.0);
        }
        ChannelState c2; double dc = 0;
        for (int n = 0; n < 192000; ++n) dc = processStages(0.0, p, c2);
        assert(std::abs(dc) < 1.0e-9);
        // tube offset removed: long tone, mean ~ 0
        const StageParams pt = makeParams(only(1, 0, 0, 1.0), kFs); ChannelState cs3; double mean = 0; int cnt = 0;
        for (int n = 0; n < 192000 + 19200; ++n)
        {
            const double y = processStages(0.5 * std::sin(2 * kPi * 1000.0 * (double) n / kFs) * preGainLinear(8.0), pt, cs3);
            if (n >= 192000) { mean += y; ++cnt; }
        }
        assert(std::abs(mean / cnt) < 2.0e-3);
    }

    std::cout << "NF Saturator DSP tests passed\n";
}
