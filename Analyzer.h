#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

struct AnalysisResult
{
    bool  pitchFound = false;   // a clear fundamental pitch was found
    float frequency  = 0.0f;    // Hz
    int   midiNote   = 60;      // nearest MIDI note of the fundamental
    float cents      = 0.0f;    // how many cents the sample is above midiNote

    bool  keyFound      = false; // a musical key was estimated
    int   keyRoot       = 0;     // 0 = C ... 11 = B
    bool  minor         = false;
    float keyConfidence = 0.0f;  // 0..1 (correlation)
};

namespace Analyzer
{
    inline const char* noteName (int pitchClass)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return names[((pitchClass % 12) + 12) % 12];
    }

    // FL Studio naming: MIDI 60 is shown as C5 in the piano roll.
    inline juce::String midiName (int midi)
    {
        return juce::String (noteName (midi)) + juce::String (midi / 12);
    }

    // Detects fundamental pitch (YIN) and musical key (chromagram + Krumhansl profiles).
    AnalysisResult analyse (const juce::AudioBuffer<float>& buffer, double sampleRate);
}
