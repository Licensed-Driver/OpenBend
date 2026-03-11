/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
*/
class OpenBendAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    OpenBendAudioProcessor();
    ~OpenBendAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Saved params
    juce::AudioProcessorValueTreeState apvts;
    // Helper to create and return param layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    // State management
    std::array<bool, 128> activeNotes{ false };
    int currentRootNote = -1;
    void updateChordState();

    // MIDI memory
    std::array<int, 128> noteToChannel{ 0 };
    int nextMpeChannel = 2;

    // Just Intonation ratios from TET
    static constexpr std::array<double, 12> jiRatios = {
        1.0,        // 0: Unison (1/1)
        16.0 / 15.0,  // 1: Minor 2nd
        9.0 / 8.0,    // 2: Major 2nd
        6.0 / 5.0,    // 3: Minor 3rd
        5.0 / 4.0,    // 4: Major 3rd
        4.0 / 3.0,    // 5: Perfect 4th
        45.0 / 32.0,  // 6: Tritone
        3.0 / 2.0,    // 7: Perfect 5th
        8.0 / 5.0,    // 8: Minor 6th
        5.0 / 3.0,    // 9: Major 6th
        9.0 / 5.0,    // 10: Minor 7th
        15.0 / 8.0    // 11: Major 7th
    };
    double mtof(int midiNote);
    double calculateJIPitchBend(int targetNote, int rootNote);

    // Starting on portamento boyyyy
    std::array<double, 128> currentPitchBends;
    std::array<double, 128> targetPitchBends;
    std::array<double, 128> startPitchBends;
    std::array<double, 128> glidePhase;

    // Stores the last portamento we sent so that we aren't spamming messages
    std::array<int, 128> lastSentPitchValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenBendAudioProcessor)

};
