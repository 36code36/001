#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <atomic>
#include "Analyzer.h"

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;
    bool trimmed = false;
    AnalysisResult analysis;
};

class SamplePitchProcessor : public juce::AudioProcessor
{
public:
    SamplePitchProcessor();
    ~SamplePitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sample Pitcher"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Loads a sound (max 10 s), analyses it. If autoSetRoot is true the root note and
    // fine tune are set from the detected pitch.
    bool loadSampleFile (const juce::File& file, bool autoSetRoot);
    void applyDetectedPitch();

    std::shared_ptr<const SampleData> getSample() const
    {
        const juce::SpinLock::ScopedLockType l (lock);
        return sample;
    }

    // parameter access for the voices (audio thread safe)
    float rootNote()  const { return rootP->load(); }
    float tuneCents() const { return tuneP->load(); }
    float attackSec() const { return attackP->load(); }
    float releaseSec() const { return releaseP->load(); }
    float outGain()   const { return gainP->load(); }
    bool  loopOn()    const { return loopP->load() > 0.5f; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> sampleVersion { 0 };
    juce::String lastError;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::Synthesiser synth;
    mutable juce::SpinLock lock;
    std::shared_ptr<const SampleData> sample;

    std::atomic<float> *rootP = nullptr, *tuneP = nullptr, *attackP = nullptr,
                       *releaseP = nullptr, *gainP = nullptr, *loopP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplePitchProcessor)
};
