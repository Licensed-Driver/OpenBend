# OpenBend

OpenBend is an open-source, MPE-based MIDI effect that retunes the chords you play from 12-tone equal temperament (12-TET) to **Just Intonation**, live. It sits in front of an MPE synthesizer, works out the root of whatever chord is held, and sends each note its own pitch bend so the intervals land on pure ratios.

Built with [JUCE](https://juce.com). Available as AU, VST3, CLAP and Standalone.

## Contents

- [What it does](#what-it-does)
- [Requirements](#requirements)
- [Install](#install)
- [Quick start](#quick-start)
- [Logic Pro setup](#logic-pro-setup)
- [Hearing the difference](#hearing-the-difference)
- [Controls](#controls)
- [Tuning table](#tuning-table)
- [Limitations](#limitations)
- [Building from source](#building-from-source)
- [Contributing](#contributing)
- [License](#license)

## What it does

1. Every incoming note is moved to its own MIDI channel (2–15), MPE style.
2. The held notes are scored to find the most likely root: a candidate gains points for each other note that sits a perfect fifth, major third, minor third or minor seventh above it. If nothing scores, the lowest note is the root.
3. Every other note gets a pitch bend that moves it from its 12-TET pitch to the pure ratio above that root. The bend is sent just before the note-on.
4. When the chord changes, the root is re-evaluated and held notes are re-bent to match.

For a C major triad that means:

| Note | Bend | Result |
|---|---|---|
| C | none | root |
| E | −13.7 cents | pure major third (5:4) |
| G | +2.0 cents | pure fifth (3:2) |

## Requirements

OpenBend only does something useful if the instrument after it:

- **supports MPE** (one voice per MIDI channel, with per-channel pitch bend), and
- has its **per-note pitch bend range set to ±48 semitones**.

A synth without MPE applies pitch bend to all voices at once, so it cannot be retuned note by note. A bend range other than 48 will produce the wrong tuning.

## Install

Download the latest build from the **[Releases](../../releases)** page.

| Platform | File | What to do |
|---|---|---|
| macOS | `OpenBend-<version>-macOS.pkg` | Run the installer. It installs the AU, VST3, CLAP and Standalone versions. |
| Windows | `OpenBend-<version>-Windows.exe` | Run the installer. |
| Linux | `OpenBend-<version>-Linux.zip` | Extract and copy the plugin into your plugin folder (for example `~/.vst3`). |

The builds are not code-signed. On macOS, if the installer is blocked, open **System Settings → Privacy & Security** and choose **Open Anyway**. On Windows, SmartScreen may ask you to confirm.

After installing, restart your DAW or rescan plugins. OpenBend is listed under the manufacturer **Layne Pitman**.

## Quick start

1. Put OpenBend **before** your instrument:
   - **Logic Pro**: the **MIDI FX** slot of a software instrument track.
   - **Hosts with MIDI/note effect slots** (Bitwig, Reaper, …): insert it ahead of the instrument.
   - **Hosts without them**: load OpenBend on its own track and route that track's MIDI output to the instrument track.
2. Switch the instrument to MPE mode and set its per-note bend range to **48**.
3. Play chords as usual.

## Logic Pro setup

OpenBend appears in the **MIDI FX** slot under **Audio Units → Layne Pitman → OpenBend**. Versions up to 1.1.1 were registered as an audio effect and did not show up there; update to 1.1.2 or later.

For Logic's own instruments (Alchemy, Sampler, Retro Synth, ES2, Sculpture and others), MPE is switched on in the plug-in's extended parameters. The names below may differ slightly between Logic versions.

1. Open the instrument and click the disclosure triangle at the bottom-left of its window.
2. Set **MIDI Mono Mode** to **On (with common base channel 1)**.
3. Set **Mono Mode Pitch Range** to **48**.

To check what OpenBend is sending, add Logic's **Scripter** after it in the MIDI FX chain and trace events. In a C major triad you should see pitch bend values of about **−23** on the E and **+3** on the G. Logic shows these as "0%" because it rounds; the values are correct.

## Hearing the difference

The retuning is small (up to about 18 cents), so the sound matters more than the chord.

- **Best chord for a big shift**: a dominant seventh such as C–E–G–B♭. The third drops and the seventh rises, widening the gap between them by about 31 cents.
- **Best chord for hearing beating disappear**: a major triad spread wide, such as C3–G3–E4.
- **Sound**: a plain sustained patch (single saw, organ) with no chorus, detune, vibrato or heavy reverb.
- **Register**: around C3–C5.
- **Comparing**: play the chord with OpenBend on, release, bypass it, and play again.

## Controls

All pitch movement is off by default: notes are simply retuned the moment they start.

| Section | Controls | Purpose |
|---|---|---|
| Portamento | Active, Speed (1–2000 ms), Curve | Slides held notes to their new tuning when the root changes, rather than jumping. |
| Glide | Active, True Glide, Speed, Curve, Range (semitones), Time Range, Search Mode | Slides from a recently played note to a new one within the given range and time window. |
| Legato | Active, True Legato, Speed, Curve, Range, Search Mode | Bends an already sounding note to the new pitch rather than starting a new one. |
| Windows | True Glide Window, Chord Change Window (ms) | How long to wait for the rest of a chord to arrive before deciding what moves where. |

Curves are Linear, Logarithmic or Exponential. Search modes (Closest, Highest Priority, Lowest Priority, Next High, Next Low, Chord Change) choose which existing note a new note glides or ties from.

## Tuning table

Ratios are measured from the detected root. Bends are relative to 12-TET.

| Interval | Ratio | Bend (cents) |
|---|---|---|
| Minor 2nd | 16:15 | +11.7 |
| Major 2nd | 9:8 | +3.9 |
| Minor 3rd | 6:5 | +15.6 |
| Major 3rd | 5:4 | −13.7 |
| Perfect 4th | 4:3 | −2.0 |
| Tritone | 45:32 | −9.8 |
| Perfect 5th | 3:2 | +2.0 |
| Minor 6th | 8:5 | +13.7 |
| Major 6th | 5:3 | −15.6 |
| Minor 7th | 9:5 | +17.6 |
| Major 7th | 15:8 | −11.7 |

At a ±48 semitone range one pitch bend step is about 0.6 cents, so the bends actually sent are within that of the figures above.

## Limitations

- **Non-MPE synths do not work.** This includes most classic polysynths and their clones, which apply pitch bend globally.
- **Detuned and chorused patches hide the effect.** A unison detune of 10 cents or more is as wide as the correction itself.
- **At most 14 notes** can be retuned independently (channels 2–15).

## Building from source

Requires CMake 3.25+ and a C++20 compiler (Xcode Command Line Tools on macOS).

```sh
git clone --recurse-submodules https://github.com/Licensed-Driver/OpenBend.git
cd OpenBend
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive` first.

Built plugins land in `build/OpenBend_artefacts/Release/` and are also copied into your user plugin folders. To build one format only, add `--target OpenBend_AU`, `OpenBend_VST3`, `OpenBend_CLAP` or `OpenBend_Standalone`.

On macOS you can validate the Audio Unit with:

```sh
auval -v aumi OBND OB01
```

Pushing a tag of the form `v*` runs the GitHub Actions workflow, which builds all platforms and publishes a release.

## Contributing

Contributions are welcome, whether that is code, tuning ideas or bug reports.

- **Bug or feature request**: open an issue.
- **Code**: fork the repository, make your changes and open a pull request.

## License

GPLv3. See [`LICENSE.md`](LICENSE.md).
