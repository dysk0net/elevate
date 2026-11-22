#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace fx
{
constexpr float twoPi = 6.28318530718f;

class DelayLine
{
public:
    void prepare(int maxSamples) { buf.assign((size_t) maxSamples + 4, 0.0f); idx = 0; }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    void push(float x) { idx = (idx + 1) % (int) buf.size(); buf[(size_t) idx] = x; }
    float read(float d) const
    {
        const int n = (int) buf.size();
        d = std::clamp(d, 1.0f, (float) (n - 3));
        float p = (float) idx - d; while (p < 0.0f) p += (float) n;
        const int i0 = (int) p, i1 = (i0 + 1) % n; const float fr = p - (float) i0;
        return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }
private:
    std::vector<float> buf; int idx = 0;
};

struct Phase
{
    float p = 0;
    float tick(float hz, double sr) { p += hz / (float) sr; p -= std::floor(p); return p; }
};

//==============================================================================
class Chorus
{
public:
    void prepare(double sr) { sampleRate = sr; dl[0].prepare((int) (sr * 0.05)); dl[1].prepare((int) (sr * 0.05)); }
    void process(float* l, float* r, int n, bool on, bool stereo, float intensity)
    {
        if (! on) return;
        for (int i = 0; i < n; ++i)
        {
            const float ph = lfo.tick(0.45f + intensity * 0.6f, sampleRate);
            const float depth = (1.5f + 5.0f * intensity) * 0.001f * (float) sampleRate;
            const float base = 0.011f * (float) sampleRate;
            const float inL = l[i], inR = r[i];
            dl[0].push(inL); dl[1].push(inR);
            const float mL = std::sin(twoPi * ph), mR = std::sin(twoPi * (ph + (stereo ? 0.5f : 0.0f)));
            const float wl = dl[0].read(base + depth * mL), wr = dl[1].read(base + depth * mR);
            const float wet = 0.5f + 0.4f * intensity;
            l[i] = inL * 0.75f + wl * wet;
            r[i] = inR * 0.75f + wr * wet;
        }
    }
private:
    double sampleRate = 44100; DelayLine dl[2]; Phase lfo;
};

//==============================================================================
class ModFx
{
public:
    enum Mode { Phaser, FlangePos, FlangeNeg, Warble };

    void prepare(double sr) { sampleRate = sr; for (auto& d : dl) d.prepare((int) (sr * 0.03)); reset(); }
    void reset() { for (auto& c : ap) for (auto& s : c) s = 0; fb[0] = fb[1] = 0; for (auto& d : dl) d.clear(); }

    void process(float* l, float* r, int n, bool on, int mode, float rate, float intensity)
    {
        if (! on) return;
        float* ch[2] { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float ph = lfo.tick(rate, sampleRate);
            const float tri = 0.5f - 0.5f * std::cos(twoPi * ph);          // 0..1 smooth sweep
            for (int c = 0; c < 2; ++c)
            {
                const float in = ch[c][i];
                const float off = c == 1 ? 0.12f : 0.0f;                     // slight stereo offset
                const float tc = 0.5f - 0.5f * std::cos(twoPi * (ph + off));
                float out = in;
                switch (mode)
                {
                    case Phaser:
                    {
                        const float f = 250.0f * std::pow(16.0f, tc);          // ~250 Hz .. 4 kHz sweep
                        const float t = std::tan(3.14159265f * f / (float) sampleRate);
                        const float a = (t - 1.0f) / (t + 1.0f);
                        float x = in + fb[c] * 0.6f * intensity;
                        for (int s = 0; s < 4; ++s)
                        { const float y = a * x + ap[c][s]; ap[c][s] = x - a * y; x = y; }
                        fb[c] = x;
                        out = 0.5f * in + 0.5f * x * (0.6f + 0.4f * intensity);
                        break;
                    }
                    case FlangePos: case FlangeNeg:
                    {
                        const float d = (0.0006f + 0.004f * tc) * (float) sampleRate;
                        const float sign = mode == FlangePos ? 1.0f : -1.0f;
                        const float w = dl[c].read(d);
                        dl[c].push(in + w * sign * 0.8f * intensity);
                        out = 0.6f * in + 0.6f * w;
                        break;
                    }
                    default:  // Warble: wow + flutter pitch wobble
                    {
                        flutter += (nextNoise() - flutter) * 0.0008f;
                        const float d = (0.004f + 0.0025f * intensity * std::sin(twoPi * ph)
                                         + 0.0015f * intensity * flutter) * (float) sampleRate;
                        dl[c].push(in);
                        out = dl[c].read(d);
                        break;
                    }
                }
                ch[c][i] = out;
            }
            (void) tri;
        }
    }
private:
    float nextNoise() { seed = seed * 1664525u + 1013904223u; return (float) (int) seed / 2147483648.0f; }
    double sampleRate = 44100; DelayLine dl[2]; Phase lfo;
    float ap[2][4] {}, fb[2] {}, flutter = 0; unsigned seed = 7u;
};

//==============================================================================
class SpaceFx
{
public:
    enum Mode { Echo, Spring, Hall };

    void prepare(double sr)
    {
        sampleRate = sr;
        for (auto& d : echo) d.prepare((int) (sr * 2.5));
        for (auto& d : spring) d.prepare((int) (sr * 0.2));
        for (auto& c : sAp) for (auto& d : c) d.prepare((int) (sr * 0.02));
        hall.reset(); hall.setSampleRate(sr);
        for (auto& d : echo) d.clear();
        lp[0] = lp[1] = sLp[0] = sLp[1] = 0;
    }

    void process(float* l, float* r, int n, float amount, bool sync, int mode, float time, float fbTone, double bpm)
    {
        if (amount < 0.001f) return;
        const float sr = (float) sampleRate;

        if (mode == Hall)
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.55f + 0.4f * time; p.damping = 1.0f - fbTone; p.wetLevel = amount * 0.5f;
            p.dryLevel = 1.0f - amount * 0.35f; p.width = 1.0f; p.freezeMode = 0.0f;
            hall.setParameters(p);
            hall.processStereo(l, r, n);
            return;
        }

        if (mode == Echo)
        {
            float delaySamples;
            if (sync)
            {
                static const float beats[] { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
                const float b = beats[std::clamp((int) (time * 4.99f), 0, 4)];
                delaySamples = b * 60.0f / (float) std::max(1.0, bpm) * sr;
            }
            else delaySamples = (0.04f * std::pow(25.0f, time)) * sr;   // 40 ms .. 1 s
            const float fbk = fbTone * 0.88f;
            for (int i = 0; i < n; ++i)
            {
                dSmooth += (delaySamples - dSmooth) * 0.0005f;           // tape-like pitch glide
                const float wl = echo[0].read(dSmooth), wr = echo[1].read(dSmooth * 1.02f);
                lp[0] += (wl - lp[0]) * 0.35f; lp[1] += (wr - lp[1]) * 0.35f;  // dark, dubby repeats
                echo[0].push(std::tanh(l[i] + lp[1] * fbk));              // ping-pong feedback
                echo[1].push(std::tanh(r[i] + lp[0] * fbk));
                l[i] += wl * amount; r[i] += wr * amount;
            }
            return;
        }

        // Spring: allpass dispersion chirp + short recirculating line
        const float loopLen = (0.028f + 0.03f * time) * sr;
        const float tone = 0.15f + 0.8f * fbTone;
        for (int i = 0; i < n; ++i)
        {
            float in[2] { l[i], r[i] }, wet[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = in[c] + sFb[c] * 0.78f;
                for (int s = 0; s < 3; ++s)
                {
                    const float d = sAp[c][(size_t) s].read(40.0f + 23.0f * (float) s + 6.0f * (float) c);
                    const float y = -0.62f * x + d;
                    sAp[c][(size_t) s].push(x + 0.62f * y);
                    x = y;
                }
                spring[c].push(x);
                const float t = spring[c].read(loopLen * (c ? 1.07f : 1.0f));
                sLp[c] += (t - sLp[c]) * tone;
                sFb[c] = std::tanh(sLp[c]);
                wet[c] = sLp[c];
            }
            l[i] += wet[0] * amount * 1.4f; r[i] += wet[1] * amount * 1.4f;
        }
    }

private:
    double sampleRate = 44100;
    DelayLine echo[2], spring[2]; std::array<DelayLine, 3> sAp[2];
    float lp[2] {}, sLp[2] {}, sFb[2] {}, dSmooth = 22050.0f;
    juce::Reverb hall;
};
} // namespace fx
