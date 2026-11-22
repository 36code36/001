#pragma once
#include "PluginProcessor.h"
#include <vector>

class SamplePitchEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit SamplePitchEditor (SamplePitchProcessor&);
    ~SamplePitchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragOver = false; repaint(); }

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void refresh();
    void setupSlider (juce::Slider&, juce::Label&, const juce::String& name);

    SamplePitchProcessor& proc;

    juce::Rectangle<int> dropZone;
    bool dragOver = false;
    int lastVersion = -1;

    std::vector<float> peakMin, peakMax;
    juce::String fileText, pitchText, keyText;

    juce::TextButton browseButton { "Browse..." };
    juce::TextButton useDetectedButton { "Use detected pitch as root" };
    juce::ToggleButton loopButton { "Loop" };

    juce::Slider rootSlider, tuneSlider, attackSlider, releaseSlider, gainSlider;
    juce::Label  rootLabel, tuneLabel, attackLabel, releaseLabel, gainLabel;

    std::unique_ptr<SliderAtt> rootAtt, tuneAtt, attackAtt, releaseAtt, gainAtt;
    std::unique_ptr<ButtonAtt> loopAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplePitchEditor)
};
