#pragma once
#include <cmath>
#include <algorithm>

// ADSR with exponential-style segments. stage() is exposed so the UI can light LEDs.
class Envelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr) { sampleRate = sr; }
    void setParams(float a, float d, float s, float r)
    { attack = a; decay = d; sustain = s; release = r; }

    void gate(bool on, bool retrigger = true)
    {
        if (on)
        {
            if (retrigger || state == Idle || state == Release) state = Attack;
        }
        else if (state != Idle)
            state = Release;
    }

    float next()
    {
        switch (state)
        {
            case Idle: level = 0.0f; break;
            case Attack:
                level += (1.3f - level) * coeff(attack);
                if (level >= 1.0f) { level = 1.0f; state = Decay; }
                break;
            case Decay:
                level += (sustain - level) * coeff(decay);
                if (level - sustain < 0.0005f) { level = sustain; state = Sustain; }
                break;
            case Sustain: level = sustain; break;
            case Release:
                level += (0.0f - level) * coeff(release);
                if (level < 0.0002f) { level = 0.0f; state = Idle; }
                break;
        }
        return level;
    }

    Stage stage() const { return state; }
    float value() const { return level; }
    bool active() const { return state != Idle; }
    void kill() { state = Idle; level = 0.0f; }

private:
    // One-pole coefficient so the segment covers ~99% of its travel in 'seconds'.
    float coeff(float seconds) const
    {
        seconds = std::max(seconds, 0.0005f);
        return 1.0f - std::exp(-4.6f / (float) (seconds * sampleRate));
    }

    double sampleRate = 44100.0;
    float attack = 0.01f, decay = 0.2f, sustain = 0.7f, release = 0.2f;
    float level = 0.0f;
    Stage state = Idle;
};
