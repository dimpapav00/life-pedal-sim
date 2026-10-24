// Offline sanity test for the Life Pedal Sim DSP core (no GUI needed).
#include "LifePedalDSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, h1, h2, h3; bool finite; };

static Result run (lifepedal::Params p, double amp, double f0 = 330.0, double fs = 48000.0)
{
    lifepedal::Engine eng;
    eng.prepare (fs);
    const int N = (int) fs, block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    out.reserve ((size_t) N);
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    // analyse the second half (steady state)
    std::vector<float> tail (out.begin() + N / 2, out.end());
    Result r { 0, 0, 0, 0, 0, 0, true };
    for (float v : tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / tail.size()); r.dc /= tail.size();
    r.h1 = goertzel (tail, f0, fs); r.h2 = goertzel (tail, 2 * f0, fs); r.h3 = goertzel (tail, 3 * f0, fs);
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-34s rms=%.3f peak=%.3f dc=%+.4f  f0=%.3f 2f0=%.3f 3f0=%.3f  %s\n", name, r.rms, r.peak, r.dc,
                 r.h1, r.h2, r.h3, r.finite ? "ok" : "NaN/Inf!");
}

int main()
{
    int failures = 0;
    auto check = [&failures] (bool cond, const char* msg) { if (! cond) { std::printf ("FAIL: %s\n", msg); ++failures; } };

    lifepedal::Params clean; clean.distOn = false; clean.boostOn = false;
    auto r0 = run (clean, 0.1);
    show ("bypass (0.1)", r0);
    check (std::fabs (r0.h1 - 0.1) < 0.01, "bypass should pass signal at unity");

    lifepedal::Params dist; dist.octave = 0.0f; dist.distortion = 0.6f;
    auto r1 = run (dist, 0.1); show ("dist only, 0.1 in", r1);
    auto r1b = run (dist, 1.0); show ("dist only, 1.0 in", r1b);
    check (r1.finite && r1b.finite, "distortion output finite");
    check (r1b.peak < 2.0, "distortion output bounded");
    check (r1.h3 > 0.01, "distortion should create odd harmonics");

    lifepedal::Params noOct = dist; noOct.distortion = 0.0f; noOct.amplitude = 0.5f;
    lifepedal::Params withOct = noOct; withOct.octave = 1.0f;
    auto r2 = run (noOct, 0.1); show ("low gain, octave 0", r2);
    auto r3 = run (withOct, 0.1); show ("low gain, octave 1", r3);
    check (r3.h2 > r2.h2 * 2.0, "octave knob should raise 2nd harmonic");

    for (int clip = 0; clip < 3; ++clip)
    {
        lifepedal::Params p = dist; p.clip = clip; p.distortion = 1.0f; p.amplitude = 1.0f; p.octave = 1.0f;
        char name[64]; std::snprintf (name, sizeof name, "max everything, clip=%d", clip);
        auto r = run (p, 1.0); show (name, r);
        check (r.finite && r.peak < 8.0 && std::fabs (r.dc) < 0.1, "max settings stable, no DC");
    }

    lifepedal::Params boost = clean; boost.boostOn = true; boost.magnitude = 1.0f;
    auto r4 = run (boost, 0.05); show ("boost max, 0.05 in", r4);
    check (r4.h1 > 0.5, "boost should add gain");

    lifepedal::Params pre = dist; pre.boostOn = true; pre.boostPost = false; pre.magnitude = 1.0f;
    auto r5 = run (pre, 0.3); show ("boost pre + dist", r5);
    check (r5.finite, "boost-pre chain finite");

    // sample-rate sweep
    for (double fs : { 44100.0, 96000.0 })
    {
        auto r = run (dist, 0.3, 330.0, fs);
        char name[64]; std::snprintf (name, sizeof name, "dist @ %.0f Hz", fs);
        show (name, r);
        check (r.finite, "finite at other sample rates");
    }

    // --- the diode clipper must match a high-precision (bisection) solution of the same equation
    for (int clip = 0; clip < 3; ++clip)
    {
        const auto d = lifepedal::diodeFor (clip);
        double worst = 0.0;
        for (double x = -20.0; x <= 20.0; x += 0.0137)
        {
            const double ax = std::fabs (x);
            double lo = 0.0, hi = ax;
            for (int i = 0; i < 200; ++i)
            {
                const double y = 0.5 * (lo + hi);
                const double g = (ax - y) / d.R - d.Is * (std::exp (std::min (y / d.nVt, 40.0)) - std::exp (std::min (-y / d.nVt, 40.0)));
                if (g > 0.0) lo = y; else hi = y;
            }
            worst = std::max (worst, std::fabs ((double) lifepedal::diodeClip ((float) x, d) - std::copysign (0.5 * (lo + hi), x)));
        }
        if (worst > 1.0e-3)
        {
            std::printf ("diode clipper clip=%d: max error %.6f V\n", clip, worst);
            check (false, "diode clipper matches the reference solution within 1 mV");
        }
    }

    // --- periodicity: 375 Hz at 48 kHz repeats every 128 samples; noise or limit cycles would break that
    for (int clip = 0; clip < 3; ++clip)
        for (float dist : { 0.3f, 1.0f })
            for (float oct : { 0.0f, 1.0f })
                for (double a : { 0.02, 0.5 })
                {
                    lifepedal::Params q; q.clip = clip; q.distortion = dist; q.octave = oct;
                    lifepedal::Engine eng; eng.prepare (48000.0);
                    const int N = 96000, block = 512;
                    juce::AudioBuffer<float> buf (2, block);
                    std::vector<float> y;
                    for (int pos = 0; pos < N; pos += block)
                    {
                        for (int i = 0; i < block; ++i)
                        {
                            const float v = (float) (a * std::sin (2.0 * M_PI * 375.0 * (pos + i) / 48000.0));
                            buf.setSample (0, i, v); buf.setSample (1, i, v);
                        }
                        eng.process (buf, q);
                        for (int i = 0; i < block; ++i) y.push_back (buf.getSample (0, i));
                    }
                    double e = 0, s = 0;
                    for (int i = 64000; i < N; ++i) { const double dd = y[(size_t) i] - y[(size_t) i - 128]; e += dd * dd; s += (double) y[(size_t) i] * y[(size_t) i]; }
                    const double res = s > 1e-12 ? std::sqrt (e / s) : 0.0;
                    if (res > 1.0e-3)
                    {
                        std::printf ("periodicity clip=%d dist=%.1f oct=%.1f amp=%.2f: residual %.2e\n", clip, dist, oct, a, res);
                        check (false, "output is periodic for a periodic input (no noise or limit cycles)");
                    }
                }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
