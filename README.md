# THUMPER

**KICK IN. EVERYTHING ELSE OUT.**

THUMPER is an aggressive, one-knob ducking and pumping effect. Its **THUMP**
control sets the gain-reduction depth (0 to -20 dB), release (50 to 500 ms),
and transient-detection sensitivity. With no active external sidechain, the
plugin detects low-frequency hits in its input. A connected, active sidechain
takes priority and is exposed as the host's native stereo sidechain bus.

The interface uses the original `Assets/background.png` artwork, preserves its
aspect ratio, and has a fixed 1152 x 768 logical-pixel size. The meter displays
the current gain reduction; the bottom status changes between orange **AUTO
DETECT** and green **SIDECHAIN CONNECTED**.

## Formats and platforms

- VST3 and Standalone on Windows, macOS, and Linux
- AU on macOS
- macOS Universal (arm64 and x86_64) by default; override with
  `-DTHUMPER_MACOS_UNIVERSAL=OFF` or set `CMAKE_OSX_ARCHITECTURES` yourself
- JUCE 8.0.6 is fetched and pinned by CMake

## Build

Requirements: CMake 3.22+, a C++17 compiler, Git, and the platform build tools.
JUCE's platform dependencies (for example, ALSA and X11 development packages
on Linux) must also be installed. CMake downloads JUCE on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target THUMPER_VST3 THUMPER_Standalone
ctest --test-dir build --output-on-failure
```

On macOS, the default build also generates `THUMPER_AU`. To create and package a
Universal Release containing AU, VST3, and Standalone bundles, use the macOS
preset (requires Xcode):

```sh
cmake --preset macos-universal-release
cmake --build --preset macos-universal-release
```

The package target verifies both architectures in all three executables and
creates `build/macos-universal-release/THUMPER-macOS.zip`.

The `macOS Universal Release` GitHub Actions workflow runs on pushes to `main`,
manual dispatches, and version tags (`v*`). It uploads the ZIP as a workflow
artifact. For version tags, it also creates or updates the matching GitHub
Release and attaches `THUMPER-macOS.zip`.

To use a separately installed JUCE package instead of fetching it:

```sh
cmake -S . -B build -DTHUMPER_FETCH_JUCE=OFF -DCMAKE_PREFIX_PATH=/path/to/JUCE
```

## Audio and state

The plugin has stereo main input/output buses and an optional stereo input bus
named **Sidechain**. Hosts that support plug-in sidechains (including Logic Pro)
can use their native sidechain source selector. Sidechain activity is detected
from its incoming RMS level; otherwise the input detector drives the pump.

THUMP is a single versioned AudioProcessorValueTreeState parameter, so host
automation, plug-in state save/restore, and the editor attachment use the same
parameter. The audio path uses a low-frequency transient detector and an
exponential gain-reduction recovery envelope; it does not use FFT, lookahead,
or oversampling.
