#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const std::vector<Preset>& presets() { static const auto p = createFactoryPresets(); return p; }

    // Default MIDI CC map (follows the CC table in the Anthem manual).
    struct CcMap { int cc; const char* id; };
    const CcMap ccTable[] {
        { 5, "v_glide" }, { 12, "modfx_rate" }, { 13, "modfx_int" }, { 14, "aenv_s" },
        { 22, "fenv_a" }, { 23, "fenv_d" }, { 24, "fenv_r" }, { 25, "f_env_amt" },
        { 26, "o1_shape" }, { 27, "o2_shape" }, { 68, "v_legato" }, { 70, "sub_level" },
        { 71, "f_res" }, { 72, "aenv_r" }, { 73, "aenv_a" }, { 74, "f_cutoff" }, { 75, "aenv_d" },
        { 76, "lfo_rate" }, { 86, "o1_vol" }, { 89, "noise_vol" }, { 91, "modfx_on" }, { 92, "space_amt" }
    };
}

ElevateAudioProcessor::ElevateAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ELEVATE", createLayout())
{
    seqEvents.reserve(512);
}

bool ElevateAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ElevateAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare(sampleRate);
    chorus.prepare(sampleRate);
    modFx.prepare(sampleRate);
    spaceFx.prepare(sampleRate);
    sequencer.reset();
}

//==============================================================================
void ElevateAudioProcessor::readEngineParams()
{
    auto& p = engineParams;
    p.o1Oct = vi(ID::o1Oct) - 1;  p.o2Oct = vi(ID::o2Oct) - 1;  p.subOct = vi(ID::subOct);
    p.o1Coarse = v(ID::o1Coarse); p.o1Fine = v(ID::o1Fine); p.o1Shape = v(ID::o1Shape);
    p.o1ShMod = v(ID::o1ShMod);   p.o1ShSrc = vi(ID::o1ShSrc); p.subLvl = v(ID::subLvl);
    p.o2Coarse = v(ID::o2Coarse); p.o2Fine = v(ID::o2Fine); p.o2Shape = v(ID::o2Shape);
    p.o2ShMod = v(ID::o2ShMod);
    p.warp = v(ID::warp); p.warpMod = v(ID::warpMod); p.warpSrc = vi(ID::warpSrc); p.warpMode = vi(ID::warpMode);
    p.pitchMod = v(ID::pitchMod); p.pitchSrc = vi(ID::pitchSrc);
    p.o1Vol = v(ID::o1Vol); p.o2Vol = v(ID::o2Vol); p.noiseVol = v(ID::noiseVol); p.noiseCol = vi(ID::noiseCol);
    p.drive = v(ID::drive); p.cutoff = v(ID::cutoff); p.res = v(ID::res); p.growl = v(ID::growl);
    p.fEnv = v(ID::fEnv); p.fLfo = v(ID::fLfo); p.fKey = v(ID::fKey); p.hp = v(ID::hp); p.toHp = vb(ID::toHp);
    p.fA = v(ID::fA); p.fD = v(ID::fD); p.fS = v(ID::fS); p.fR = v(ID::fR); p.fV = v(ID::fV);
    p.aA = v(ID::aA); p.aD = v(ID::aD); p.aS = v(ID::aS); p.aR = v(ID::aR); p.aV = v(ID::aV);
    p.lfoSync = vb(ID::lfoSync); p.lfoRetrig = vb(ID::lfoRetrig); p.lfoRate = v(ID::lfoRate);
    p.lfoDiv = vi(ID::lfoDiv); p.lfoTilt = v(ID::lfoTilt); p.lfoShape = vi(ID::lfoShape);
    p.mode = vi(ID::mode); p.legato = vb(ID::legato); p.detune = v(ID::detune); p.bend = v(ID::bend);
    p.modDest = vi(ID::modDest); p.modAmt = v(ID::modAmt); p.glide = v(ID::glide);
    p.tune = v(ID::tune); p.volume = v(ID::volume);
}

void ElevateAudioProcessor::readSequencerSnapshot()
{
    auto& s = seqSnap;
    s.on = vb(ID::seqOn); s.retrig = vb(ID::seqRetrig);
    s.rateIdx = vi(ID::seqRate); s.dir = vi(ID::seqDir);
    s.swing = v(ID::seqSwing); s.gateScale = v(ID::seqGate);
    if (! s.on) return;                                  // step data is only needed while running
    const bool link = vb(ID::seqLink);
    s.len[0] = vi(ID::seqLenG);
    s.len[1] = link ? s.len[0] : vi(ID::seqLenP);
    s.len[2] = link ? s.len[0] : vi(ID::seqLenV);
    for (int i = 0; i < Sequencer::numSteps; ++i)
    {
        s.stepOn[0][i] = vb(ID::stepOn('g', i)); s.val[0][i] = v(ID::stepVal('g', i));
        s.stepOn[1][i] = vb(ID::stepOn('p', i)); s.val[1][i] = v(ID::stepVal('p', i)); s.oct[i] = vi(ID::stepOct(i));
        s.stepOn[2][i] = vb(ID::stepOn('v', i)); s.val[2][i] = v(ID::stepVal('v', i));
        s.val[1][i] = std::round(s.val[1][i]);
    }
}

void ElevateAudioProcessor::setParam(const juce::String& id, float normalised)
{
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(normalised);
}

void ElevateAudioProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())            engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      engine.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())   engine.setPitchBend((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float norm = (float) m.getControllerValue() / 127.0f;
        if (cc == 1) { engine.setModWheel(norm); uiModWheel = norm; return; }
        for (auto& e : ccTable)
            if (e.cc == cc) { setParam(e.id, norm); break; }
    }
}

void ElevateAudioProcessor::renderSegment(float* out, int len, double ppq)
{
    if (len <= 0) return;
    seqEvents.clear();
    sequencer.process(len, bpm, sr, hostPlaying, ppq, engine.anyKeyHeld(), seqSnap,
        [this](int off, Sequencer::Ev t, int semis, float vel, bool leg)
        { if (seqEvents.size() < seqEvents.capacity()) seqEvents.push_back({ off, t, semis, vel, leg }); });

    int pos = 0;
    for (auto& e : seqEvents)
    {
        if (e.offset > pos) { engine.process(out + pos, e.offset - pos, engineParams, bpm); pos = e.offset; }
        if (e.type == Sequencer::Ev::NoteOn) engine.seqNoteOn(e.semis, e.vel, e.legato);
        else                                 engine.seqNoteOff();
    }
    if (pos < len) engine.process(out + pos, len - pos, engineParams, bpm);
}

void ElevateAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    double ppqStart = 0.0;
    hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppqStart = *q;
        }

    readEngineParams();
    readSequencerSnapshot();
    engine.setSequencerActive(seqSnap.on);

    float* left = buffer.getWritePointer(0);
    const double beatsPerSample = bpm / 60.0 / sr;

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = std::clamp(meta.samplePosition, pos, numSamples);
        renderSegment(left + pos, t - pos, ppqStart + (double) pos * beatsPerSample);
        pos = t;
        handleMidi(meta.getMessage());
    }
    renderSegment(left + pos, numSamples - pos, ppqStart + (double) pos * beatsPerSample);
    midi.clear();

    // Effects chain: Chorus -> Mod FX -> Space FX (stereo)
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    juce::AudioBuffer<float> scratch;
    if (right == nullptr) { scratch.setSize(1, numSamples); right = scratch.getWritePointer(0); }
    juce::FloatVectorOperations::copy(right, left, numSamples);

    chorus.process(left, right, numSamples, vb(ID::chOn), vb(ID::chSt), v(ID::chInt));
    modFx.process(left, right, numSamples, vb(ID::mfOn), vi(ID::mfMode), v(ID::mfRate), v(ID::mfInt));
    spaceFx.process(left, right, numSamples, v(ID::spAmt), vb(ID::spSync), vi(ID::spMode),
                    v(ID::spTime), v(ID::spFb), bpm);

    // Safety limiter keeps resonant patches from clipping the host.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) d[i] = std::tanh(d[i]);
    }
}

//==============================================================================
int ElevateAudioProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String ElevateAudioProcessor::getProgramName(int i)
{ return juce::isPositiveAndBelow(i, getNumPrograms()) ? presets()[(size_t) i].name : juce::String(); }

void ElevateAudioProcessor::setCurrentProgram(int index)
{
    if (! juce::isPositiveAndBelow(index, getNumPrograms())) return;
    currentProgram = index;
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            r->setValueNotifyingHost(r->getDefaultValue());
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* r = apvts.getParameter(id))
            r->setValueNotifyingHost(r->convertTo0to1(value));
}

void ElevateAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void ElevateAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            currentProgram = (int) tree.getProperty("program", 0);
            apvts.replaceState(tree);
        }
}

juce::AudioProcessorEditor* ElevateAudioProcessor::createEditor() { return new ElevateEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ElevateAudioProcessor(); }
