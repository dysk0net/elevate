#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// LFO: triangle, square, stepped random (S&H), smooth random, with TILT asymmetry.
class Lfo
{
public:
    enum Shape { Triangle, Square, SampleHold, SmoothRandom };

    void setSampleRate(double sr) { sampleRate = sr; }
    void retrigger() { phase = 0.0f; }
    float getPhase() const { return phase; }
    void setPhase(float p) { phase = p - std::floor(p); }

    // Returns a bipolar value in [-1, 1].
    float next(float hz, int shape, float tilt)
    {
        phase += (float) (hz / sampleRate);
        if (phase >= 1.0f)
        {
            phase -= std::floor(phase);
            prevRand = targetRand;
            targetRand = rand01() * 2.0f - 1.0f;
        }

        // TILT moves the waveform "centre" left/right: warps phase around a pivot.
        const float pivot = std::clamp(0.5f + 0.45f * tilt, 0.05f, 0.95f);
        const float p = phase < pivot ? 0.5f * phase / pivot
                                      : 0.5f + 0.5f * (phase - pivot) / (1.0f - pivot);

        switch (shape)
        {
            case Triangle:     return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case Square:       return p < 0.5f ? 1.0f : -1.0f;
            case SampleHold:   return targetRand;
            default:           { const float s = p * p * (3.0f - 2.0f * p);
                                 return prevRand + (targetRand - prevRand) * s; }
        }
    }

private:
    float rand01()
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) / 16777216.0f;
    }

    double sampleRate = 44100.0;
    float phase = 0.0f, prevRand = 0.0f, targetRand = 0.0f;
    uint32_t seed = 12345u;
};
