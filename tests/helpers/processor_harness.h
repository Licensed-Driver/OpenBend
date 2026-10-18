#pragma once

#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <map>
#include <vector>

// Test notes sent like regular MIDI from a keyboard
[[maybe_unused]] static juce::MidiMessage noteOn (int note)
{
    return juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (100));
}

[[maybe_unused]] static juce::MidiMessage noteOff (int note)
{
    return juce::MidiMessage::noteOff (1, note);
}

// Same kind of pitch wheel bend that OpenBend sends
[[maybe_unused]] static int pitchWheelFor (double semitones)
{
    return juce::jlimit (0, 16383, 8192 + static_cast<int> ((semitones / 48.0) * 8192.0));
}

// For checking how far the JI offset is from the original 12-TET note
[[maybe_unused]] static double justOffset (double ratio, int equalTemperedSemitones)
{
    return 12.0 * std::log2 (ratio) - equalTemperedSemitones;
}

// Checking if it's within a tolerance
[[maybe_unused]] static bool isWithinCents (int pitchWheelValue, double expectedSemitones, double cents)
{
    const double unitsPerCent = 8192.0 / 48.0 / 100.0;
    return std::abs (pitchWheelValue - pitchWheelFor (expectedSemitones)) <= cents * unitsPerCent;
}

// Simulates a MIDI processor sending a block at a time and remembering ntoes and bends for checking
class ProcessorHarness
{
public:
    static constexpr double sampleRate = 44100.0;
    static constexpr int blockSize = 512;

    ProcessorHarness()
    {
        processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
    }

    // Setting gui params
    void setParam (const juce::String& id, float plainValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
    }

    // Runs a given block of MIDI messages
    std::vector<juce::MidiMessage> run (std::initializer_list<juce::MidiMessage> input = {})
    {
        return process (input, false);
    }

    // Same as run but with the plugin bypassed. OpenBend exposes a bypass parameter, so hosts set
    // BYPASS_ON and keep calling processBlock instead of processBlockBypassed. The next run() un-bypasses
    std::vector<juce::MidiMessage> runBypassed (std::initializer_list<juce::MidiMessage> input = {})
    {
        return process (input, true);
    }

    void runEmptyBlocks (int count)
    {
        for (int i = 0; i < count; ++i)
            run();
    }

    int channelOf (int note) const
    {
        REQUIRE (noteChannel.count (note) == 1);
        return noteChannel.at (note);
    }

    int bendOnChannel (int channel) const
    {
        REQUIRE (channelBend.count (channel) == 1);
        return channelBend.at (channel);
    }

    // Last pitch bend fr the channel this note used/uses
    int bendOf (int note) const { return bendOnChannel (channelOf (note)); }

    OpenBendAudioProcessor processor;

private:
    std::map<int, int> noteChannel;
    std::map<int, int> channelBend;

    std::vector<juce::MidiMessage> process (std::initializer_list<juce::MidiMessage> input, bool bypassed)
    {
        juce::AudioBuffer<float> audio (2, blockSize);
        audio.clear();

        juce::MidiBuffer midi;
        for (const auto& message : input)
            midi.addEvent (message, 0);

        setParam ("BYPASS_ON", bypassed ? 1.0f : 0.0f);
        processor.processBlock (audio, midi);

        std::vector<juce::MidiMessage> output;
        for (const auto metadata : midi)
        {
            auto message = metadata.getMessage();
            if (message.isNoteOn())
                noteChannel[message.getNoteNumber()] = message.getChannel();
            if (message.isPitchWheel())
                channelBend[message.getChannel()] = message.getPitchWheelValue();
            output.push_back (message);
        }
        return output;
    }
};

[[maybe_unused]] static std::vector<juce::MidiMessage> pitchWheelsOnChannel (const std::vector<juce::MidiMessage>& messages, int channel)
{
    std::vector<juce::MidiMessage> result;
    for (const auto& message : messages)
        if (message.isPitchWheel() && message.getChannel() == channel)
            result.push_back (message);
    return result;
}

[[maybe_unused]] static std::vector<juce::MidiMessage> noteOffsIn (const std::vector<juce::MidiMessage>& messages)
{
    std::vector<juce::MidiMessage> result;
    for (const auto& message : messages)
        if (message.isNoteOff())
            result.push_back (message);
    return result;
}

// True if this channel was ended (whether from individual note off or all notes off like bypass)
[[maybe_unused]] static bool voiceEnded (const std::vector<juce::MidiMessage>& messages, int channel, int note)
{
    for (const auto& message : messages)
    {
        if (message.getChannel() != channel)
            continue;
        if (message.isNoteOff() && message.getNoteNumber() == note)
            return true;
        if (message.isAllNotesOff() || message.isAllSoundOff())
            return true;
    }
    return false;
}

[[maybe_unused]] static std::vector<juce::MidiMessage> noteOnsIn (const std::vector<juce::MidiMessage>& messages)
{
    std::vector<juce::MidiMessage> result;
    for (const auto& message : messages)
        if (message.isNoteOn())
            result.push_back (message);
    return result;
}
