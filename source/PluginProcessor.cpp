/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OpenBendAudioProcessor::OpenBendAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        #if ! JucePlugin_IsMidiEffect
        #if ! JucePlugin_IsSynth
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
        #endif
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        #endif
        ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    lastSentPitchValue.fill(-1);
    glidePhase.fill(1.0);
    currentPitchBends.fill(0.0);
    targetPitchBends.fill(0.0);
    startPitchBends.fill(0.0);
}

OpenBendAudioProcessor::~OpenBendAudioProcessor()
{
}

//==============================================================================
const juce::String OpenBendAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OpenBendAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool OpenBendAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool OpenBendAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double OpenBendAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OpenBendAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int OpenBendAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OpenBendAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String OpenBendAudioProcessor::getProgramName (int index)
{
    return {};
}

void OpenBendAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void OpenBendAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void OpenBendAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OpenBendAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void  OpenBendAudioProcessor::updateChordState() {
    // Just finding the root note of the chord
    currentRootNote = -1;

    for (int i = 0; i < 128; i++) {
        if (activeNotes[i]) {
            currentRootNote = i;
            break;
        }
    }
}

double OpenBendAudioProcessor::mtof(int midiNote) {
    // Helper to go from TET to JI in frequency
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

double OpenBendAudioProcessor::calculateJIPitchBend(int targetNote, int rootNote) {
    // Helper to get the pitch bend required from TET to JI
    if (rootNote == -1 || targetNote % 12 == rootNote % 12) {
        return 0.0;
    }

    // Get semitone interval between root and target
    int interval = (targetNote - rootNote) % 12;
    if (interval < 0) interval += 12;   // Account for those played below root

    // Calculate the ideal frequency
    double rootFreq = mtof(rootNote);
    double idealJIFreq = rootFreq * jiRatios[interval];

    // Account for octaves if the target note is in a different octave
    int octaveDifference = static_cast<int>(std::floor((targetNote - rootNote) / 12.0f));
    idealJIFreq *= std::pow(2.0, octaveDifference);

    // Calculate the standard TET frequency of the target
    double standardFreq = mtof(targetNote);

    // Get the semitone diff with logs
    double pitchBendInSemitones = 12.0 * std::log2(idealJIFreq / standardFreq);

    return pitchBendInSemitones;
}

juce::AudioProcessorValueTreeState::ParameterLayout OpenBendAudioProcessor::createParameterLayout() {
    //Creates our layout

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "PORTAMENTO_ON",
        "Portamento Active",
        true
    ));

    // Create know clamped betwee 1ms and 2000ms logarithmic
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "GLIDE_SPEED",
        "Glide Speed (ms)",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f),
        150.0f
    ));

    juce::StringArray curveTypes;
    curveTypes.add("Linear");
    curveTypes.add("Logarithmic");
    curveTypes.add("Exponential");

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "CURVE_TYPE",
        "Curve Type",
        curveTypes,
        0
    ));

    return layout;
}

void OpenBendAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    bool isPortamentoOn = *apvts.getRawParameterValue("PORTAMENTO_ON") > 0.5f;
    float glideSpeedMs = *apvts.getRawParameterValue("GLIDE_SPEED");
    int curveType = static_cast<int>(*apvts.getRawParameterValue("CURVE_TYPE"));

    // For calculating the smoothing of the portamento
    double sampleRate = getSampleRate();
    if (sampleRate <= 0) sampleRate = 44100.0;  // Just in case

    double glideTimeSeconds = glideSpeedMs / 1000.0;
    double phaseIncrement = 1.0;

    if (glideTimeSeconds > 0.0) {
        // Amount to add per block to reach 1.0 in glideTimeSeconds
        phaseIncrement = buffer.getNumSamples() / (sampleRate * glideTimeSeconds);
    }

    juce::MidiBuffer processedMidi;

    for (const auto metadata : midiMessages) {
        auto message = metadata.getMessage();
        auto samplePosition = metadata.samplePosition;

        if (message.isNoteOn()) {
            int noteNumber = message.getNoteNumber();
            // This is where we calculate the values

            // Kill any overlapping notes so we don't get shuttering
            if (activeNotes[noteNumber]) {
                auto killNote = juce::MidiMessage::noteOff(noteToChannel[noteNumber], noteNumber);
                processedMidi.addEvent(killNote, samplePosition);
            }

            // Find the root note given this value
            activeNotes[noteNumber] = true;
            updateChordState();

            // Give MPE channel
            int currentChannel = nextMpeChannel;
            noteToChannel[noteNumber] = currentChannel;

            // Increment it for next note
            nextMpeChannel++;
            if (nextMpeChannel > 15) nextMpeChannel = 2;

            // Calculate the initial target
            double initialBend = calculateJIPitchBend(noteNumber, currentRootNote);
            targetPitchBends[noteNumber] = initialBend;

            // If portamento is off we snap to the target, but if it's on we wanna slerp (tech lerp but slerp is funnier) to it
            currentPitchBends[noteNumber] = isPortamentoOn ? 0.0 : initialBend;
            startPitchBends[noteNumber] = currentPitchBends[noteNumber];
            glidePhase[noteNumber] = isPortamentoOn ? 0.0 : 1.0;

            // Calculate the pitch bend immediately
            int midiPitchValue = 8192 + static_cast<int>((currentPitchBends[noteNumber] / 48.0) * 8192.0);
            midiPitchValue = juce::jlimit(0, 16383, midiPitchValue);

            // Send that pitch bend before the note on
            auto pitchWheelMessage = juce::MidiMessage::pitchWheel(currentChannel, midiPitchValue);
            processedMidi.addEvent(pitchWheelMessage, samplePosition);
            lastSentPitchValue[noteNumber] = midiPitchValue;    // Save for portamento

            // Now create the note message
            auto mpeNoteOn = juce::MidiMessage::noteOn(currentChannel, noteNumber, message.getFloatVelocity());
            processedMidi.addEvent(mpeNoteOn, samplePosition);
        }
        else if (message.isNoteOff()) {
            int noteNumber = message.getNoteNumber();
            // The user releases a key

            // Crash protection on notes being played when we are activated
            if (!activeNotes[noteNumber]) continue;

            // Update state management
            activeNotes[noteNumber] = false;
            updateChordState();

            // Lookup the channel of this note
            int channelToTurnOff = noteToChannel[noteNumber];

            // Send the note off message
            auto mpeNoteOff = juce::MidiMessage::noteOff(channelToTurnOff, noteNumber);
            processedMidi.addEvent(mpeNoteOff, samplePosition);

            // Clear the channel and pitch mapping
            noteToChannel[noteNumber] = 0;
            lastSentPitchValue[noteNumber] = -1;
        }
        else {
            // Just pass through any other data
            processedMidi.addEvent(message, samplePosition);
        }
    }

    // This is where we actually apply the portamento since we always wanna apply it, not just at the start of the press
    for (int i = 0; i < 128; i++) {
        if (activeNotes[i]) {
            // Recalculate the target in case the root note changed while they were holding the key down
            double newTarget = calculateJIPitchBend(i, currentRootNote);
            // Tracking where we are in the pitch bend to keep pitch bends correct between blocks for each note
            if (newTarget != targetPitchBends[i]) {
                targetPitchBends[i] = newTarget;
                startPitchBends[i] = currentPitchBends[i];
                glidePhase[i] = 0.0;
            }

            // Slide the current pitch by the coefficient to the target
            if (isPortamentoOn && glideTimeSeconds > 0.0) {
                
                glidePhase[i] += phaseIncrement;
                if (glidePhase[i] > 1.0) glidePhase[i] = 1.0;

                double phase = glidePhase[i];
                double t = phase;

                if (curveType == 1) { // If we're using logarithmic curve for portamento
                    t = std::log10(1.0 + 9.0 * phase);
                }
                else if (curveType == 2) { // If we are using exponential curve for portamento
                    // Just a standard exponential slope since we don't have a custom curve creator or anything yet
                    t = (std::exp(phase * 2.30258509) - 1.0) / 9.0;
                }

                currentPitchBends[i] = startPitchBends[i] + (targetPitchBends[i] - startPitchBends[i]) * t;
            }
            else {
                // Otherwise we just snap
                currentPitchBends[i] = targetPitchBends[i];
                glidePhase[i] = 1.0;    // Since we don't wanna be snapping then gliding
            }

            int channel = noteToChannel[i];
            if (channel < 2 || channel > 15) continue;  // Just in case again


            // Convert semitones to MIDI pitch wheel
            int midiPitchValue = 8192 + static_cast<int>((currentPitchBends[i] / 48.0) * 8192.0);

            // Clamp it to MIDI range
            midiPitchValue = juce::jlimit(0, 16383, midiPitchValue);

            // Make sure we only send the message if it's actually going to change something instead of every time
            if (midiPitchValue != lastSentPitchValue[i]) {
                // Portamento for all notes to slide correctly
                auto pitchWheelMessage = juce::MidiMessage::pitchWheel(channel, midiPitchValue);
                processedMidi.addEvent(pitchWheelMessage, 0);

                lastSentPitchValue[i] = midiPitchValue;
            }
        }
    }

    midiMessages.swapWith(processedMidi);
}

//==============================================================================
bool OpenBendAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* OpenBendAudioProcessor::createEditor()
{
    return new OpenBendAudioProcessorEditor (*this);
}

//==============================================================================
void OpenBendAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void OpenBendAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenBendAudioProcessor();
}
