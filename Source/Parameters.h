#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>

// Parameter IDs for Elevate. Layout follows the OSC / MIXER / FILTER / ENV / LFO /
// VOICE / FX / SEQUENCER sections of a classic paraphonic analog mono-synth.
namespace ID
{
    // Oscillator 1
    inline const juce::String o1Oct   = "o1_oct";
    inline const juce::String o1Coarse= "o1_coarse";
    inline const juce::String o1Fine  = "o1_fine";
    inline const juce::String o1Shape = "o1_shape";
    inline const juce::String o1ShMod = "o1_shape_mod";
    inline const juce::String o1ShSrc = "o1_shape_src";
    inline const juce::String subLvl  = "sub_level";
    inline const juce::String subOct  = "sub_oct";
    // Oscillator 2
    inline const juce::String o2Oct   = "o2_oct";
    inline const juce::String o2Coarse= "o2_coarse";
    inline const juce::String o2Fine  = "o2_fine";
    inline const juce::String o2Shape = "o2_shape";
    inline const juce::String o2ShMod = "o2_shape_mod";
    inline const juce::String warp    = "warp";
    inline const juce::String warpMod = "warp_mod";
    inline const juce::String warpSrc = "warp_src";
    inline const juce::String warpMode= "warp_mode";
    inline const juce::String pitchMod= "pitch_mod";
    inline const juce::String pitchSrc= "pitch_src";
    // Mixer
    inline const juce::String o1Vol   = "o1_vol";
    inline const juce::String o2Vol   = "o2_vol";
    inline const juce::String noiseVol= "noise_vol";
    inline const juce::String noiseCol= "noise_color";
    // Filter
    inline const juce::String drive   = "f_drive";
    inline const juce::String cutoff  = "f_cutoff";
    inline const juce::String res     = "f_res";
    inline const juce::String growl   = "f_growl";
    inline const juce::String fEnv    = "f_env_amt";
    inline const juce::String fLfo    = "f_lfo_amt";
    inline const juce::String fKey    = "f_key";
    inline const juce::String hp      = "f_hp";
    inline const juce::String toHp    = "f_to_hp";
    // Envelopes
    inline const juce::String fA = "fenv_a", fD = "fenv_d", fS = "fenv_s", fR = "fenv_r", fV = "fenv_vel";
    inline const juce::String aA = "aenv_a", aD = "aenv_d", aS = "aenv_s", aR = "aenv_r", aV = "aenv_vel";
    // LFO
    inline const juce::String lfoSync = "lfo_sync";
    inline const juce::String lfoRetrig = "lfo_retrig";
    inline const juce::String lfoRate = "lfo_rate";
    inline const juce::String lfoDiv  = "lfo_div";
    inline const juce::String lfoTilt = "lfo_tilt";
    inline const juce::String lfoShape= "lfo_shape";
    // Voice
    inline const juce::String mode    = "v_mode";
    inline const juce::String legato  = "v_legato";
    inline const juce::String detune  = "v_detune";
    inline const juce::String bend    = "v_bend";
    inline const juce::String modDest = "v_mod_dest";
    inline const juce::String modAmt  = "v_mod_amt";
    inline const juce::String glide   = "v_glide";
    inline const juce::String tune    = "v_tune";
    inline const juce::String volume  = "v_volume";
    // Effects
    inline const juce::String chOn = "chorus_on", chSt = "chorus_stereo", chInt = "chorus_int";
    inline const juce::String mfOn = "modfx_on", mfMode = "modfx_mode", mfRate = "modfx_rate", mfInt = "modfx_int";
    inline const juce::String spAmt = "space_amt", spSync = "space_sync", spMode = "space_mode",
                              spTime = "space_time", spFb = "space_fb";
    // Sequencer
    inline const juce::String seqOn = "seq_on", seqRate = "seq_rate", seqSwing = "seq_swing",
                              seqGate = "seq_gate_scale", seqDir = "seq_dir", seqLink = "seq_link",
                              seqRetrig = "seq_retrig";
    inline const juce::String seqLenG = "seq_len_gate", seqLenP = "seq_len_pitch", seqLenV = "seq_len_vel";
    // Sequencer 2 reuses every sequencer ID with a "seq2_" prefix.
    inline juce::String sq(int n, const juce::String& id) { return n == 0 ? id : "seq2" + id.substring(3); }
    inline juce::String stepOn(char lane, int i, int n = 0)
    { return sq(n, "seq_" + juce::String::charToString(lane) + "_on_" + juce::String(i)); }
    inline juce::String stepVal(char lane, int i, int n = 0)
    { return sq(n, "seq_" + juce::String::charToString(lane) + "_val_" + juce::String(i)); }
    inline juce::String stepOct(int i, int n = 0) { return sq(n, "seq_p_oct_" + juce::String(i)); }

    // Sequencer 2 routing
    inline const juce::String seq2Dest = "seq2_dest", seq2Amt = "seq2_amt", seq2Slew = "seq2_slew";
    // Per-oscillator unison
    inline const juce::String o1Uni = "o1_uni_voices", o1UniDet = "o1_uni_detune",
                              o2Uni = "o2_uni_voices", o2UniDet = "o2_uni_detune";
    // Ordered IDs of every control Sequencer 2 can drive (index 0 of the choice is "None").
    inline juce::StringArray& destIds() { static juce::StringArray a; return a; }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using C = AudioParameterChoice;
    using B = AudioParameterBool;
    using I = AudioParameterInt;
    using R = NormalisableRange<float>;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto flt = [&](const String& id, const String& n, float lo, float hi, float def, float skew = 1.0f)
    { p.push_back(std::make_unique<F>(ParameterID{id, 1}, n, R(lo, hi, 0.0f, skew), def)); };
    auto bip = [&](const String& id, const String& n, float def = 0.0f) { flt(id, n, -1.0f, 1.0f, def); };
    auto chc = [&](const String& id, const String& n, StringArray s, int def = 0)
    { p.push_back(std::make_unique<C>(ParameterID{id, 1}, n, s, def)); };
    auto bln = [&](const String& id, const String& n, bool def)
    { p.push_back(std::make_unique<B>(ParameterID{id, 1}, n, def)); };

    const StringArray oct { "-1", "0", "+1" }, src { "LFO", "F.ENV" };

    chc(ID::o1Oct, "Osc1 Octave", oct, 1);
    flt(ID::o1Coarse, "Osc1 Coarse", -12, 12, 0);
    flt(ID::o1Fine, "Osc1 Fine", -100, 100, 0);
    flt(ID::o1Shape, "Osc1 Shape", 0, 1, 0.4f);
    bip(ID::o1ShMod, "Osc1 Shape Mod");
    chc(ID::o1ShSrc, "Osc1 Shape Src", src);
    flt(ID::subLvl, "Sub Level", 0, 1, 0);
    chc(ID::subOct, "Sub Octave", { "-1", "-2" });

    chc(ID::o2Oct, "Osc2 Octave", oct, 1);
    flt(ID::o2Coarse, "Osc2 Coarse", -12, 12, 0);
    flt(ID::o2Fine, "Osc2 Fine", -100, 100, 0);
    flt(ID::o2Shape, "Osc2 Shape", 0, 1, 0.4f);
    bip(ID::o2ShMod, "Osc2 Shape Mod");
    flt(ID::warp, "Warp", 0, 36, 0);
    bip(ID::warpMod, "Warp Mod");
    chc(ID::warpSrc, "Warp Src", src);
    chc(ID::warpMode, "Warp Mode", { "Sync", "Ring" });
    bip(ID::pitchMod, "Pitch Mod");
    chc(ID::pitchSrc, "Pitch Src", src);
    p.push_back(std::make_unique<I>(ParameterID{ID::o1Uni, 1}, "Osc1 Unison Voices", 1, 11, 1));
    flt(ID::o1UniDet, "Osc1 Unison Detune", 0, 1, 0.25f);
    p.push_back(std::make_unique<I>(ParameterID{ID::o2Uni, 1}, "Osc2 Unison Voices", 1, 11, 1));
    flt(ID::o2UniDet, "Osc2 Unison Detune", 0, 1, 0.25f);

    flt(ID::o1Vol, "Osc1 Volume", 0, 1, 0.8f);
    flt(ID::o2Vol, "Osc2 Volume", 0, 1, 0.0f);
    flt(ID::noiseVol, "Noise Volume", 0, 1, 0);
    chc(ID::noiseCol, "Noise Color", { "Pink", "White" });

    flt(ID::drive, "Drive", 0, 1, 0.2f);
    flt(ID::cutoff, "Cutoff", 20.0f, 20000.0f, 2000.0f, 0.3f);
    flt(ID::res, "Resonance", 0, 1, 0.2f);
    flt(ID::growl, "Growl", 0, 1, 0);
    bip(ID::fEnv, "Filter Env Amt");
    bip(ID::fLfo, "Filter LFO Amt");
    flt(ID::fKey, "Key Track", 0, 1, 0);
    flt(ID::hp, "High Pass", 20.0f, 5000.0f, 20.0f, 0.3f);
    bln(ID::toHp, "HP Follows LP", false);

    auto env = [&](const String& a, const String& d, const String& s, const String& r, const String& v,
                   const String& pre, float sus)
    {
        flt(a, pre + " Attack", 0.001f, 10.0f, 0.005f, 0.3f);
        flt(d, pre + " Decay", 0.001f, 10.0f, 0.3f, 0.3f);
        flt(s, pre + " Sustain", 0, 1, sus);
        flt(r, pre + " Release", 0.001f, 10.0f, 0.2f, 0.3f);
        flt(v, pre + " Velocity", 0, 1, 0.5f);
    };
    env(ID::fA, ID::fD, ID::fS, ID::fR, ID::fV, "Filter Env", 0.0f);
    env(ID::aA, ID::aD, ID::aS, ID::aR, ID::aV, "Amp Env", 0.8f);

    bln(ID::lfoSync, "LFO Sync", false);
    bln(ID::lfoRetrig, "LFO Retrig", false);
    flt(ID::lfoRate, "LFO Rate", 0.05f, 30.0f, 4.0f, 0.4f);
    chc(ID::lfoDiv, "LFO Division", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);
    bip(ID::lfoTilt, "LFO Tilt");
    chc(ID::lfoShape, "LFO Shape", { "Triangle", "Square", "S&H", "Smooth Rnd" });

    chc(ID::mode, "Voice Mode", { "Para", "Mono", "Unison" });
    bln(ID::legato, "Legato", false);
    flt(ID::detune, "Unison Detune", 0, 1, 0.3f);
    flt(ID::bend, "Bend Range", 0, 24, 2);
    chc(ID::modDest, "Mod Wheel Dest", { "Vibrato", "Cutoff", "LFO Rate" });
    flt(ID::modAmt, "Mod Wheel Amt", 0, 1, 0.5f);
    flt(ID::glide, "Glide", 0, 1, 0);
    flt(ID::tune, "Master Tune", -100, 100, 0);
    flt(ID::volume, "Volume", 0, 1, 0.7f);

    bln(ID::chOn, "Chorus On", false);
    bln(ID::chSt, "Chorus Stereo", true);
    flt(ID::chInt, "Chorus Intensity", 0, 1, 0.5f);
    bln(ID::mfOn, "ModFX On", false);
    chc(ID::mfMode, "ModFX Mode", { "Phaser", "Flange +", "Flange -", "Warble" });
    flt(ID::mfRate, "ModFX Rate", 0.05f, 10.0f, 0.6f, 0.4f);
    flt(ID::mfInt, "ModFX Intensity", 0, 1, 0.5f);
    flt(ID::spAmt, "Space Amount", 0, 1, 0);
    bln(ID::spSync, "Space Sync", false);
    chc(ID::spMode, "Space Mode", { "Echo", "Spring", "Hall" });
    flt(ID::spTime, "Space Time", 0, 1, 0.4f);
    flt(ID::spFb, "Space Feedback/Tone", 0, 1, 0.4f);

    // Sequencer 2 can be routed to any control created above.
    static std::once_flag once;
    std::call_once(once, [&] { for (auto& q : p) ID::destIds().add(q->getParameterID()); });
    StringArray destChoices { "None" };
    for (auto& q : p) destChoices.add(q->getName(64));

    for (int n = 0; n < 2; ++n)
    {
        const String pre = n == 0 ? "Seq " : "Seq2 ";
        auto sid = [n](const String& id) { return ID::sq(n, id); };
        bln(sid(ID::seqOn), pre + "On", false);
        chc(sid(ID::seqRate), pre + "Rate", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3);
        flt(sid(ID::seqSwing), pre + "Swing", 0, 0.75f, 0);
        flt(sid(ID::seqGate), pre + "Gate Scale", 0.25f, 2.0f, 1.0f);
        chc(sid(ID::seqDir), pre + "Direction", { "Forward", "Reverse", "PingPong 1", "PingPong 2", "Random" });
        bln(sid(ID::seqLink), pre + "Link Lanes", true);
        bln(sid(ID::seqRetrig), pre + "Retrigger", true);
        p.push_back(std::make_unique<I>(ParameterID{sid(ID::seqLenG), 1}, pre + "Gate Length", 1, 16, 16));
        p.push_back(std::make_unique<I>(ParameterID{sid(ID::seqLenP), 1}, pre + "Pitch Length", 1, 16, 16));
        p.push_back(std::make_unique<I>(ParameterID{sid(ID::seqLenV), 1}, pre + "Vel Length", 1, 16, 16));

        if (n == 1)
        {
            chc(ID::seq2Dest, "Seq2 Destination", destChoices);
            flt(ID::seq2Amt, "Seq2 Amount", -1.0f, 1.0f, 0.5f);
            flt(ID::seq2Slew, "Seq2 Slew", 0.0f, 1.0f, 0.0f);
        }

        for (int i = 0; i < 16; ++i)
        {
            const auto num = String(i + 1);
            bln(ID::stepOn('g', i, n), pre + "Gate On " + num, true);
            flt(ID::stepVal('g', i, n), pre + "Gate Len " + num, 0.05f, 1.2f, 0.5f);   // >1.0 = legato tie
            bln(ID::stepOn('p', i, n), pre + "Pitch On " + num, true);
            p.push_back(std::make_unique<I>(ParameterID{ID::stepVal('p', i, n), 1}, pre + "Pitch " + num, -12, 12, 0));
            p.push_back(std::make_unique<I>(ParameterID{ID::stepOct(i, n), 1}, pre + "Octave " + num, -3, 3, 0));
            bln(ID::stepOn('v', i, n), pre + "Vel On " + num, true);
            flt(ID::stepVal('v', i, n), pre + "Vel " + num, 0, 1, 0.8f);
        }
    }
    return { p.begin(), p.end() };
}
