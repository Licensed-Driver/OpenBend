# OpenBend

OpenBend is an open-source, MPE-based MIDI effect plugin designed to convert standard 12-Tone Equal Temperament (12-TET) MIDI data into pure **Just Intonation** on the fly. Built with the JUCE framework, it functions as a MIDI insert effect that intercepts incoming MIDI notes, analyzes the harmony to find the root, and applies per-note pitch bends to retune chords to Just Intonation ratios.

## Features
- **Dynamic Just Intonation**: Automatically finds the root note of the current chord and retunes all notes to ideal harmonic ratios (e.g., a Perfect 5th is exactly 3:2, a Major 3rd is 5:4).
- **MPE Support**: Utilizes MIDI Polyphonic Expression (MPE) to apply independent pitch bends to each note.
- **Portamento / Glide**: Smoothly glides the pitch between moving chords and intervals with adjustable speed/time (1ms to 2000ms).
- **Customizable Glide Curves**: Choose between Linear, Logarithmic, and Exponential glide curves for different portamento effects.

## How It Works & How It Is Used

### How It Works
OpenBend intercepts standard MIDI input and assigns each note to its own MPE channel (Channels 2-15). It then monitors the active notes to determine the lowest note (the root) of the current chord. For every other note played, the plugin calculates the exact deviation required to reach the Just Intonation frequency relative to that root. It then sends an MPE pitch bend message to the target note's specific channel.

*Note: OpenBend assumes that the receiving synthesizer is configured for MPE and has its pitch bend range set to **±48 semitones***.

### How To Use It
1. **Insert the Plugin**: Load OpenBend onto a MIDI track in your DAW before your chosen synthesizer instrument.
2. **Configure Your Synth**: Ensure the synthesizer receiving MIDI from OpenBend is MPE-compatible and that its pitch bend range is set to **±48 semitones**.
3. **Play Chords**: Play standard 12-TET chords on your MIDI controller. OpenBend will do the rest for you, returning Just Intonation MIDI data to your synthesizer.
4. **Tweak Glide**: Use the UI to enable/disable portamento, adjust the glide speed in milliseconds, and pick the curve type (Linear/Logarithmic/Exponential) that suits your style.

## Installation & Build Instructions

You do not need to write or compile any code to use OpenBend. Pre-compiled binaries are automatically generated for both Windows and Mac!

### 1. Download the Plugin
* Go to the **[Releases](../../releases)** page (look for the "Releases" section on the right side of the GitHub homepage).
* Under the latest version (e.g., `v1.0.0`), download the `.zip` file for your operating system.

### 2. Windows Setup
1. Extract the downloaded `.zip` file.
2. Drag and drop the `OpenBend.vst3` file into your system's VST3 folder:
   `C:\Program Files\Common Files\VST3`

### 3. macOS Setup
1. Extract the downloaded `.zip` file.
2. For Logic Pro / GarageBand, move `OpenBend.component` to your Audio Units folder:
   `/Library/Audio/Plug-Ins/Components`
3. For Ableton / Bitwig / Studio One, move `OpenBend.vst3` to your VST3 folder:
   `/Library/Audio/Plug-Ins/VST3`

*(Mac Troubleshooting: Since this is an indie open-source plugin, Apple's Gatekeeper might flag it. If your DAW refuses to load it, open your Mac's **System Settings > Privacy & Security**, scroll down, and click "Allow Anyway" for OpenBend.)*

### 4. Load it in your DAW
* Restart your DAW or force a plugin rescan.
* OpenBend will show up in your **MIDI Effects** or **MIDI Tools** category, listed under the manufacturer name **Layne Pitman**.
* *Reminder: Place this plugin directly before your MPE-enabled synthesizer in the device chain.*

## Contributing & Feedback

OpenBend is an open-source project, and contributions are highly encouraged! Whether you're an experienced C++/JUCE developer, a music theory enthusiast, or just someone who found a bug, your help is welcome.

- **Found a bug or have a feature request?** Please open an issue on GitHub to let me know what you'd like to see in the future or what needs fixing.
- **Want to contribute code?** Feel free to fork the repository, make your changes, and submit a Pull Request.

I'm always looking for ways to improve the plugin, add new tuning features, or optimize the MPE handling. Don't hesitate to reach out!

## License

This project is licensed under the terms of the GPLv3 License. See the `LICENSE.md` file for details.
