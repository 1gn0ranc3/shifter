# Shifter

![build](https://github.com/1gn0ranc3/shifter/actions/workflows/build.yml/badge.svg)

Low-latency pitch shifter VST3 plugin for electric guitar.

Personal project targeting 8-string drop tunings with discrete semitone steps (0 to -12).
Work in progress — currently at milestone **M0 (skeleton)**.

## Download prebuilt binaries

Every push to `main` produces VST3 bundles as GitHub Actions artifacts for macOS (arm64)
and Windows (x64). Grab the latest from the [Actions tab](../../actions/workflows/build.yml):
pick the most recent successful run and download under **Artifacts**.

Tagged releases (`v*`) are also published to the [Releases page](../../releases).

### Install

- **macOS**: unzip, drop `Shifter.vst3` into `~/Library/Audio/Plug-Ins/VST3/`.
  Plugin is ad-hoc code-signed (no Apple Developer ID). Gatekeeper may complain on first use —
  right-click `Shifter.vst3` → Open, or allow it from System Settings → Privacy & Security.
- **Windows**: unzip, place `Shifter.vst3` into `C:\Program Files\Common Files\VST3\` (needs
  admin) or `%LOCALAPPDATA%\Programs\Common\VST3\`. The plugin is unsigned — your DAW may
  prompt the first time you load it.

## Milestones

- **M0** — Skeleton: VST3 loads, pass-through audio, mix knob.
- **M1** — Baseline shifter: phase vocoder, -2 st hardcoded.
- **M2** — Transient preservation + 13 shift values (reverted: transients was wrong for pure replacement).
- **M2.5** — Phase-locked vocoder, 512-sample window (~10.7 ms), shift-only UI. ← current
- **M3** — Dictionary decomposition + calibration UX.
- **M4** — Online learning (background thread, EMA, novelty capture).
- **M5** — Offline retrain.
- **M6** — UI polish.

## Build

Requires:
- CMake 3.22+
- Xcode command line tools (clang 15+)
- First-time build downloads JUCE 8 (~200 MB) and Catch2 via FetchContent.

```sh
cmake --preset debug
cmake --build build/debug
```

The built VST3 is at `build/debug/src/Shifter_artefacts/Debug/VST3/Shifter.vst3`
and is auto-copied to `~/Library/Audio/Plug-Ins/VST3/Shifter.vst3`.

Point Studio One at that location (or just rescan plugins).

## Test

```sh
cmake --build build/debug --target shifter_tests
ctest --preset debug
```

## CLion

Open the project directory. CLion picks up `CMakePresets.json` automatically — select
the `debug` preset in the toolbar. First configure will take a few minutes while JUCE
is fetched.

## License

GPLv3. JUCE's non-paid licence requires the plugin to be GPL (which is why
`JUCE_DISPLAY_SPLASH_SCREEN` can legitimately be set to `0`).
