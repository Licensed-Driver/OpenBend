#include "helpers/processor_harness.h"

// Reference values for MIDI values, JI ratios, and pitch bends from 12-TET (8192 is centre for plain 12-TET)
//   Root          8192
//   Major 3rd     8169   (5/4, -13.7 cents)
//   Minor 3rd     8218   (6/5, +15.6 cents)
//   Perfect 5th   8195   (3/2,  +2.0 cents)
TEST_CASE ("Reference pitch-wheel values match the hand-computed table", "[tuning]")
{
    CHECK (pitchWheelFor (0.0) == 8192);
    CHECK (pitchWheelFor (justOffset (5.0 / 4.0, 4)) == 8169);
    CHECK (pitchWheelFor (justOffset (6.0 / 5.0, 3)) == 8218);
    CHECK (pitchWheelFor (justOffset (3.0 / 2.0, 7)) == 8195);
}

TEST_CASE ("A single note is sent in plain 12-TET", "[tuning]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60) });

    CHECK (h.bendOf (60) == 8192);
}

TEST_CASE ("A major triad is tuned to just intonation", "[tuning]")
{
    ProcessorHarness h;

    SECTION ("all notes in one block")
    {
        h.run ({ noteOn (60), noteOn (64), noteOn (67) });
    }

    SECTION ("notes arriving in separate blocks")
    {
        h.run ({ noteOn (60) });
        h.run ({ noteOn (64) });
        h.run ({ noteOn (67) });
    }

    CHECK (h.bendOf (60) == 8192);
    CHECK (h.bendOf (64) == pitchWheelFor (justOffset (5.0 / 4.0, 4)));
    CHECK (h.bendOf (67) == pitchWheelFor (justOffset (3.0 / 2.0, 7)));
}

TEST_CASE ("A minor triad is tuned to just intonation", "[tuning]")
{
    ProcessorHarness h;
    h.run ({ noteOn (57), noteOn (60), noteOn (64) }); // A C E

    CHECK (h.bendOf (57) == 8192);
    CHECK (h.bendOf (60) == pitchWheelFor (justOffset (6.0 / 5.0, 3)));
    CHECK (h.bendOf (64) == pitchWheelFor (justOffset (3.0 / 2.0, 7)));
}

TEST_CASE ("An inverted triad still finds the right root", "[tuning]")
{
    ProcessorHarness h;
    h.run ({ noteOn (64), noteOn (67), noteOn (72) }); // E G C, first inversion of C major

    CHECK (h.bendOf (72) == 8192);
    CHECK (h.bendOf (64) == pitchWheelFor (justOffset (5.0 / 4.0, 4)));
    CHECK (h.bendOf (67) == pitchWheelFor (justOffset (3.0 / 2.0, 7)));
}

// Testing for #9 bending back to 12-TET on release
TEST_CASE ("Releasing notes from a chord doesn't retune the notes still held", "[tuning][regression]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64), noteOn (67) });

    const int eChannel = h.channelOf (64);
    const int gChannel = h.channelOf (67);
    const int eBend = h.bendOf (64);
    const int gBend = h.bendOf (67);

    SECTION ("releasing the root")
    {
        auto output = h.run ({ noteOff (60) });

        CHECK (pitchWheelsOnChannel (output, eChannel).empty());
        CHECK (pitchWheelsOnChannel (output, gChannel).empty());

        h.runEmptyBlocks (5);
        CHECK (h.bendOf (64) == eBend);
        CHECK (h.bendOf (67) == gBend);
    }

    SECTION ("releasing down to the last held note")
    {
        auto output = h.run ({ noteOff (60), noteOff (67) });

        CHECK (pitchWheelsOnChannel (output, eChannel).empty());

        h.runEmptyBlocks (5);
        CHECK (h.bendOf (64) == eBend);
    }
}

TEST_CASE ("Each held note gets its own MPE channel", "[channels]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64), noteOn (67) });

    const int c = h.channelOf (60);
    const int e = h.channelOf (64);
    const int g = h.channelOf (67);

    CHECK (c != e);
    CHECK (e != g);
    CHECK (c != g);

    for (int channel : { c, e, g })
    {
        CHECK (channel >= 2);
        CHECK (channel <= 15);
    }
}

TEST_CASE ("MPE channels rotate through 2-15 and wrap back to 2", "[channels]")
{
    ProcessorHarness h;
    std::vector<int> channels;

    for (int i = 0; i < 15; ++i)
    {
        h.run ({ noteOn (60) });
        channels.push_back (h.channelOf (60));
        h.run ({ noteOff (60) });
    }

    const std::vector<int> expected { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 2 };
    CHECK (channels == expected);
}

TEST_CASE ("A note-off ends only the released note, on its own channel", "[channels]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64) });

    auto noteOffs = noteOffsIn (h.run ({ noteOff (64) }));

    REQUIRE (noteOffs.size() == 1);
    CHECK (noteOffs[0].getNoteNumber() == 64);
    CHECK (noteOffs[0].getChannel() == h.channelOf (64));
}

TEST_CASE ("Non-note messages pass straight through", "[passthrough]")
{
    ProcessorHarness h;
    const auto modWheel = juce::MidiMessage::controllerEvent (1, 1, 64);

    auto output = h.run ({ modWheel });

    bool found = false;
    for (const auto& message : output)
        if (message.isController() && message.getControllerNumber() == 1 && message.getControllerValue() == 64 && message.getChannel() == 1)
            found = true;

    CHECK (found);
}

TEST_CASE ("Legato slides a new note in from the held note", "[legato][timing]")
{
    ProcessorHarness h;
    h.setParam ("LEGATO_ON", 1.0f);

    h.run ({ noteOn (60) });
    juce::Thread::sleep (60);
    auto output = h.run ({ noteOn (62) });

    const int dChannel = h.channelOf (62);
    auto firstBends = pitchWheelsOnChannel (output, dChannel);
    REQUIRE_FALSE (firstBends.empty());

    // D starts at C's pitch, about 2 semitones down
    CHECK (isWithinCents (firstBends.front().getPitchWheelValue(), -2.0, 50.0));

    // Then slides up over several blocks without jumping backwards
    std::vector<int> slide { firstBends.back().getPitchWheelValue() };
    for (int i = 0; i < 40; ++i)
        for (const auto& message : pitchWheelsOnChannel (h.run(), dChannel))
            slide.push_back (message.getPitchWheelValue());

    CHECK (slide.size() > 5);
    CHECK (std::is_sorted (slide.begin(), slide.end()));
    CHECK (isWithinCents (h.bendOf (62), 0.0, 50.0));
}

TEST_CASE ("Notes pressed together don't legato into each other", "[legato]")
{
    ProcessorHarness h;
    h.setParam ("LEGATO_ON", 1.0f);

    h.run ({ noteOn (60), noteOn (62) });

    CHECK (h.channelOf (60) != h.channelOf (62));
    CHECK (isWithinCents (h.bendOf (62), 0.0, 50.0));
}

TEST_CASE ("True legato bends the held voice instead of starting a new one", "[legato][timing]")
{
    ProcessorHarness h;
    h.setParam ("LEGATO_ON", 1.0f);
    h.setParam ("TRUE_LEGATO_ON", 1.0f);

    h.run ({ noteOn (60) });
    const int cChannel = h.channelOf (60);
    juce::Thread::sleep (60);

    auto output = h.run ({ noteOn (62) });
    CHECK (noteOnsIn (output).empty());

    h.runEmptyBlocks (40);
    CHECK (isWithinCents (h.bendOnChannel (cChannel), 2.0, 50.0));

    // Lifting the old key keeps the voice going since D now owns it
    CHECK (noteOffsIn (h.run ({ noteOff (60) })).empty());

    // Releasing D ends the original voice
    auto noteOffs = noteOffsIn (h.run ({ noteOff (62) }));
    REQUIRE (noteOffs.size() == 1);
    CHECK (noteOffs[0].getChannel() == cChannel);
    CHECK (noteOffs[0].getNoteNumber() == 60);
}

// Bypassing passes the raw channel 1 note-offs through, which don't match the MPE channels the notes were started on, so the synth doesn't know to stop them.
TEST_CASE ("Notes released while bypassed are still turned off", "[bypass][regression]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64), noteOn (67) });

    std::vector<juce::MidiMessage> output;

    SECTION ("keys released while bypassed")
    {
        output = h.runBypassed ({ noteOff (60), noteOff (64), noteOff (67) });
    }

    SECTION ("keys released while bypassed, then the plugin is re-enabled")
    {
        output = h.runBypassed ({ noteOff (60), noteOff (64), noteOff (67) });
        for (const auto& message : h.run())
            output.push_back (message);
    }

    for (int note : { 60, 64, 67 })
    {
        INFO ("note " << note << " on channel " << h.channelOf (note));
        CHECK (voiceEnded (output, h.channelOf (note), note));
    }
}

TEST_CASE ("Notes held through a bypass are ended by the time they're released", "[bypass]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64) });

    std::vector<juce::MidiMessage> output;
    for (const auto& block : { h.runBypassed(), h.runBypassed(), h.run ({ noteOff (60), noteOff (64) }) })
        output.insert (output.end(), block.begin(), block.end());

    CHECK (voiceEnded (output, h.channelOf (60), 60));
    CHECK (voiceEnded (output, h.channelOf (64), 64));
}

TEST_CASE ("Bypass ends only the voices that were playing", "[bypass]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60), noteOn (64) });

    auto noteOffs = noteOffsIn (h.runBypassed());

    CHECK (voiceEnded (noteOffs, h.channelOf (60), 60));
    CHECK (voiceEnded (noteOffs, h.channelOf (64), 64));
    CHECK (noteOffs.size() == 2);
}

TEST_CASE ("Bypass only ends voices once, not every block", "[bypass]")
{
    ProcessorHarness h;
    h.run ({ noteOn (60) });
    h.runBypassed();

    for (int i = 0; i < 3; ++i)
        CHECK (noteOffsIn (h.runBypassed()).empty());
}

TEST_CASE ("MIDI passes through untouched while bypassed", "[bypass]")
{
    ProcessorHarness h;
    h.runBypassed();

    auto output = h.runBypassed ({ noteOn (60) });

    REQUIRE (output.size() == 1);
    CHECK (output[0].isNoteOn());
    CHECK (output[0].getChannel() == 1);
    CHECK (output[0].getNoteNumber() == 60);
}

TEST_CASE ("Bypassing doesn't leave phantom notes to glide from", "[bypass][glide]")
{
    ProcessorHarness h;
    h.setParam ("GLIDE_ON", 1.0f);

    // Nothing was ever played, so after a bypass round trip the first note has nothing to glide from
    h.runBypassed();
    h.runBypassed();
    h.run();
    auto output = h.run ({ noteOn (62) });

    auto bends = pitchWheelsOnChannel (output, h.channelOf (62));
    REQUIRE_FALSE (bends.empty());
    CHECK (bends.front().getPitchWheelValue() == 8192);
}

TEST_CASE ("Non-note messages pass straight through while bypassed", "[bypass][passthrough]")
{
    ProcessorHarness h;
    h.runBypassed();

    const auto sustain = juce::MidiMessage::controllerEvent (1, 64, 127);
    auto output = h.runBypassed ({ sustain });

    CHECK (noteOnsIn (output).empty());
    REQUIRE (output.size() == 1);
    CHECK (output[0].isController());
    CHECK (output[0].getControllerNumber() == 64);
}

TEST_CASE ("A note started while bypassed still ends after bypass is turned off", "[bypass]")
{
    ProcessorHarness h;
    h.runBypassed();
    auto started = noteOnsIn (h.runBypassed ({ noteOn (60) }));
    REQUIRE (started.size() == 1);
    const int channel = started[0].getChannel();

    h.run();
    auto output = h.run ({ noteOff (60) });

    CHECK (voiceEnded (output, channel, 60));
}

TEST_CASE ("Notes from a bypass don't linger and affect tuning afterwards", "[bypass][tuning]")
{
    ProcessorHarness h;

    // Get C stuck in the chord change cache, then bypass and release it
    h.setParam ("GLIDE_ON", 1.0f);
    h.setParam ("GLIDE_SEARCH_MODE", 5.0f);
    h.setParam ("CHORD_CHANGE_DEBOUNCE", 500.0f);
    h.run ({ noteOn (60) });
    h.runBypassed();
    h.runBypassed ({ noteOff (60) });
    h.run();

    // E played on its own afterwards has nothing to be tuned against
    h.setParam ("GLIDE_ON", 0.0f);
    h.run ({ noteOn (64) });

    CHECK (h.bendOf (64) == 8192);
}

TEST_CASE ("True glide still ends notes after their release is debounced", "[glide][timing]")
{
    ProcessorHarness h;
    h.setParam ("GLIDE_ON", 1.0f);
    h.setParam ("TRUE_GLIDE_ON", 1.0f);

    h.run ({ noteOn (60) });
    const int channel = h.channelOf (60);

    // True glide holds note-offs for the match window (50ms default) before letting them through
    auto output = h.run ({ noteOff (60) });
    juce::Thread::sleep (80);
    for (const auto& message : h.run())
        output.push_back (message);

    CHECK (voiceEnded (output, channel, 60));
}
