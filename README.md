# SimpleAlignmentVST

A VST3 audio plugin that applies per-channel alignment delays and gain adjustments to an 8-channel audio stream. Designed for use on a Raspberry Pi hosted by the **Hang Loose Host** application.

---

## Features

- **8-channel delay alignment** — independent delay per channel with linear interpolation for sub-sample accuracy
- **Per-channel gain trim** — ±12 dB per channel
- **System delay** — global offset added to all channels (0–30 ms)
- **Automatic normalization** — the channel with the lowest delay always gets zero delay; all others are relative to it. Similarly, the loudest channel gets 0 dB and all others are attenuated relative to it. This ensures no unnecessary delay or gain reduction is introduced.
- **Zero latency on the reference channel** — if the effective delay for a channel is 0.0 ms, the audio bypasses the circular buffer entirely
- **Lock control** — prevents accidental changes to any text field
- **Bypass** — hard passthrough with zero processing
- **Persistent settings** — all parameters (including channel names) survive host/system restarts

---

## I/O Layout

| Ch | Default Name    |
|----|-----------------|
| 0  | Front Left      |
| 1  | Front Right     |
| 2  | LFE             |
| 3  | Center          |
| 4  | Surround Left   |
| 5  | Surround Right  |
| 6  | Subwoofer 1     |
| 7  | Subwoofer 2     |

Channel names are user-editable in the UI and persist across restarts.

---

## Processing Logic

```
if bypass:
    in → out (passthrough, no delay or gain)
else:
    norm_delay[ch] = align_delay[ch] - min(align_delay)
    norm_gain[ch]  = chan_gain[ch]   - max(chan_gain)

    effective_delay[ch] = system_delay + norm_delay[ch]

    for each channel:
        apply delay of effective_delay[ch] ms  (linear interpolation)
        apply gain  of norm_gain[ch] dB
```

### Delay implementation

Delays are implemented as per-channel circular buffers with linear interpolation between adjacent samples. Maximum buffer size is 70 ms (30 ms system + 40 ms maximum alignment spread).

When `effective_delay[ch] == 0.0`, the channel bypasses the buffer entirely for guaranteed zero latency.

---

## UI Controls

| Control | Description |
|---------|-------------|
| **Bypass** | Toggle passthrough mode |
| **Lock** | Disable all text entry fields |
| **System Delay** | Global delay offset in ms (0.0–30.0) |
| **Channel / Align / Norm Delay / Gain / Norm Gain** | Per-channel columns (see below) |

### Per-channel columns

| Column | Range | Default | Description |
|--------|-------|---------|-------------|
| Channel | — | See table above | Editable channel description |
| Align (ms) | −20.0 to +20.0 | 0.0 | Raw alignment delay input |
| Norm Delay | — | — | Computed: align − min(align) |
| Gain (dB) | −12.0 to +12.0 | 0.0 | Raw gain input |
| Norm Gain | — | — | Computed: gain − max(gain) |

---

## Building

### Prerequisites

- CMake ≥ 3.22
- C++17 compiler (`g++` ≥ 9 on Raspberry Pi OS)
- JUCE (added as a Git submodule)

```bash
# 1. Clone with JUCE submodule
git clone --recurse-submodules https://github.com/kmarkley/SimpleAlignment.git
cd SimpleAlignment

# 2. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release -j$(nproc)
```

The compiled `.vst3` bundle appears under `build/SimpleAlignment_artefacts/Release/VST3/`.

### Installing on Raspberry Pi

```bash
cp -r build/SimpleAlignment_artefacts/Release/VST3/SimpleAlignment.vst3 ~/.vst3/
```

Then rescan plugins in Hang Loose Host.

### Adding JUCE as a submodule

```bash
git submodule add https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive
```

---

## Project Structure

```
SimpleAlignment/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.h    ← delay lines, normalization, state persistence
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h       ← UI layout, numeric text filters, channel rows
│   └── PluginEditor.cpp
├── JUCE/                    ← submodule
├── .gitignore
└── README.md
```

---

## License

GPL v3 — see `LICENSE` for details. This project links against JUCE, which is licensed under the GNU General Public License v3.
