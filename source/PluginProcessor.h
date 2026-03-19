/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// A circular queue to safely store note information
template<typename T, size_t Size>
class AudioQueue {
public:
    void push(const T& item) {
        buffer[writeIndex] = item;
        writeIndex = (writeIndex + 1) % Size;
        if (count < Size) count++;
    }

    // Pop oldest item into the given ref
    bool pop(T& item) {
        if (count == 0) return false;

        item == buffer[readIndex];
        readIndex = (readIndex + 1) % Size;
        count--;
        return true;
    }

    // To get the item without popping it out
    T& get() {
        if (count == 0) return nullptr;
        
        T& item = buffer[readIndex];
        return item;
    }

    int getCount() const { return count; }
    void clear() { writeIndex = 0; readIndex = 0; count = 0; }
private:
    std::array<T, Size> buffer;
    int writeIndex = 0;
    int readIndex = 0;
    int count = 0;
};

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

    struct NotePair {
        juce::MidiMessage target;
        juce::MidiMessage origin;
        int distance;
    };

    struct NoteTime {
        juce::MidiMessage message;
        long long eventTime;
    };

    enum states {
        ON,
        OFF
    };

    struct NoteInfo {
        juce::MidiMessage message;
        long long dispatchTime;
        bool dispatched;
    };

    std::array<NoteInfo, 128> noteDispatch;
    std::array<states, 128> noteStates;

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

    void cacheEvent(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition);
    void setupLegato(juce::MidiMessage& originNote, juce::MidiMessage& targetNote, double initialBend);
    void handleDebounce(juce::MidiBuffer& processedMidi);

    void findLegato(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition);
    void findLegato(std::vector<juce::MidiMessage>& messages, juce::MidiBuffer& processedMidi, int samplePosition);

    void processDebounceNotesOn(juce::MidiBuffer& midiMessages);
    void processDebounceNotesOff(juce::MidiBuffer& midiMessages);

    void handleRegularNote(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition);

    void findGlide(std::vector<juce::MidiMessage>& messages, juce::MidiBuffer& processedMidi, int samplePosition);
    void findGlide(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition);

    void setupSlide(int originNote, juce::MidiMessage& targetMessage, juce::MidiBuffer& processedMidi, int samplePosition);
    void cleanupNote(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition);

    void processNote(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition);

    std::vector<OpenBendAudioProcessor::NotePair> findChord(std::vector<juce::MidiMessage>& targetNotes, std::vector<juce::MidiMessage>& originNotes);

    // Starting on portamento boyyyy
    std::array<double, 128> currentPitchBends;
    std::array<double, 128> targetPitchBends;
    std::array<double, 128> startPitchBends;
    std::array<double, 128> slidePhase;

    // Note gliding!
    std::array<double, 128> targetGlideBends;
    std::array<long long, 128> glideTimers;

    // For tracking time between sample blocks
    double lastTime;

    // Hardcoded window for legato to wait before considering a note
    double legatoWaitMs;
    // For storing when a key was pressed for that legato timing
    std::array<double, 128> legatoTimers;
    // For chord change logic
    std::array<double, 128> noteEndTimes;

    // When we want to do true legato we need to make the new key tell the old key to not stop playing
    // Then when the old key gets a noteOff, we instead just ignore it until the G gets a noteOff
    std::array<int, 128> hijackingNote; 
    std::array<int, 128> hijackedBy;

    long long chordChangeDebounceDeadline;

    // Stores the last portamento we sent so that we aren't spamming messages
    std::array<int, 128> lastSentPitchValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenBendAudioProcessor)

};
