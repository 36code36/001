#include "PluginEditor.h"

namespace
{
    bool isAudioFile (const juce::String& path)
    {
        const auto ext = juce::File (path).getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg";
    }
}

SamplePitchEditor::SamplePitchEditor (SamplePitchProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setupSlider (rootSlider,    rootLabel,    "Root note");
    setupSlider (tuneSlider,    tuneLabel,    "Fine tune (ct)");
    setupSlider (attackSlider,  attackLabel,  "Attack (s)");
    setupSlider (releaseSlider, releaseLabel, "Release (s)");
    setupSlider (gainSlider,    gainLabel,    "Gain");

    rootAtt    = std::make_unique<SliderAtt> (proc.apvts, "root",    rootSlider);
    tuneAtt    = std::make_unique<SliderAtt> (proc.apvts, "tune",    tuneSlider);
    attackAtt  = std::make_unique<SliderAtt> (proc.apvts, "attack",  attackSlider);
    releaseAtt = std::make_unique<SliderAtt> (proc.apvts, "release", releaseSlider);
    gainAtt    = std::make_unique<SliderAtt> (proc.apvts, "gain",    gainSlider);

    addAndMakeVisible (loopButton);
    loopAtt = std::make_unique<ButtonAtt> (proc.apvts, "loop", loopButton);

    addAndMakeVisible (browseButton);
    browseButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a sound (about 5 seconds)", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        juce::Component::SafePointer<SamplePitchEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                  {
                                      safe->proc.loadSampleFile (f, true);
                                      safe->refresh();
                                  }
                              });
    };

    addAndMakeVisible (useDetectedButton);
    useDetectedButton.onClick = [this] { proc.applyDetectedPitch(); };

    setSize (620, 440);
    refresh();
    startTimerHz (10);
}

SamplePitchEditor::~SamplePitchEditor()
{
    stopTimer();
}

void SamplePitchEditor::setupSlider (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 22);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (l);
}

void SamplePitchEditor::timerCallback()
{
    if (proc.sampleVersion.load() != lastVersion)
        refresh();
}

void SamplePitchEditor::refresh()
{
    lastVersion = proc.sampleVersion.load();
    peakMin.clear();
    peakMax.clear();

    auto s = proc.getSample();

    if (proc.lastError.isNotEmpty())
        fileText = proc.lastError;
    else if (s == nullptr)
        fileText = "No sound loaded";

    if (s != nullptr && proc.lastError.isEmpty())
    {
        const int n = s->buffer.getNumSamples();
        const double secs = (double) n / s->sampleRate;
        fileText = s->name + "   (" + juce::String (secs, 2) + " s" + (s->trimmed ? ", trimmed to 10 s)" : ")");

        const auto& a = s->analysis;

        if (a.pitchFound)
        {
            const juce::String sign = a.cents >= 0.0f ? "+" : "";
            pitchText = "Detected pitch: " + Analyzer::midiName (a.midiNote)
                      + "   (" + juce::String (a.frequency, 1) + " Hz, " + sign + juce::String (a.cents, 0) + " cents)";
        }
        else
        {
            pitchText = "Detected pitch: none found (noisy or percussive sound) - set the root note by hand";
        }

        if (a.keyFound)
            keyText = "Detected key: " + juce::String (Analyzer::noteName (a.keyRoot)) + (a.minor ? " minor" : " major")
                    + "   (confidence " + juce::String (juce::roundToInt (a.keyConfidence * 100.0f)) + "%)";
        else
            keyText = "Detected key: unknown";

        const int bins = 512;
        peakMin.assign ((size_t) bins, 0.0f);
        peakMax.assign ((size_t) bins, 0.0f);
        const float* ch0 = s->buffer.getReadPointer (0);

        for (int b = 0; b < bins; ++b)
        {
            const int from = (int) ((juce::int64) n * b / bins);
            const int to   = juce::jmax (from + 1, (int) ((juce::int64) n * (b + 1) / bins));
            float mn = 0.0f, mx = 0.0f;
            for (int i = from; i < to && i < n; ++i)
            {
                mn = juce::jmin (mn, ch0[i]);
                mx = juce::jmax (mx, ch0[i]);
            }
            peakMin[(size_t) b] = mn;
            peakMax[(size_t) b] = mx;
        }
    }
    else
    {
        pitchText.clear();
        keyText.clear();
    }

    repaint();
}

bool SamplePitchEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f))
            return true;
    return false;
}

void SamplePitchEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;

    for (auto& f : files)
    {
        if (isAudioFile (f))
        {
            proc.loadSampleFile (juce::File (f), true);
            break;
        }
    }

    refresh();
}

void SamplePitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d22));

    // drop zone
    auto zone = dropZone.toFloat();
    g.setColour (dragOver ? juce::Colour (0xff2d4a63) : juce::Colour (0xff262a31));
    g.fillRoundedRectangle (zone, 8.0f);
    g.setColour (dragOver ? juce::Colour (0xff5fb4ff) : juce::Colour (0xff4a505b));
    g.drawRoundedRectangle (zone.reduced (0.5f), 8.0f, 1.5f);

    if (peakMax.empty())
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (18.0f));
        g.drawText ("Drop a sound here (about 5 seconds)", dropZone, juce::Justification::centred);
    }
    else
    {
        g.setColour (juce::Colour (0xff5fb4ff));
        const float mid = zone.getCentreY();
        const float half = zone.getHeight() * 0.45f;
        const int bins = (int) peakMax.size();

        for (int b = 0; b < bins; ++b)
        {
            const float x = zone.getX() + 6.0f + (zone.getWidth() - 12.0f) * (float) b / (float) bins;
            g.drawLine (x, mid - peakMax[(size_t) b] * half, x, mid - peakMin[(size_t) b] * half - 0.5f, 1.0f);
        }
    }

    // analyzer readout
    auto text = getLocalBounds().withTop (dropZone.getBottom() + 8).withHeight (74).reduced (18, 0);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (fileText, text.removeFromTop (22), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffffd166));
    g.drawText (pitchText, text.removeFromTop (22), juce::Justification::centredLeft);
    g.drawText (keyText, text.removeFromTop (22), juce::Justification::centredLeft);
}

void SamplePitchEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    dropZone = area.removeFromTop (150);

    area.removeFromTop (84); // analyzer text is painted here

    auto row = area.removeFromTop (30);
    browseButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (10);
    useDetectedButton.setBounds (row.removeFromLeft (210));
    row.removeFromLeft (10);
    loopButton.setBounds (row.removeFromLeft (80));
    area.removeFromTop (8);

    auto place = [&area] (juce::Label& l, juce::Slider& s)
    {
        auto r = area.removeFromTop (30);
        l.setBounds (r.removeFromLeft (110));
        s.setBounds (r);
    };

    place (rootLabel,    rootSlider);
    place (tuneLabel,    tuneSlider);
    place (attackLabel,  attackSlider);
    place (releaseLabel, releaseSlider);
    place (gainLabel,    gainSlider);
}
