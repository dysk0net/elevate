#pragma once
#include "Parameters.h"
#include <vector>
#include <utility>

// Factory presets. Values are real (not normalised) parameter values; choices are indices.
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

namespace PresetHelpers
{
    using V = std::vector<std::pair<juce::String, float>>;

    // Gate lane: 0 = rest, values > 1.0 tie into the next step.
    inline void pattern(V& v, const std::vector<float>& gate, const std::vector<int>& pitch,
                        const std::vector<float>& vel, const std::vector<int>& oct = {})
    {
        v.push_back({ ID::seqOn, 1.0f });
        v.push_back({ ID::seqLink, 1.0f });
        v.push_back({ ID::seqLenG, (float) gate.size() });
        v.push_back({ ID::seqLenP, (float) gate.size() });
        v.push_back({ ID::seqLenV, (float) gate.size() });
        for (size_t i = 0; i < gate.size(); ++i)
        {
            v.push_back({ ID::stepOn('g', (int) i), gate[i] > 0.0f ? 1.0f : 0.0f });
            v.push_back({ ID::stepVal('g', (int) i), gate[i] > 0.0f ? gate[i] : 0.5f });
            if (i < pitch.size()) v.push_back({ ID::stepVal('p', (int) i), (float) pitch[i] });
            if (i < vel.size())   v.push_back({ ID::stepVal('v', (int) i), vel[i] });
            if (i < oct.size())   v.push_back({ ID::stepOct((int) i), (float) oct[i] });
        }
    }
}

inline std::vector<Preset> createFactoryPresets()
{
    using namespace PresetHelpers;
    std::vector<Preset> list;

    list.push_back({ "Init", {} });

    list.push_back({ "Fat Bass", {
        { ID::mode, 1 }, { ID::o1Shape, 0.33f }, { ID::o1Oct, 0 }, { ID::subLvl, 0.6f },
        { ID::o2Vol, 0.8f }, { ID::o2Oct, 0 }, { ID::o2Shape, 0.33f }, { ID::o2Fine, 9 },
        { ID::cutoff, 280 }, { ID::res, 0.3f }, { ID::fEnv, 0.55f }, { ID::fD, 0.25f }, { ID::fS, 0.1f },
        { ID::drive, 0.5f }, { ID::aS, 0.9f }, { ID::aR, 0.15f }, { ID::fV, 0.4f } } });

    {
        V v { { ID::mode, 1 }, { ID::legato, 1 }, { ID::glide, 0.22f }, { ID::o1Shape, 0.33f }, { ID::o1Vol, 0.9f },
              { ID::cutoff, 380 }, { ID::res, 0.78f }, { ID::growl, 0.3f }, { ID::drive, 0.4f },
              { ID::fEnv, 0.7f }, { ID::fD, 0.22f }, { ID::fS, 0.0f }, { ID::aS, 0.8f }, { ID::aR, 0.08f },
              { ID::seqRate, 3 } };
        pattern(v, { 0.6f, 0.6f, 1.1f, 0.5f, 0.6f, 0.0f, 1.1f, 0.5f, 0.6f, 0.6f, 0.5f, 1.1f, 0.6f, 0.5f, 0.0f, 0.6f },
                   { 0, 0, 12, 0, 3, 0, 7, 10, 0, 0, 12, 7, 0, 5, 0, 3 },
                   { 1.0f, 0.6f, 0.8f, 0.7f, 1.0f, 0.5f, 0.9f, 0.7f, 1.0f, 0.6f, 0.8f, 0.7f, 1.0f, 0.5f, 0.6f, 0.9f });
        list.push_back({ "Acid Line", v });
    }

    list.push_back({ "Brass Stab", {
        { ID::mode, 0 }, { ID::o1Shape, 0.33f }, { ID::o2Vol, 0.8f }, { ID::o2Shape, 0.33f }, { ID::o2Fine, 8 },
        { ID::cutoff, 300 }, { ID::fEnv, 0.45f }, { ID::fA, 0.1f }, { ID::fD, 0.8f }, { ID::fS, 0.5f }, { ID::fR, 0.2f },
        { ID::res, 0.15f }, { ID::aS, 0.85f }, { ID::aR, 0.25f }, { ID::chOn, 1 }, { ID::chInt, 0.4f } } });

    list.push_back({ "Warm Pad", {
        { ID::mode, 0 }, { ID::o1Shape, 0.15f }, { ID::o2Vol, 0.7f }, { ID::o2Shape, 0.2f }, { ID::o2Fine, -9 },
        { ID::aA, 1.2f }, { ID::aR, 2.2f }, { ID::aS, 1.0f }, { ID::cutoff, 1500 }, { ID::res, 0.1f },
        { ID::fEnv, 0.2f }, { ID::fA, 1.5f }, { ID::fS, 0.6f }, { ID::fLfo, 0.1f }, { ID::lfoRate, 0.3f },
        { ID::o1ShMod, 0.3f }, { ID::chOn, 1 }, { ID::chInt, 0.7f }, { ID::spAmt, 0.35f }, { ID::spMode, 2 }, { ID::spTime, 0.7f },
        { ID::spFb, 0.5f } } });

    list.push_back({ "Sync Lead", {
        { ID::mode, 1 }, { ID::legato, 1 }, { ID::glide, 0.12f }, { ID::o1Vol, 0.3f }, { ID::o2Vol, 0.9f },
        { ID::warp, 7 }, { ID::warpMod, 0.5f }, { ID::warpSrc, 1 }, { ID::warpMode, 0 },
        { ID::cutoff, 5000 }, { ID::res, 0.2f }, { ID::fEnv, 0.3f }, { ID::fD, 0.4f }, { ID::fS, 0.3f },
        { ID::aS, 0.9f }, { ID::spAmt, 0.3f }, { ID::spMode, 0 }, { ID::spSync, 1 }, { ID::spTime, 0.5f }, { ID::spFb, 0.4f } } });

    list.push_back({ "Dub Echo Keys", {
        { ID::mode, 0 }, { ID::o1Shape, 0.6f }, { ID::cutoff, 1800 }, { ID::res, 0.3f }, { ID::fEnv, 0.3f },
        { ID::aS, 0.5f }, { ID::aD, 0.5f }, { ID::aR, 0.3f }, { ID::mfOn, 1 }, { ID::mfMode, 3 }, { ID::mfInt, 0.3f },
        { ID::spAmt, 0.45f }, { ID::spMode, 0 }, { ID::spSync, 1 }, { ID::spTime, 0.6f }, { ID::spFb, 0.6f } } });

    list.push_back({ "Unison Super", {
        { ID::mode, 2 }, { ID::detune, 0.6f }, { ID::o1Shape, 0.33f }, { ID::o2Vol, 0.7f }, { ID::o2Shape, 0.33f },
        { ID::cutoff, 4000 }, { ID::res, 0.15f }, { ID::fEnv, 0.25f }, { ID::fD, 0.5f }, { ID::fS, 0.4f },
        { ID::aS, 0.9f }, { ID::chOn, 1 }, { ID::chInt, 0.6f } } });

    {
        V v { { ID::mode, 0 }, { ID::o1Shape, 0.5f }, { ID::o2Vol, 0.5f }, { ID::o2Oct, 2 }, { ID::cutoff, 1200 },
              { ID::res, 0.35f }, { ID::fEnv, 0.5f }, { ID::fD, 0.18f }, { ID::fS, 0.0f }, { ID::aS, 0.0f },
              { ID::aD, 0.22f }, { ID::aR, 0.2f }, { ID::seqRate, 3 }, { ID::seqSwing, 0.25f },
              { ID::spAmt, 0.3f }, { ID::spMode, 1 }, { ID::spTime, 0.5f }, { ID::spFb, 0.5f } };
        pattern(v, { 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f },
                   { 0, 7, 12, 0, 3, 10, 7, 15 },
                   { 1.0f, 0.5f, 0.7f, 0.5f, 0.9f, 0.5f, 0.7f, 0.6f },
                   { 0, 0, 0, 0, 0, 0, 1, 0 });
        list.push_back({ "Arp Sequence", v });
    }
    return list;
}
