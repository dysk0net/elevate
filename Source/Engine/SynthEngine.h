#pragma once
#include "Oscillator.h"
#include "Envelope.h"
#include "Filter.h"
#include "Lfo.h"
#include <array>
#include <vector>
#include <cstdint>

struct EngineParams
{
    int o1Oct = 0, o2Oct = 0, subOct = 0;                 // octave offsets (-1/0/+1), sub (0 => -1, 1 => -2)
    float o1Coarse = 0, o1Fine = 0, o1Shape = 0.4f, o1ShMod = 0; int o1ShSrc = 0;
    float subLvl = 0;
    float o2Coarse = 0, o2Fine = 0, o2Shape = 0.4f, o2ShMod = 0;
    float warp = 0, warpMod = 0; int warpSrc = 0, warpMode = 0;
    float pitchMod = 0; int pitchSrc = 0;
    float o1Vol = 0.8f, o2Vol = 0, noiseVol = 0; int noiseCol = 0;
    float drive = 0.2f, cutoff = 2000, res = 0.2f, growl = 0, fEnv = 0, fLfo = 0, fKey = 0, hp = 20; bool toHp = false;
    float fA = 0.005f, fD = 0.3f, fS = 0, fR = 0.2f, fV = 0.5f;
    float aA = 0.005f, aD = 0.3f, aS = 0.8f, aR = 0.2f, aV = 0.5f;
    bool lfoSync = false, lfoRetrig = false; float lfoRate = 4, lfoTilt = 0; int lfoDiv = 2, lfoShape = 0;
    int mode = 0; bool legato = false; float detune = 0.3f, bend = 2; int modDest = 0; float modAmt = 0.5f;
    float glide = 0, tune = 0, volume = 0.7f;
    int o1Uni = 1, o2Uni = 1; float o1UniDet = 0.25f, o2UniDet = 0.25f;   // per-oscillator unison
};

class SynthEngine
{
public:
    static constexpr int numSlots = 4;
    static constexpr int maxUni = 11;

    SynthEngine()
    {
        // Spread starting phases so stacked unison voices don't begin coherent.
        for (auto& sl : slots)
            for (int u = 0; u < maxUni; ++u)
            {
                const float ph = std::fmod((float) u * 0.61803f, 1.0f);
                sl.o1[u].reset(ph);
                sl.o2[u].reset(std::fmod(ph + 0.37f, 1.0f));
                sl.master[u].reset(std::fmod(ph + 0.37f, 1.0f));
            }
    }

    void prepare(double sr)
    {
        sampleRate = sr;
        fenv.setSampleRate(sr); aenv.setSampleRate(sr);
        lfo.setSampleRate(sr); filter.setSampleRate(sr);
        allNotesOff();
        for (auto* s : { &sCutoff, &sRes, &sDrive, &sVol, &sO1, &sO2, &sNoise, &sSub }) s->snap();
    }

    // ---- note handling ---------------------------------------------------------
    void noteOn(int note, float velocity)
    {
        held.erase(std::remove_if(held.begin(), held.end(), [&](const Held& h) { return h.note == note; }), held.end());
        const bool wasEmpty = held.empty();
        held.push_back({ note, velocity });
        if (held.size() > 8) held.erase(held.begin());
        refreshTargets(wasEmpty);
        if (! seqActive)
        {
            lastVel = velocity;
            const bool legatoNow = lastParams.legato && ! wasEmpty;
            triggerEnvelopes(! legatoNow);
        }
    }

    void noteOff(int note)
    {
        const bool wasLast = ! held.empty() && held.back().note == note;
        held.erase(std::remove_if(held.begin(), held.end(), [&](const Held& h) { return h.note == note; }), held.end());
        if (held.empty())
        {
            if (! seqActive) { fenv.gate(false); aenv.gate(false); }
            return;
        }
        refreshTargets(false);
        if (wasLast && ! seqActive && lastParams.mode != 0 && ! lastParams.legato)
        {
            lastVel = held.back().vel;
            triggerEnvelopes(true);
        }
    }

    void allNotesOff() { held.clear(); fenv.kill(); aenv.kill(); seqTieNext = false; }
    void setPitchBend(float v) { bendValue = v; }          // -1 .. 1
    void setModWheel(float v) { modWheel = v; }            // 0 .. 1
    bool anyKeyHeld() const { return ! held.empty(); }
    bool isIdle() const { return held.empty() && ! aenv.active(); }

    // ---- sequencer hooks -------------------------------------------------------
    void setSequencerActive(bool on)
    {
        if (seqActive && ! on) { fenv.gate(false); aenv.gate(false); }
        seqActive = on;
    }
    void seqNoteOn(int semis, float vel, bool legato)
    {
        seqSemis = semis; lastVel = vel;
        triggerEnvelopes(! legato);
    }
    void seqNoteOff() { fenv.gate(false); aenv.gate(false); }

    // ---- audio -----------------------------------------------------------------
    void process(float* out, int numSamples, const EngineParams& p, double bpm)
    {
        lastParams = p;
        fenv.setParams(p.fA, p.fD, p.fS, p.fR);
        aenv.setParams(p.aA, p.aD, p.aS, p.aR);

        const float glideSec = p.glide * p.glide * 2.0f;
        const float glideCoef = glideSec < 0.002f ? 1.0f : 1.0f - std::exp(-1.0f / (glideSec * (float) sampleRate));
        const float ampVel = 1.0f - p.aV + p.aV * lastVel;
        const float filVel = 1.0f - p.fV + p.fV * lastVel;

        float lfoHz = p.lfoRate;
        if (p.lfoSync)
        {
            static const float beats[] { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
            lfoHz = (float) (std::max(1.0, bpm) / 60.0) / beats[std::clamp(p.lfoDiv, 0, 5)];
        }
        if (p.modDest == 2) lfoHz *= std::exp2(modWheel * p.modAmt * 3.0f);

        const float keyNote = held.empty() ? lastKeyNote : (float) held.back().note + (float) seqSemis;
        lastKeyNote = keyNote;

        // Unison: symmetric detune ratios for each oscillator's stacked voices.
        const int n1 = std::clamp(p.o1Uni, 1, maxUni), n2 = std::clamp(p.o2Uni, 1, maxUni);
        float r1[maxUni], r2[maxUni];
        for (int u = 0; u < maxUni; ++u)
        {
            const float o1 = n1 > 1 ? 2.0f * (float) u / (float) (n1 - 1) - 1.0f : 0.0f;
            const float o2 = n2 > 1 ? 2.0f * (float) u / (float) (n2 - 1) - 1.0f : 0.0f;
            r1[u] = std::exp2(o1 * p.o1UniDet * 100.0f / 1200.0f);     // up to +/-100 cents
            r2[u] = std::exp2(o2 * p.o2UniDet * 100.0f / 1200.0f);
        }
        const float uni1 = 1.0f / std::sqrt((float) n1), uni2 = 1.0f / std::sqrt((float) n2);

        for (int n = 0; n < numSamples; ++n)
        {
            const float lfoV = lfo.next(lfoHz, p.lfoShape, p.lfoTilt);
            const float fe = fenv.next() * filVel;
            const float ae = aenv.next() * ampVel;
            lastFenv = fe; lastAenv = ae;

            auto src = [&](int s) { return s == 0 ? lfoV : fe; };
            float pitchSemis = p.pitchMod * 12.0f * src(p.pitchSrc);
            pitchSemis += bendValue * p.bend + p.tune / 100.0f;
            if (p.modDest == 0) pitchSemis += modWheel * p.modAmt * lfoV * 1.0f;

            const float shapeMod = src(p.o1ShSrc);
            const float sh1 = std::clamp(p.o1Shape + p.o1ShMod * 0.5f * shapeMod, 0.0f, 1.0f);
            const float sh2 = std::clamp(p.o2Shape + p.o2ShMod * 0.5f * shapeMod, 0.0f, 1.0f);
            const float warpSemis = std::clamp(p.warp + p.warpMod * 36.0f * src(p.warpSrc), 0.0f, 36.0f);

            sO1.set(p.o1Vol); sO2.set(p.o2Vol); sNoise.set(p.noiseVol); sSub.set(p.subLvl);
            const float v1 = sO1.next(), v2 = sO2.next(), vn = sNoise.next(), vs = sSub.next();

            const float warpRatio = std::exp2(warpSemis / 12.0f);
            float mix = 0.0f; int voices = 0;
            for (auto& s : slots)
            {
                if (! s.valid) continue;
                ++voices;
                s.curNote += (s.targetNote + (float) seqSemis - s.curNote) * glideCoef;
                const float note = s.curNote + pitchSemis + s.detuneOffset * p.detune * 0.5f * (p.mode == 2 ? 1.0f : 0.0f);
                const float base = 440.0f * std::exp2((note - 69.0f) / 12.0f);

                bool wrapped = false;
                const float f1 = base * std::exp2((float) p.o1Oct + p.o1Coarse / 12.0f + p.o1Fine / 1200.0f);
                float a = 0.0f;
                for (int u = 0; u < n1; ++u) a += s.o1[u].process(f1 * r1[u], sh1, sampleRate, wrapped);
                a *= v1 * uni1;

                if (vs > 0.0001f)
                {
                    s.subPhase += f1 * std::exp2(-(float) (p.subOct + 1)) / (float) sampleRate;
                    s.subPhase -= std::floor(s.subPhase);
                    a += std::sin(6.2831853f * s.subPhase) * vs;
                }

                const float f2 = base * std::exp2((float) p.o2Oct + p.o2Coarse / 12.0f + p.o2Fine / 1200.0f);
                float b = 0.0f;
                for (int u = 0; u < n2; ++u)
                {
                    const float fu = f2 * r2[u];
                    if (warpSemis < 0.01f)
                    {
                        b += s.o2[u].process(fu, sh2, sampleRate, wrapped);
                    }
                    else
                    {
                        bool mw = false;
                        const float m = s.master[u].process(fu, 0.0f, sampleRate, mw);
                        const float y = s.o2[u].process(fu * warpRatio, sh2, sampleRate, wrapped);
                        if (p.warpMode == 0) { if (mw) s.o2[u].setPhase(0.0f); b += y; }
                        else                 b += y * m;
                    }
                }
                b *= uni2;
                mix += a + b * v2;
            }

            if (vn > 0.0001f) mix += noise(p.noiseCol) * vn;
            if (voices > 1) mix *= 1.0f / std::sqrt((float) voices);

            // Filter
            float oct = std::log2(std::max(20.0f, p.cutoff));
            oct += p.fEnv * fe * 4.0f + p.fLfo * lfoV * 3.0f;
            oct += p.fKey * (keyNote - 60.0f) / 12.0f;
            if (p.modDest == 1) oct += modWheel * p.modAmt * 4.0f;
            sCutoff.set(oct); sRes.set(p.res); sDrive.set(p.drive);
            const float fc = std::exp2(sCutoff.next());
            float hpHz = p.hp;
            if (p.toHp) hpHz = fc * (0.2f + 0.8f * (p.hp - 20.0f) / 4980.0f);

            float y = filter.process(mix, fc, sRes.next(), sDrive.next(), p.growl, hpHz);
            sVol.set(p.volume);
            out[n] = y * ae * sVol.next() * 1.2f;
        }
    }

    float getFilterEnv() const { return lastFenv; }
    float getAmpEnv() const { return lastAenv; }
    float getLfoPhase() const { return lfo.getPhase(); }

private:
    struct Held { int note; float vel; };
    struct Slot
    {
        Oscillator o1[maxUni], o2[maxUni], master[maxUni]; float subPhase = 0, curNote = 60, targetNote = 60, detuneOffset = 0; bool valid = false;
    };
    struct Smoother
    {
        float v = 0, t = 0;
        void set(float x) { t = x; }
        void snap() { v = t; }
        float next() { v += (t - v) * 0.002f; return v; }
    };

    void refreshTargets(bool fresh)
    {
        if (held.empty()) return;
        static const float spread[numSlots] { -1.0f, 0.34f, -0.34f, 1.0f };
        const int n = (int) held.size();
        auto assign = [&](int i, int note)
        {
            auto& s = slots[(size_t) i];
            if (! s.valid || fresh) s.curNote = (float) note;
            s.valid = true; s.targetNote = (float) note; s.detuneOffset = spread[i];
        };
        for (auto& s : slots) { s.detuneOffset = 0; }
        switch (lastParams.mode)
        {
            case 0: for (int i = 0; i < numSlots; ++i)
                    { if (i < n) assign(i, held[(size_t) (n > numSlots ? n - numSlots + i : i)].note);
                      else slots[(size_t) i].valid = false; }
                    break;
            case 1: assign(0, held.back().note);
                    for (int i = 1; i < numSlots; ++i) slots[(size_t) i].valid = false;
                    break;
            default: for (int i = 0; i < numSlots; ++i) assign(i, held.back().note);
        }
    }

    void triggerEnvelopes(bool retrigger)
    {
        fenv.gate(true, retrigger);
        aenv.gate(true, retrigger);
        if (retrigger && lastParams.lfoRetrig) lfo.retrigger();
    }

    float noise(int color)
    {
        rng = rng * 1664525u + 1013904223u;
        const float w = (float) (int32_t) rng / 2147483648.0f;
        if (color == 1) return w * 0.5f;
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return pink * 0.11f;
    }

    double sampleRate = 44100.0;
    std::array<Slot, numSlots> slots;
    std::vector<Held> held;
    Envelope fenv, aenv;
    Lfo lfo;
    LadderFilter filter;
    Smoother sCutoff, sRes, sDrive, sVol, sO1, sO2, sNoise, sSub;
    EngineParams lastParams;
    bool seqActive = false, seqTieNext = false;
    int seqSemis = 0;
    float lastVel = 0.8f, bendValue = 0, modWheel = 0, lastFenv = 0, lastAenv = 0, lastKeyNote = 60;
    uint32_t rng = 22222u;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};
