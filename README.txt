Sample Pitcher - VST3 for FL Studio
===================================
Needs: Visual Studio 2022 (Desktop C++ workload), CMake 3.22+, Git, internet (CMake downloads JUCE).

Build (x64 Developer PowerShell, inside this folder):
  cmake -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release

Result:
  build\SamplePitcher_artefacts\Release\VST3\Sample Pitcher.vst3

Install:
  Copy "Sample Pitcher.vst3" to  C:\Program Files\Common Files\VST3\
  FL Studio: Options > Manage plugins > Find installed plugins (verify) > add "Sample Pitcher" as instrument.

Use:
  Drop a ~5 s sound (wav/aiff/flac/ogg) on the window (or press Browse).
  Plugin detects pitch + key, sets root note/fine tune automatically.
  Play it in the piano roll: root note plays the sound unchanged, other notes pitch it up/down.
  FL naming: MIDI 60 = C5.

No Visual Studio? Build in the cloud instead:
  1. Create a free GitHub repo, upload this whole folder (including the .github folder).
  2. Open the Actions tab > "Build VST3" > wait for the green check (a few minutes).
  3. Download the "SamplePitcher-VST3" artifact, unzip, copy "Sample Pitcher.vst3" to
     C:\Program Files\Common Files\VST3\
