#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

// Three-lane (gate / pitch / velocity), 16-step sequencer with per-lane lengths
// (polyrhythms), direction modes, swing, gate scale, legato ties and host sync.
// The sequencer never touches audio: it reports note events at sample offsets.
class Sequencer
{
public:
    static constexpr int numSteps = 16;
    enum Lane { Gate = 0, Pitch = 1, Vel = 2 };
    enum class Ev { NoteOn, NoteOff };
    enum Direction { Forward, Reverse, PingPong1, PingPong2, Random };

    struct Snapshot
    {
        bool on = false, retrig = true;
        int rateIdx = 3, dir = Forward;
        float swing = 0.0f, gateScale = 1.0f;
        int len[3] { 16, 16, 16 };
        bool stepOn[3][numSteps];
        float val[3][numSteps];          // gate length (>1 = legato) / semitones / velocity
        int oct[numSteps];
    };

    // Current step per lane, for the UI playhead (-1 when stopped).
    std::atomic<int> uiStep[3] { -1, -1, -1 };

    template <class Fn>
    void process(int numSamples, double bpm, double sr, bool hostPlaying, double hostPpq,
                 bool anyHeld, const Snapshot& s, Fn&& fn)
    {
        if (! s.on || ! anyHeld)
        {
            if (running) stop(0, fn);
            return;
        }

        static const double stepBeats[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double L = stepBeats[std::clamp(s.rateIdx, 0, 5)];
        const double beatsPerSample = std::max(1.0, bpm) / 60.0 / sr;
        const bool hostLocked = hostPlaying && ! s.retrig;

        if (! running)
        {
            running = true; gateActive = false; tieNext = false;
            if (hostLocked) { beatPos = hostPpq; nextTick = (long long) std::ceil(beatPos / L - 1e-9); }
            else            { beatPos = 0.0;     nextTick = 0; }
        }
        else if (hostLocked)
        {
            beatPos = hostPpq;
            const double tickTime = (double) nextTick * L;
            if (tickTime > beatPos + 2.0 * L || tickTime < beatPos - 2.0 * L)
                nextTick = (long long) std::ceil(beatPos / L - 1e-9);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            if (gateActive && beatPos >= gateOffBeat)
            {
                fn(i, Ev::NoteOff, 0, 0.0f, false);
                gateActive = false;
            }

            for (;;)
            {
                const double swingDelay = (nextTick & 1) ? (double) s.swing * 0.5 * L : 0.0;
                const double t = (double) nextTick * L + swingDelay;
                if (beatPos < t) break;
                trigger(i, nextTick, t, L, s, fn);
                ++nextTick;
            }
            beatPos += beatsPerSample;
        }
    }

    void reset() { running = false; gateActive = false; tieNext = false; for (auto& u : uiStep) u = -1; }

private:
    template <class Fn> void stop(int offset, Fn&& fn)
    {
        if (gateActive) fn(offset, Ev::NoteOff, 0, 0.0f, false);
        reset();
    }

    int position(int lane, long long tick, const Snapshot& s)
    {
        const int len = std::clamp(s.len[lane], 1, numSteps);
        const int t = (int) (tick % 100000);
        switch (s.dir)
        {
            case Reverse:   return len - 1 - (t % len);
            case PingPong1: { const int period = std::max(1, 2 * len - 2); const int m = t % period;
                              return m < len ? m : period - m; }
            case PingPong2: { const int period = 2 * len; const int m = t % period;
                              return m < len ? m : period - 1 - m; }
            case Random:    return (int) (nextRandom() % (uint32_t) len);
            default:        return t % len;
        }
    }

    template <class Fn>
    void trigger(int offset, long long tick, double startBeat, double L, const Snapshot& s, Fn&& fn)
    {
        const int pg = position(Gate, tick, s), pp = position(Pitch, tick, s), pv = position(Vel, tick, s);
        uiStep[Gate] = pg; uiStep[Pitch] = pp; uiStep[Vel] = pv;

        if (! s.stepOn[Gate][pg])                    // rest
        {
            if (gateActive) { fn(offset, Ev::NoteOff, 0, 0.0f, false); gateActive = false; }
            tieNext = false;
            return;
        }

        const float g = s.val[Gate][pg];
        const bool tie = g > 1.0f;
        const int semis = s.stepOn[Pitch][pp] ? (int) std::lround(s.val[Pitch][pp]) + 12 * s.oct[pp] : 0;
        const float vel = s.stepOn[Vel][pv] ? s.val[Vel][pv] : 0.8f;

        if (gateActive && ! tieNext) fn(offset, Ev::NoteOff, 0, 0.0f, false);
        fn(offset, Ev::NoteOn, semis, vel, tieNext);

        gateActive = true;
        tieNext = tie;
        gateOffBeat = tie ? 1.0e18 : startBeat + (double) std::min(g, 1.0f) * (double) s.gateScale * L;
    }

    uint32_t nextRandom() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

    bool running = false, gateActive = false, tieNext = false;
    double beatPos = 0.0, gateOffBeat = 0.0;
    long long nextTick = 0;
    uint32_t rng = 2463534242u;
};
