#pragma once
// Life Pedal Sim - DSP core.
//
// Signal model (inspired by the publicly described Sunn O))) x EarthQuaker "Life Pedal"):
//   [Distortion side]  octave-up (full-wave rectifier) blend -> RAT-style op-amp
//                      distortion (GBW limited, slew limited) -> diode clipper -> filter -> amplitude
//   [Boost side]       MOSFET-style "Magnitude" clean boost, can sit before or after the distortion
//
// This is a behavioural model built from public descriptions, not a schematic-exact copy.

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace lifepedal
{
constexpr float kTwoPi = 6.283185307179586f;

struct Params
{
    float magnitude  = 0.4f;   // 0..1  boost amount
    float octave     = 0.3f;   // 0..1  octave-up blend
    float distortion = 0.6f;   // 0..1  op-amp gain
    float filter     = 0.35f;  // 0..1  0 = bright, 1 = dark
    float amplitude  = 0.5f;   // 0..1  distortion level (0.5 ~ unity)
    float outputDb   = 0.0f;   // host-side trim
    bool  boostOn    = false;
    bool  distOn     = true;
    int   clip       = 0;      // 0 silicon, 1 germanium, 2 LED
    bool  boostPost  = true;   // boost after distortion (drives the amp) or before it
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct DiodeModel
{
    double Is, nVt, R;
    float norm;
};

inline DiodeModel diodeFor (int clip)
{
    switch (clip)
    {
        case 1:  return { 1.0e-6,  0.0336, 1000.0, 0.5f / 0.25f }; // germanium-like, ~0.23 V
        case 2:  return { 2.0e-18, 0.0517, 1000.0, 0.5f / 1.8f  }; // LED-like, ~1.8 V
        default: return { 2.5e-9,  0.0452, 1000.0, 0.5f / 0.6f  }; // silicon 1N914-like, ~0.6 V
    }
}

struct Coeffs
{
    float boostGain = 1, boostEng = 0, distEng = 1, octMix = 0;
    float g1 = 0, g2 = 0, opA = 0, toneA = 0, ampGain = 1, outGain = 1;
    DiodeModel diode { 2.5e-9, 0.0452, 1000.0, 0.5f / 0.6f };
    bool boostPost = true;
};

// Solves the diode-pair clipper (series 1k, anti-parallel diodes) with a monotone Newton iteration.
inline float diodeClip (float x, const DiodeModel& d)
{
    const double ax = std::fabs ((double) x);
    double y = std::min (ax, d.nVt * std::asinh (ax / (2.0 * d.Is * d.R)));

    // iterate to convergence (a fixed small count is not enough for the LED setting)
    for (int i = 0; i < 20; ++i)
    {
        const double e  = std::exp (std::min (y / d.nVt, 40.0));
        const double ei = 1.0 / e;
        const double g  = (ax - y) / d.R - d.Is * (e - ei);
        const double gp = -1.0 / d.R - d.Is / d.nVt * (e + ei);
        const double dy = g / gp;
        y = std::max (0.0, y - dy);
        if (std::fabs (dy) < 1.0e-9) break;
    }
    return (float) std::copysign (y, (double) x);
}

class Channel
{
public:
    void prepare (float fsOS)
    {
        dcIn.setCutoff (12.0f, fsOS);
        octHP.setCutoff (50.0f, fsOS);
        octLP.setCutoff (4000.0f, fsOS);
        hpLow.setCutoff (60.5f, fsOS);    // 560R + 4.7uF branch
        hpHigh.setCutoff (1539.0f, fsOS); // 47R + 2.2uF branch
        coupling.setCutoff (34.0f, fsOS); // 4.7uF into 1k
        boostDc.setCutoff (20.0f, fsOS);
        outHP.setCutoff (15.0f, fsOS);
        slewMax = 0.3e6f / fsOS;          // LM308-ish 0.3 V/us
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &octHP, &octLP, &hpLow, &hpHigh, &opLP, &coupling, &toneLP, &boostDc, &outHP })
            f->reset();
        slewPrev = 0.0f;
    }

    float process (float x, const Coeffs& c)
    {
        float v = dcIn.hp (x);

        if (! c.boostPost)
            v = boost (v, c);

        v = distortion (v, c);

        if (c.boostPost)
            v = boost (v, c);

        v *= c.outGain;
        if (! std::isfinite (v))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (v, -8.0f, 8.0f);
    }

    OnePole opLP, toneLP; // coefficients set per chunk

private:
    float boost (float v, const Coeffs& c)
    {
        const float u = v * c.boostGain * 0.5f;
        float y = 2.0f * (std::tanh (u + 0.06f) - 0.0599281f); // slight MOSFET-style asymmetry
        y = boostDc.hp (y);
        return v + c.boostEng * (y - v);
    }

    float distortion (float v, const Coeffs& c)
    {
        // analog octave: soft full-wave rectifier, DC removed, band limited
        const float eps = 0.03f;
        const float rect = std::sqrt (v * v + eps * eps) - eps;
        const float oct = octLP.lp (octHP.hp (rect)) * 2.0f;
        const float mixed = v * (1.0f - 0.6f * c.octMix) + oct * c.octMix;

        // RAT-style gain stage: two high-pass shaped feedback legs
        float y = mixed + c.g1 * hpLow.hp (mixed) + c.g2 * hpHigh.hp (mixed);
        y = opLP.lp (y); // gain-bandwidth limit

        // op-amp rails (+-4.5 V) and slew rate
        const float s = y * (1.0f / 4.5f);
        const float s2 = s * s;
        y = 4.5f * s / std::sqrt (std::sqrt (1.0f + s2 * s2));
        const float d = std::clamp (y - slewPrev, -slewMax, slewMax);
        slewPrev += d;
        y = slewPrev;

        y = coupling.hp (y);
        y = diodeClip (y, c.diode);
        y = toneLP.lp (y) * c.diode.norm * c.ampGain;
        y = outHP.hp (y); // output coupling cap: removes DC left by the asymmetric octave waveform

        return v + c.distEng * (y - v);
    }

    OnePole dcIn, octHP, octLP, hpLow, hpHigh, coupling, boostDc, outHP;
    float slewMax = 1.0f, slewPrev = 0.0f;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        fsOS = fs * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smMag, &smOct, &smDist, &smFilter, &smAmp, &smOut, &smBoost, &smDistOn })
            s->reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };
        set (smMag, p.magnitude);   set (smOct, p.octave);     set (smDist, p.distortion);
        set (smFilter, p.filter);   set (smAmp, p.amplitude);  set (smOut, p.outputDb);
        set (smBoost, p.boostOn ? 1.0f : 0.0f);
        set (smDistOn, p.distOn ? 1.0f : 0.0f);
        snap = false;

        const auto diode = diodeFor (p.clip);

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            const float mag = smMag.skip (len), oct = smOct.skip (len), dist = smDist.skip (len);
            const float flt = smFilter.skip (len), amp = smAmp.skip (len), outDb = smOut.skip (len);
            c.boostEng = smBoost.skip (len);
            c.distEng = smDistOn.skip (len);

            c.boostGain = std::pow (10.0f, (mag * 28.0f) / 20.0f);
            c.octMix = oct;
            const float rd = 100000.0f * std::pow (10.0f, 2.0f * dist - 2.0f);
            c.g1 = rd / 560.0f;
            c.g2 = rd / 47.0f;
            const float fcOp = std::min (1.0e6f / (1.0f + rd / 47.0f), 0.45f * fsOS);
            const float rf = std::max (0.0f, 100000.0f * (std::pow (10.0f, 2.0f * flt - 2.0f) - 0.01f) * 1.0101f);
            const float fcTone = std::min (1.0f / (kTwoPi * (1500.0f + rf) * 3.3e-9f), 0.45f * fsOS);
            const float aOp = 1.0f - std::exp (-kTwoPi * fcOp / fsOS);
            const float aTone = 1.0f - std::exp (-kTwoPi * fcTone / fsOS);
            const float ampLin = amp * 2.0f;
            c.ampGain = ampLin * ampLin;
            c.outGain = std::pow (10.0f, outDb / 20.0f);
            c.diode = diode;
            c.boostPost = p.boostPost;

            for (int ch = 0; ch < numCh; ++ch)
            {
                channels[(size_t) ch].opLP.a = aOp;
                channels[(size_t) ch].toneLP.a = aTone;
            }

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fs = 44100.0f, fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smMag, smOct, smDist, smFilter, smAmp, smOut, smBoost, smDistOn;
};
} // namespace lifepedal
