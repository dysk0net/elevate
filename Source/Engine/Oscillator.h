#pragma once
#include <cmath>
#include <algorithm>

// Band-limited (PolyBLEP) oscillator with a continuous shape control:
//   shape 0.00 -> triangle
//   shape 0.33 -> sawtooth
//   shape 0.66 -> 50% square
//   shape 1.00 -> narrow pulse
class Oscillator
{
public:
    void reset(float startPhase = 0.0f) { phase = startPhase; }
    float getPhase() const { return phase; }
    void setPhase(float p) { phase = p; }

    // Advances the oscillator by one sample at frequency hz and returns its output.
    // Sets wrapped = true when the phase wrapped this sample (used as a sync master).
    float process(float hz, float shape, double sampleRate, bool& wrapped)
    {
        const float dt = std::min(0.45f, (float) (hz / sampleRate));
        wrapped = false;
        const float out = render(phase, dt, shape);
        phase += dt;
        if (phase >= 1.0f) { phase -= 1.0f; wrapped = true; }
        return out;
    }

    static float polyBlep(float t, float dt)
    {
        if (t < dt)         { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    static float render(float ph, float dt, float shape)
    {
        shape = std::clamp(shape, 0.0f, 1.0f);
        const float tri = 4.0f * std::fabs(ph - 0.5f) - 1.0f;
        const float saw = 2.0f * ph - 1.0f - polyBlep(ph, dt);
        auto pulse = [&](float width)
        {
            float v = ph < width ? 1.0f : -1.0f;
            v += polyBlep(ph, dt);
            float t2 = ph - width; if (t2 < 0.0f) t2 += 1.0f;
            v -= polyBlep(t2, dt);
            return v;
        };

        const float region = shape * 3.0f;
        if (region < 1.0f)  return tri + (saw - tri) * region;
        if (region < 2.0f)  { const float x = region - 1.0f; return saw + (pulse(0.5f) - saw) * x; }
        const float x = region - 2.0f;
        return pulse(0.5f - 0.45f * x);
    }

private:
    float phase = 0.0f;
};
