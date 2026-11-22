#pragma once
#include <cmath>
#include <algorithm>

// 24 dB/oct transistor-ladder style low-pass with input DRIVE, RESONANCE feedback and
// GROWL (cutoff modulated by the incoming signal), followed by a 12 dB high-pass.
class LadderFilter
{
public:
    void setSampleRate(double sr) { sampleRate = sr; reset(); }
    void reset() { for (auto& s : y) s = 0.0f; hp1 = hp2 = hpIn1 = hpIn2 = 0.0f; }

    float process(float in, float cutoffHz, float resonance, float drive, float growl, float hpHz)
    {
        // Drive: boosts the mixer output; high drive also tames self-oscillation.
        const float gain = 1.0f + drive * 6.0f;
        const float k = resonance * 4.05f / (1.0f + drive * 0.6f);

        // Growl: signal-dependent cutoff modulation.
        float fc = cutoffHz * (1.0f + growl * 0.8f * std::tanh(in * gain));
        fc = std::clamp(fc, 15.0f, (float) (sampleRate * 0.33));

        // Two sub-steps per sample for stability at high cutoff/resonance.
        const float g = 1.0f - std::exp((float) (-2.0 * 3.14159265358979 * fc / (sampleRate * 2.0)));
        float out = 0.0f;
        for (int n = 0; n < 2; ++n)
        {
            const float x = std::tanh(in * gain - k * y[3]);
            y[0] += g * (std::tanh(x)    - std::tanh(y[0]));
            y[1] += g * (std::tanh(y[0]) - std::tanh(y[1]));
            y[2] += g * (std::tanh(y[1]) - std::tanh(y[2]));
            y[3] += g * (std::tanh(y[2]) - std::tanh(y[3]));
            out = y[3];
        }

        // High-pass (two cascaded one-poles = 12 dB/oct).
        const float hc = std::exp((float) (-2.0 * 3.14159265358979 * std::clamp(hpHz, 20.0f, 8000.0f) / sampleRate));
        const float h1 = hc * (hp1 + out - hpIn1);  hpIn1 = out;  hp1 = h1;
        const float h2 = hc * (hp2 + h1 - hpIn2);   hpIn2 = h1;   hp2 = h2;
        const float hpOut = h2;

        // Makeup so drive adds saturation rather than just level.
        return (hpHz > 21.0f ? hpOut : out) / (1.0f + drive * 1.5f) * 1.6f;
    }

private:
    double sampleRate = 44100.0;
    float y[4] { 0, 0, 0, 0 };
    float hp1 = 0, hp2 = 0, hpIn1 = 0, hpIn2 = 0;
};
