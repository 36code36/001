#include "Analyzer.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace
{
    // One YIN pitch estimate. Returns frequency in Hz or 0 if nothing reliable was found.
    float yinFrame (const float* x, int W, int tauMax, int tauMin, double sr, float& clarity)
    {
        std::vector<float> d ((size_t) tauMax + 1, 0.0f), cm ((size_t) tauMax + 1, 1.0f);

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const float diff = x[j] - x[j + tau];
                s += (double) diff * diff;
            }
            d[(size_t) tau] = (float) s;
        }

        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            cm[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }

        int tau = juce::jmax (2, tauMin);
        while (tau < tauMax)
        {
            if (cm[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau])
                    ++tau;
                break;
            }
            ++tau;
        }

        if (tau >= tauMax)
            return 0.0f;

        float better = (float) tau;
        if (tau > 1 && tau < tauMax)
        {
            const float a = cm[(size_t) tau - 1], b = cm[(size_t) tau], c = cm[(size_t) tau + 1];
            const float den = a + c - 2.0f * b;
            if (std::abs (den) > 1.0e-9f)
                better = (float) tau + (a - c) / (2.0f * den);
        }

        clarity = 1.0f - cm[(size_t) tau];
        return (float) (sr / better);
    }

    float correlate (const float* a, const float* b)
    {
        float ma = 0, mb = 0;
        for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12.0f; mb /= 12.0f;

        float num = 0, da = 0, db = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = a[i] - ma, y = b[i] - mb;
            num += x * y; da += x * x; db += y * y;
        }
        return (da > 0 && db > 0) ? num / std::sqrt (da * db) : 0.0f;
    }
}

AnalysisResult Analyzer::analyse (const juce::AudioBuffer<float>& buffer, double sr)
{
    AnalysisResult r;

    const int n = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    if (n < 512 || chans < 1 || sr <= 0.0)
        return r;

    // mono mix
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int ch = 0; ch < chans; ++ch)
    {
        const float* s = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] += s[i] / (float) chans;
    }

    // ---------- 1) fundamental pitch (YIN on up to 24 frames) ----------
    {
        const int tauMax = juce::jmin ((int) (sr / 40.0), n / 2);
        const int tauMin = (int) (sr / 2000.0);
        const int W = juce::jmin (2048, n - tauMax);

        if (W >= 256 && tauMax > tauMin + 2)
        {
            const int frameLen = W + tauMax;
            const int numFrames = 24;
            const int lastStart = juce::jmax (0, n - frameLen);
            std::vector<float> midis;

            for (int f = 0; f < numFrames; ++f)
            {
                const int start = (numFrames > 1) ? (lastStart * f) / (numFrames - 1) : 0;
                const float* x = mono.data() + start;

                double e = 0.0;
                for (int j = 0; j < W; ++j) e += (double) x[j] * x[j];
                if (std::sqrt (e / W) < 0.01)
                    continue; // silent

                float clarity = 0.0f;
                const float hz = yinFrame (x, W, tauMax, tauMin, sr, clarity);
                if (hz > 0.0f && clarity > 0.85f)
                    midis.push_back (69.0f + 12.0f * std::log2 (hz / 440.0f));
            }

            if (midis.size() >= 2)
            {
                std::sort (midis.begin(), midis.end());
                const float m = midis[midis.size() / 2];
                r.pitchFound = true;
                r.midiNote   = juce::jlimit (0, 127, (int) std::lround (m));
                r.cents      = (m - (float) std::lround (m)) * 100.0f;
                r.frequency  = 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f);
            }
        }
    }

    // ---------- 2) key (chromagram + Krumhansl-Schmuckler) ----------
    {
        constexpr int order = 13;
        constexpr int N = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) N * 2);
        float chroma[12] = {};

        for (int pos = 0; pos == 0 || pos + N <= n; pos += N / 2)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            const int count = juce::jmin (N, n - pos);
            std::copy (mono.begin() + pos, mono.begin() + pos + count, data.begin());

            window.multiplyWithWindowingTable (data.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (data.data());

            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * sr / N;
                if (f < 100.0 || f > 5000.0)
                    continue;
                const double m = 69.0 + 12.0 * std::log2 (f / 440.0);
                const int pc = (((int) std::lround (m)) % 12 + 12) % 12;
                chroma[pc] += data[(size_t) k];
            }
        }

        float total = 0;
        for (float c : chroma) total += c;

        if (total > 1.0e-6f)
        {
            static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                             2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
            static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                             2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

            float best = -2.0f;
            for (int key = 0; key < 12; ++key)
            {
                float rotMajor[12], rotMinor[12];
                for (int i = 0; i < 12; ++i)
                {
                    rotMajor[i] = major[((i - key) % 12 + 12) % 12];
                    rotMinor[i] = minor[((i - key) % 12 + 12) % 12];
                }
                const float cMaj = correlate (chroma, rotMajor);
                const float cMin = correlate (chroma, rotMinor);

                if (cMaj > best) { best = cMaj; r.keyRoot = key; r.minor = false; }
                if (cMin > best) { best = cMin; r.keyRoot = key; r.minor = true; }
            }

            r.keyFound = true;
            r.keyConfidence = juce::jlimit (0.0f, 1.0f, best);
        }
    }

    return r;
}
