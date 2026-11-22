#pragma once
#include "Sequencer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Gentle, musical randomisation for sequencer patterns.
// "chaos" (0..1) is both the chance that each step is touched AND how far/wild the change is,
// so low settings nudge an existing pattern and high settings rewrite it.
namespace seqrand
{
struct Rng
{
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float next() { return (float) (u() >> 8) / 16777216.0f; }
    int nextInt(int n) { return std::min(n - 1, (int) (next() * (float) n)); }
};

struct Pattern
{
    bool gateOn[16], pitchOn[16];
    float gateLen[16], vel[16];
    int pitch[16], oct[16];
};

inline void mutate(Pattern& p, bool modSeq, int scale, float chaos,
                   bool doGate, bool doPitch, bool doVel, Rng& rng)
{
    const float a = std::clamp(chaos, 0.0f, 1.0f);

    // Pitches the melody may use: the chosen scale, or minor pentatonic when the scale is Off.
    std::vector<int> cands;
    if (! modSeq)
        for (int s = -12; s <= 12; ++s)
        {
            const int pc = ((s % 12) + 12) % 12;
            const bool ok = scale > 0 ? Sequencer::quantise(s, scale) == s
                                      : (pc == 0 || pc == 3 || pc == 5 || pc == 7 || pc == 10);
            if (ok) cands.push_back(s);
        }

    int prev = 0;
    for (int i = 0; i < 16; ++i)
    {
        if (doGate && rng.next() < a)
        {
            static const float lens[] { 0.5f, 0.5f, 0.75f, 1.0f, 0.25f };
            p.gateOn[i] = i == 0 || rng.next() < 1.0f - 0.3f * a;
            p.gateLen[i] = rng.next() < 0.2f * a * a ? 1.1f : lens[rng.nextInt(a > 0.6f ? 5 : 4)];  // 1.1 = tie
        }

        if (doPitch)
        {
            if (rng.next() < a)
            {
                p.pitchOn[i] = true;
                if (modSeq)      // random walk with a limited jump size
                {
                    const int L = 2 + (int) std::lround(8.0f * a);
                    p.pitch[i] = std::clamp(prev + rng.nextInt(2 * L + 1) - L, -12, 12);
                    p.oct[i] = 0;
                }
                else             // melodic walk: favours notes near the previous one and near the root
                {
                    std::vector<float> w; float tot = 0.0f;
                    for (int c : cands)
                    {
                        float ww = std::exp(-(float) std::abs(c - prev) / (1.5f + 6.0f * a))
                                 * std::exp(-(float) std::abs(c) / (3.0f + 9.0f * a));
                        if (c == 0) ww *= 1.5f;
                        w.push_back(ww); tot += ww;
                    }
                    float r = rng.next() * tot; size_t k = 0;
                    while (k + 1 < w.size() && r >= w[k]) { r -= w[k]; ++k; }
                    p.pitch[i] = cands[k];
                    p.oct[i] = (a > 0.7f && rng.next() < 0.06f) ? (rng.nextInt(2) ? 1 : -1) : 0;
                }
            }
            prev = p.pitch[i];
        }

        if (doVel && rng.next() < a)
        {
            const float v = rng.next() < 0.15f * a ? 1.0f : 0.7f + (rng.next() - 0.5f) * (0.3f + 0.4f * a);
            p.vel[i] = std::clamp(v, 0.4f, 1.0f);
        }
    }
}
} // namespace seqrand
