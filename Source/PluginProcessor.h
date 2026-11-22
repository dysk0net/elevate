#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Engine/SynthEngine.h"
#include "Engine/Sequencer.h"
#include "FX/Effects.h"

class ElevateAudioProcessor : public juce::AudioProcessor
{
public:
    ElevateAudioProcessor();
    ~ElevateAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Elevate"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    Sequencer sequencer;                         // public so the UI can show the playhead
    std::atomic<float> uiModWheel { 0.0f };

private:
    struct SeqEvent { int offset; Sequencer::Ev type; int semis; float vel; bool legato; };

    float v(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    int   vi(const juce::String& id) const { return (int) std::lround(v(id)); }
    bool  vb(const juce::String& id) const { return v(id) > 0.5f; }

    void readEngineParams();
    void readSequencerSnapshot();
    void handleMidi(const juce::MidiMessage& m);
    void renderSegment(float* out, int len, double ppq);
    void setParam(const juce::String& id, float normalised);

    SynthEngine engine;
    EngineParams engineParams;
    Sequencer::Snapshot seqSnap;
    std::vector<SeqEvent> seqEvents;

    fx::Chorus chorus;
    fx::ModFx modFx;
    fx::SpaceFx spaceFx;

    double sr = 44100.0, bpm = 120.0;
    bool hostPlaying = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ElevateAudioProcessor)
};
