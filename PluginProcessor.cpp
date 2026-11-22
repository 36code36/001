#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    class SampleSound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    // Plays the loaded sound resampled so that the root note maps to the played note.
    class SampleVoice : public juce::SynthesiserVoice
    {
    public:
        explicit SampleVoice (SamplePitchProcessor& p) : proc (p) {}

        bool canPlaySound (juce::SynthesiserSound* s) override
        {
            return dynamic_cast<SampleSound*> (s) != nullptr;
        }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
        {
            data = proc.getSample();
            if (data == nullptr || data->buffer.getNumSamples() < 2)
            {
                data = nullptr;
                clearCurrentNote();
                return;
            }

            const double semis = (double) midiNote - (double) proc.rootNote() - (double) proc.tuneCents() / 100.0;
            baseRatio = std::pow (2.0, semis / 12.0) * data->sampleRate / getSampleRate();
            bendSemis = ((double) pitchWheel - 8192.0) / 8192.0 * 2.0;
            pos = 0.0;
            gain = velocity;

            adsr.setSampleRate (getSampleRate());
            adsr.setParameters (juce::ADSR::Parameters (proc.attackSec(), 0.001f, 1.0f, proc.releaseSec()));
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                adsr.noteOff();
            }
            else
            {
                adsr.reset();
                data = nullptr;
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int v) override { bendSemis = ((double) v - 8192.0) / 8192.0 * 2.0; }
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
        {
            if (data == nullptr)
                return;

            const auto& src = data->buffer;
            const int n = src.getNumSamples();
            const int srcCh = src.getNumChannels();
            const bool loop = proc.loopOn();
            const float g = gain * proc.outGain();
            const double ratio = baseRatio * std::pow (2.0, bendSemis / 12.0);

            for (int i = 0; i < numSamples; ++i)
            {
                if (pos >= (double) (n - 1))
                {
                    if (loop)
                    {
                        pos = std::fmod (pos, (double) (n - 1));
                    }
                    else
                    {
                        adsr.reset();
                        data = nullptr;
                        clearCurrentNote();
                        break;
                    }
                }

                const int i0 = (int) pos;
                const float frac = (float) (pos - (double) i0);
                const float env = adsr.getNextSample() * g;

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                {
                    const float* s = src.getReadPointer (juce::jmin (ch, srcCh - 1));
                    out.addSample (ch, startSample + i, (s[i0] + frac * (s[i0 + 1] - s[i0])) * env);
                }

                pos += ratio;

                if (! adsr.isActive())
                {
                    data = nullptr;
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        SamplePitchProcessor& proc;
        std::shared_ptr<const SampleData> data;
        juce::ADSR adsr;
        double pos = 0.0, baseRatio = 1.0, bendSemis = 0.0;
        float gain = 1.0f;
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SamplePitchProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterInt> (
        ParameterID { "root", 1 }, "Root Note", 0, 127, 60,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return Analyzer::midiName (v); })));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tune", 1 }, "Fine Tune", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.005f));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release", NormalisableRange<float> (0.005f, 5.0f, 0.001f, 0.4f), 0.2f));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gain", 1 }, "Gain", NormalisableRange<float> (0.0f, 1.5f, 0.01f), 0.8f));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Loop", false));

    return l;
}

SamplePitchProcessor::SamplePitchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    rootP    = apvts.getRawParameterValue ("root");
    tuneP    = apvts.getRawParameterValue ("tune");
    attackP  = apvts.getRawParameterValue ("attack");
    releaseP = apvts.getRawParameterValue ("release");
    gainP    = apvts.getRawParameterValue ("gain");
    loopP    = apvts.getRawParameterValue ("loop");

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new SampleVoice (*this));
    synth.addSound (new SampleSound());
}

void SamplePitchProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

bool SamplePitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SamplePitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}

//==============================================================================
bool SamplePitchProcessor::loadSampleFile (const juce::File& file, bool autoSetRoot)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2)
    {
        lastError = "Could not read " + file.getFileName();
        sampleVersion++;
        return false;
    }

    const juce::int64 maxLen = (juce::int64) (reader->sampleRate * 10.0); // 10 s limit
    const int n = (int) juce::jmin (reader->lengthInSamples, maxLen);

    auto d = std::make_shared<SampleData>();
    d->buffer.setSize (juce::jmin (2, (int) reader->numChannels), n);
    reader->read (&d->buffer, 0, n, 0, true, true);
    d->sampleRate = reader->sampleRate;
    d->name = file.getFileName();
    d->trimmed = reader->lengthInSamples > maxLen;
    d->analysis = Analyzer::analyse (d->buffer, d->sampleRate);

    {
        const juce::SpinLock::ScopedLockType l (lock);
        sample = d;
    }

    lastError.clear();
    apvts.state.setProperty ("samplePath", file.getFullPathName(), nullptr);

    if (autoSetRoot)
        applyDetectedPitch();

    sampleVersion++;
    return true;
}

void SamplePitchProcessor::applyDetectedPitch()
{
    auto s = getSample();
    if (s == nullptr || ! s->analysis.pitchFound)
        return;

    if (auto* p = apvts.getParameter ("root"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) s->analysis.midiNote));
    if (auto* p = apvts.getParameter ("tune"))
        p->setValueNotifyingHost (p->convertTo0to1 (s->analysis.cents));
}

//==============================================================================
juce::AudioProcessorEditor* SamplePitchProcessor::createEditor()
{
    return new SamplePitchEditor (*this);
}

void SamplePitchProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SamplePitchProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto vt = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (vt);

    const auto path = vt.getProperty ("samplePath").toString();
    if (path.isNotEmpty() && juce::File (path).existsAsFile())
        loadSampleFile (juce::File (path), false);
}
