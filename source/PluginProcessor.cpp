/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <ranges>
#include <juce_audio_basics/juce_audio_basics.h>

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
    slidePhase.fill(1.0);
    currentPitchBends.fill(0.0);
    targetPitchBends.fill(0.0);
    startPitchBends.fill(0.0);
    targetGlideBends.fill(0.0);
    legatoTimers.fill(0.0);
    glideTimers.fill(0.0);

    for (int i = 0; i < 128; i++) {
        hijackingNote[i] = i;
        hijackedBy[i] = i;
    }

    legatoWaitMs = 30;

    chordChangeDebounceDeadline = 0;
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

void OpenBendAudioProcessor::updateChordState() {
    // We are using a more sophisticated scoring root note system cuz I have since learned that the last way was kinda ass
    currentRootNote = -1;
    int bestScore = -1;
    int lowestNote = -1;

    for (int candidate = 0; candidate < 128; candidate++) {
        // Only evaluating notes that are active and not being hijacked
        if (!activeNotes[candidate] || hijackedBy[candidate] != candidate) continue;

        // Just in case the user is playing some fuck ass chord we fall back to the lowetst
        if (lowestNote == -1) lowestNote = candidate;

        int score = 0;

        // Compare every other active note against the current candidate
        for (int other = 0; other < 128; other++) {
            if (candidate == other || !activeNotes[other] || hijackedBy[other] != other) continue;

            // Get the interval
            int interval = (other - candidate) % 12;
            if (interval < 0) interval += 12; // Account for notes lower than the candidate

            // Our weighting is based on harmonic strength (perfect 5th is the best since it's the foundation of the circle of 5ths and mogs the rest of music theory ig)
            if (interval == 7) score += 3;       // Perfect 5th
            else if (interval == 4) score += 2;  // Major 3rd
            else if (interval == 3) score += 2;  // Minor 3rd
            else if (interval == 10) score += 1; // Minor 7th (Dominant)
        }

        // Base notes first and > so that things like inverted notes have to be strictly a better candidate
        if (score > bestScore) {
            bestScore = score;
            currentRootNote = candidate;
        }
    }

    // Base note fallback
    if (bestScore == 0 && lowestNote != -1) {
        currentRootNote = lowestNote;
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
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>("PORTAMENTO_ON", "Portamento Active", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("PORT_SPEED", "Portamento Speed (ms)", juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 150.0f));
    
    juce::StringArray curveTypes;
    curveTypes.add("Linear");
    curveTypes.add("Logarithmic");
    curveTypes.add("Exponential");

    layout.add(std::make_unique<juce::AudioParameterChoice>("PORT_CURVE_TYPE", "Portamento Curve Type", curveTypes, 0));
    
    layout.add(std::make_unique<juce::AudioParameterBool>("GLIDE_ON", "Glide Active", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("GLIDE_SPEED", "Glide Speed (ms)", juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 150.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("GLIDE_CURVE_TYPE", "Glide Curve Type", curveTypes, 0));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("GLIDE_RANGE", "Glide Range (Semitones)", juce::NormalisableRange<float>(1.0f, 24.0f, 1.0f), 12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("GLIDE_TIME_RANGE", "Glide Time Range (s)", juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f), 500.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>("LEGATO_ON", "Legato Active", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("TRUE_LEGATO_ON", "True Legato Active", false));

    juce::StringArray searchModes;
    searchModes.add("Closest");
    searchModes.add("Highest Priority");
    searchModes.add("Lowest Priority");
    searchModes.add("Next High");
    searchModes.add("Next Low");
    searchModes.add("Chord Change (True Glide)");
    layout.add(std::make_unique<juce::AudioParameterChoice>("LEGATO_SEARCH_MODE", "Legato Search Mode", searchModes, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("GLIDE_SEARCH_MODE", "Glide Search Mode", searchModes, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("LEGATO_SPEED", "Legato Speed (ms)", juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 150.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("LEGATO_CURVE_TYPE", "Legato Curve Type", curveTypes, 0));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("TRUE_GLIDE_DEBOUNCE", "True Glide Match Window (ms)", juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f), 50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "CHORD_CHANGE_DEBOUNCE",
        "Chord Change Debounce (ms)",
        juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "LEGATO_RANGE",
        "Legato Range (Semitones)",
        juce::NormalisableRange<float>(1.0f, 24.0f, 1.0f),
        12.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        "TRUE_GLIDE_ON",
        "True Glide Active",
        false));

    return layout;
}

void OpenBendAudioProcessor::cacheEvent(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition) {

    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    bool isGlideTrue = *apvts.getRawParameterValue("TRUE_GLIDE_ON") > 0.5f;

    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;
    bool isLegatoOn = *apvts.getRawParameterValue("LEGATO_ON") > 0.5f;
    if (message.isNoteOn()) {
        int noteNumber = message.getNoteNumber();
        // if we have the chord change search mode selected we use the cache for one state change, otherwise just push that shit out
        if ((isGlideOn && glideSearchMode == 5) || (isLegatoOn && legatoSearchMode == 5)) {
            const long long chordChangeDebounce = static_cast<long long>(*apvts.getRawParameterValue("CHORD_CHANGE_DEBOUNCE"));
            if (!noteDispatch[noteNumber].dispatched) {
                // A note is already cached here so we push it out to be processed and cache the new one
                NoteInfo dispatchNote;
                dispatchNote = noteDispatch[noteNumber];
                processNote(dispatchNote.message, processedMidi, 0);
            }

            // If it ain't 0 we gonna be in a debounce for a chord change
            if (chordChangeDebounceDeadline == 0) {
                chordChangeDebounceDeadline = juce::Time::currentTimeMillis() + chordChangeDebounce;
            }

            noteDispatch[noteNumber] = {
                message,
                chordChangeDebounceDeadline,
                false
            };
        }
        else {
            if (!noteDispatch[noteNumber].dispatched) {
                // A note is already cached here so we push it out to be processed and cache the new one
                NoteInfo dispatchNote;
                dispatchNote = noteDispatch[noteNumber];
                processNote(dispatchNote.message, processedMidi, 0);
            }
            noteDispatch[noteNumber] = {
                message,
                juce::Time::currentTimeMillis(),
                true // Must be true so it doesn't get processed a second time!
            };
            // Otherwise we just push it in now
            processNote(message, processedMidi, samplePosition);
        }
    }
    else if (message.isNoteOff()) {
        int noteNumber = message.getNoteNumber();
        // if we have the true glide on we gotta get that note in the cache to wait for its turn >:(
        if (isGlideOn && isGlideTrue) {
            const long long trueGlideDebounceMs = static_cast<long long>(*apvts.getRawParameterValue("TRUE_GLIDE_DEBOUNCE"));
            if (!noteDispatch[noteNumber].dispatched) {
                NoteInfo dispatchNote;
                dispatchNote = noteDispatch[noteNumber];
                processNote(dispatchNote.message, processedMidi, 0);
            }

            noteDispatch[noteNumber] = {
                message,
                juce::Time::currentTimeMillis() + trueGlideDebounceMs,
                false
            };
        }
        else {
            if (!noteDispatch[noteNumber].dispatched) {
                // A note is already cached here so we push it out to be processed and cache the new one
                NoteInfo dispatchNote;
                dispatchNote = noteDispatch[noteNumber];
                processNote(dispatchNote.message, processedMidi, 0);
            }
            noteDispatch[noteNumber] = {
                message,
                juce::Time::currentTimeMillis(),
                false
            };
            noteDispatch[noteNumber] = {
                message,
                juce::Time::currentTimeMillis(),
                true // Processed immediately so mark da bih dispatched
            };
            // Again if it ain't caching we just push that bad boy through
            processNote(message, processedMidi, samplePosition);
        }
    }
    else {
        // Neither on or off so we just pass through like diarea after a good taco night ykwm
        processNote(message, processedMidi, samplePosition);
    }
}

void OpenBendAudioProcessor::setupLegato(juce::MidiMessage& originMessage, juce::MidiMessage& targetMessage, double initialBend) {

    int targetNote = targetMessage.getNoteNumber();
    int originNote = originMessage.getNoteNumber();

    // When we release targetNote we want it to stop originNote in the channel
    hijackingNote[targetNote] = hijackingNote[originNote];
    // Since we are just bending to the new note instead of bending back
    startPitchBends[targetNote] = currentPitchBends[originNote];
    currentPitchBends[targetNote] = startPitchBends[targetNote];
    // Since pitch bend is realative we need the bend to our note as well
    targetPitchBends[targetNote] = (hijackingNote[targetNote] - hijackingNote[originNote]) + initialBend;
    slidePhase[targetNote] = 0.0;
    lastSentPitchValue[targetNote] = lastSentPitchValue[originNote];
    // We just hijack the whole channel and the logic is much easier
    noteToChannel[targetNote] = noteToChannel[originNote];
    // Since we don't wanna bend it for the previous note anymore
    noteToChannel[originNote] = 0;
    hijackedBy[originNote] = targetNote;
    hijackedBy[targetNote] = targetNote;
}

void OpenBendAudioProcessor::handleDebounce(juce::MidiBuffer& processedMidi) {

    // Prioritize the key on events so that if we happen to have both go off we still get true glide
    if (chordChangeDebounceDeadline > 0 && chordChangeDebounceDeadline < juce::Time::currentTimeMillis()) {
        chordChangeDebounceDeadline = 0;

        processDebounceNotesOn(processedMidi);

    }
    // All we need to do here is process all the note off messages that we have cached and passed their time
    processDebounceNotesOff(processedMidi);
}

void OpenBendAudioProcessor::findGlide(std::vector<juce::MidiMessage>& messages, juce::MidiBuffer& processedMidi, int samplePosition) {
    bool isGlideTrue = *apvts.getRawParameterValue("TRUE_GLIDE_ON") > 0.5f;
    float glideRange = *apvts.getRawParameterValue("GLIDE_RANGE");
    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));

    std::vector<juce::MidiMessage> noteOffMessages;

    // Vectorize the messages for findChord
    for (const auto& ni : noteDispatch) { 
        bool dispatched = ni.dispatched;
        bool noteoff = ni.message.isNoteOff();
        if (!ni.dispatched && ni.message.isNoteOff()) noteOffMessages.push_back(ni.message);
    }

    // Run the chord finder
    std::vector<NotePair> chordPairs = findChord(messages, noteOffMessages);
    for (auto& pair : chordPairs) {
        // do da gliding ma boy
        if (isGlideTrue) {
            setupLegato(pair.origin, pair.target, calculateJIPitchBend(pair.target.getNoteNumber(), currentRootNote));
        }
        else {
            setupSlide(pair.origin.getNoteNumber(), pair.target, processedMidi, samplePosition);
        }
        // Remove all the notes we made transitions out of
        // Since they're references to the original messages we can just directly remove them
        auto it = std::find_if(messages.begin(), messages.end(),
            [&pair](const juce::MidiMessage& m)
            {
                return m.getNoteNumber() == pair.target.getNoteNumber();
            });
        if (it != messages.end()) messages.erase(it);
    }

    // Handle all the ones we didn't find a trueglide notes for as regular glide
    for (auto& message : messages) {
        findGlide(message, processedMidi, samplePosition);
    }
}

void OpenBendAudioProcessor::findGlide(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition) {


    bool isGlideTrue = *apvts.getRawParameterValue("TRUE_GLIDE_ON") > 0.5f;
    float glideRange = *apvts.getRawParameterValue("GLIDE_RANGE");
    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));

    int noteNumber = message.getNoteNumber();
    double initialBend = calculateJIPitchBend(noteNumber, currentRootNote);

    // Helper lambda to check if a note is valid for Legato
    auto testNote = [&](int checkNote) -> int {
        if (checkNote >= 0 && checkNote < 128)
        {
            if (isGlideTrue && noteDispatch[checkNote].message.isNoteOff() && !noteDispatch[checkNote].dispatched && hijackedBy[checkNote] == checkNote) {
                // Reconstruct origin and target message references for setupLegato
                juce::MidiMessage dummyOrigin = juce::MidiMessage::noteOn(1, checkNote, (juce::uint8)100);
                setupLegato(dummyOrigin, message, initialBend);
                updateChordState();

                return 2; // True glide Success
            }
            else if (!activeNotes[checkNote] &&
                glideTimers[checkNote] > juce::Time::currentTimeMillis() &&
                glideTimers[checkNote] > 0 &&
                hijackedBy[checkNote] == checkNote) {
                glideTimers[checkNote] = 0;
                setupSlide(checkNote, message, processedMidi, samplePosition);
                return 1; // Standard glide Success
            }
        }
        return 0; // Invalid/not glidable
    };

    int res = 0;
    // The switch statement routing the search algorithm
    switch (glideSearchMode) {
    case 0: { // Closest
        for (int i = 1; i <= glideRange; i++) {
            int left = noteNumber - i;
            int right = noteNumber + i;

            if (left < 0 && right > 127) break;

            res = testNote(left);
            if (res > 0) break;
            res = testNote(right);
            if (res > 0) break;
        }
        break;
    }
    case 1: { // Highest Priority
        for (int note = 127; note >= 0; note--) {
            if (std::abs(note - noteNumber) <= glideRange) {
                res = testNote(note);
                if (res > 0) break;
            }
        }
        break;
    }
    case 2: { // Lowest Priority
        for (int note = 0; note <= 127; note++) {
            if (std::abs(note - noteNumber) <= glideRange) {
                res = testNote(note);
                if (res > 0) break;
            }
        }
        break;
    }
    case 3: { // Next High
        for (int note = noteNumber + 1; note <= noteNumber + glideRange && note < 128; note++) {
            res = testNote(note);
            if (res > 0) break;
        }
        break;
    }
    case 4: { // Next Low
        for (int note = noteNumber - 1; note >= noteNumber - glideRange && note >= 0; note--) {
            res = testNote(note);
            if (res > 0) break;
        }
        break;
    }
    }

    if (res == 0) {
        // If we found no glide we just gotta handle it regularly
        handleRegularNote(message, processedMidi, samplePosition);
    }
    // otherwise we done
}

void OpenBendAudioProcessor::findLegato(std::vector<juce::MidiMessage>& messages, juce::MidiBuffer& processedMidi, int samplePosition) {
    bool isLegatoTrue = *apvts.getRawParameterValue("TRUE_LEGATO_ON") > 0.5f;
    float legatoRange = *apvts.getRawParameterValue("LEGATO_RANGE");
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    std::vector<juce::MidiMessage> originNotes;
    // Create our pool of possible notes
    for (int note = 0; note < 128; note++) {
        // Check if the current note is able to be used for legato
        if (activeNotes[note] && legatoTimers[note] < juce::Time::currentTimeMillis() && hijackedBy[note] == note) {
            // Reconstruct a dummy message for the origin note (velocity doesn't matter much here for origin)
            originNotes.push_back(juce::MidiMessage::noteOn(1, note, (juce::uint8)100));
        }
    }

    // Now run the chord slide finder
    std::vector<NotePair> chordPairs = findChord(messages, originNotes);
    

    // Setup true legato
    for (auto& pair : chordPairs) {
        // make-a-da sure we support also doing regular legato with chords (Mario)
        if (isLegatoTrue) {
            setupLegato(pair.origin, pair.target, calculateJIPitchBend(pair.target.getNoteNumber(), currentRootNote));
        }
        else {
            setupSlide(pair.origin.getNoteNumber(), pair.target, processedMidi, samplePosition);
        }
        auto it = std::find_if(messages.begin(), messages.end(),
            [&pair](const juce::MidiMessage& m)
            {
                return m.getNoteNumber() == pair.target.getNoteNumber();
            });
        if (it != messages.end()) messages.erase(it);
    }

    // Try to find individual legato for all the leftovers
    for (const auto& message : messages) {
        findLegato(message, processedMidi, samplePosition);
    }
}

void OpenBendAudioProcessor::findLegato(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition) {

    bool isLegatoTrue = *apvts.getRawParameterValue("TRUE_LEGATO_ON") > 0.5f;
    float legatoRange = *apvts.getRawParameterValue("LEGATO_RANGE");
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    int noteNumber = message.getNoteNumber();
    double initialBend = calculateJIPitchBend(noteNumber, currentRootNote);

    // Helper lambda to check if a note is valid for Legato
    auto testNote = [&](int checkNote) -> int {
        if (checkNote >= 0 && checkNote < 128 &&
            activeNotes[checkNote] &&
            hijackedBy[checkNote] == checkNote &&
            legatoTimers[checkNote] < juce::Time::currentTimeMillis())
        {
            if (isLegatoTrue && hijackedBy[checkNote] == checkNote) {
                // Reconstruct origin and target message references for setupLegato
                juce::MidiMessage dummyOrigin = juce::MidiMessage::noteOn(1, checkNote, (juce::uint8)100);
                setupLegato(dummyOrigin, message, initialBend);
                updateChordState();
                return 2; // True Legato Success
            }
            else {
                setupSlide(checkNote, message, processedMidi, samplePosition);
                return 1; // Standard Legato Success
            }
        }
        return 0; // Invalid
    };

    int res = 0;
    // The switch statement routing the search algorithm
    switch (legatoSearchMode) {
        case 0: { // Closest
            for (int i = 1; i <= legatoRange; i++) {
                int left = noteNumber - i;
                int right = noteNumber + i;
                if (left < 0 && right > 127) break;

                res = testNote(left);
                if (res > 0) break;
                res = testNote(right);
                if (res > 0) break;
            }
            break;
        }
        case 1: { // Highest Priority
            for (int note = 127; note >= 0; note--) {
                if (std::abs(note - noteNumber) <= legatoRange) {
                    res = testNote(note);
                    if (res > 0) break;
                }
            }
            break;
        }
        case 2: { // Lowest Priority
            for (int note = 0; note <= 127; note++) {
                if (std::abs(note - noteNumber) <= legatoRange) {
                    res = testNote(note);
                    if (res > 0) break;
                }
            }
            break;
        }
        case 3: { // Next High
            for (int note = noteNumber + 1; note <= noteNumber + legatoRange && note < 128; note++) {
                res = testNote(note);
                if (res > 0) break;
            }
            break;
        }
        case 4: { // Next Low
            for (int note = noteNumber - 1; note >= noteNumber - legatoRange && note >= 0; note--) {
                res = testNote(note);
                if (res > 0) break;
            }
            break;
        }
    }

    if (res == 0 && *apvts.getRawParameterValue("GLIDE_ON") > 0.5f) {
        // Found no legato so we check glide if they have it enabled
        findGlide(message, processedMidi, samplePosition);
    }
    else if (res == 0) {
        // Otherwise we just need to finish setup
        handleRegularNote(message, processedMidi, samplePosition);
    }
    // otherwise we done
}

void OpenBendAudioProcessor::setupSlide(int originNote, juce::MidiMessage& targetMessage, juce::MidiBuffer& processedMidi, int samplePosition) {
    int targetNote = targetMessage.getNoteNumber();
    double initialBend = calculateJIPitchBend(targetNote, currentRootNote);

    startPitchBends[targetNote] = originNote - targetNote + calculateJIPitchBend(originNote, currentRootNote);
    currentPitchBends[targetNote] = startPitchBends[targetNote];
    targetPitchBends[targetNote] = initialBend;

    // Give MPE channel
    noteToChannel[targetNote] = nextMpeChannel;

    // Increment it for next note
    nextMpeChannel++;
    if (nextMpeChannel > 15) nextMpeChannel = 2;

    // Since we are just gonna highjack the porta loop later
    slidePhase[targetNote] = 0.0;

    // Calculate the pitch bend immediately
    int midiPitchValue = 8192 + static_cast<int>((currentPitchBends[targetNote] / 48.0) * 8192.0);
    midiPitchValue = juce::jlimit(0, 16383, midiPitchValue);

    // Send that pitch bend before the note on
    auto pitchWheelMessage = juce::MidiMessage::pitchWheel(noteToChannel[targetNote], midiPitchValue);
    processedMidi.addEvent(pitchWheelMessage, samplePosition);
    lastSentPitchValue[targetNote] = midiPitchValue;    // Save for portamento

    // Now create the note message
    auto mpeNoteOn = juce::MidiMessage::noteOn(noteToChannel[targetNote], targetNote, targetMessage.getFloatVelocity());
    processedMidi.addEvent(mpeNoteOn, samplePosition);
}

std::vector<OpenBendAudioProcessor::NotePair> OpenBendAudioProcessor::findChord(std::vector<juce::MidiMessage>& targetNotes, std::vector<juce::MidiMessage>& originNotes) {
    // Attempts to match origin notes to the available notes reasonably based on the smallest sum of distances
    using NotePair = OpenBendAudioProcessor::NotePair;

    // Make all possible pairs with their semitone distances
    std::vector<NotePair> possiblePairs;

    for (const auto& target : targetNotes) {
        for (const auto& origin : originNotes) {
            possiblePairs.push_back({
                target,
                origin,
                std::abs(target.getNoteNumber() - origin.getNoteNumber())
            });
        }
    }

    std::sort(possiblePairs.begin(), possiblePairs.end(), [](const NotePair& a, const NotePair& b) {
        return a.distance < b.distance;
    });

    std::vector<NotePair> foundPairs;

    std::vector<int> assignedTargets;
    std::vector<int> hijackedOrigins;

    for (auto& pair : possiblePairs) {
        bool targetTaken = std::find(assignedTargets.begin(), assignedTargets.end(), pair.target.getNoteNumber()) != assignedTargets.end();
        bool originTaken = std::find(hijackedOrigins.begin(), hijackedOrigins.end(), pair.origin.getNoteNumber()) != hijackedOrigins.end();

        if (!targetTaken && !originTaken) {
            pair.distance = 1;  // Signal that we found a pair for this note
            foundPairs.push_back(pair);
            assignedTargets.push_back(pair.target.getNoteNumber());
            hijackedOrigins.push_back(pair.origin.getNoteNumber());
        }

        if (foundPairs.size() == targetNotes.size()) break;
    }

    return foundPairs;
}

void OpenBendAudioProcessor::processDebounceNotesOn(juce::MidiBuffer& processedMidi) {

    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;
    bool isLegatoOn = *apvts.getRawParameterValue("LEGATO_ON") > 0.5f;
    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    std::vector<juce::MidiMessage> noteOnMessages;

    for (auto& ni : noteDispatch) {
        auto message = ni.message;
        // Bruh obvi we gonna skip if there ain't anything to dispatch or if it ain't a note on message or if it's time has not yet come
        if (ni.dispatched || !message.isNoteOn() || ni.dispatchTime > juce::Time::currentTimeMillis()) continue;

        const int noteNumber = message.getNoteNumber();

        // Setup the state that processNote normally handles for us
        legatoTimers[noteNumber] = juce::Time::currentTimeMillis() + legatoWaitMs;
        glideTimers[noteNumber] = 0;
        activeNotes[noteNumber] = true;

        noteOnMessages.push_back(message);
        ni.dispatched = true;
    }

    if (noteOnMessages.empty()) return;

    updateChordState();

    // Route to the chord versions of the legato and glide functions
    if (isLegatoOn && legatoSearchMode == 5) {
        findLegato(noteOnMessages, processedMidi, 0);
    }
    else if (isGlideOn && glideSearchMode == 5) {
        findGlide(noteOnMessages, processedMidi, 0);
    }
    else {
        // Or just back to regular in the case we somehow get here without either being on
        for (auto& msg : noteOnMessages) {
            if (isLegatoOn) findLegato(msg, processedMidi, 0);
            else if (isGlideOn) findGlide(msg, processedMidi, 0);
            else handleRegularNote(msg, processedMidi, 0);
        }
    }

    
}

void OpenBendAudioProcessor::processDebounceNotesOff(juce::MidiBuffer& processedMidi) {

    const auto glideTimeRange = static_cast<juce::int64>(
        std::llround(*apvts.getRawParameterValue("GLIDE_TIME_RANGE")));
    const auto nowMs = juce::Time::currentTimeMillis();

    for (auto& ni : noteDispatch) {
        auto& message = ni.message;
        // Bruh obvi we gonna skip if there ain't anything to dispatch or if it ain't a note on message or if it's time has not yet come
        if (ni.dispatched || !message.isNoteOff() || ni.dispatchTime > juce::Time::currentTimeMillis()) continue;

        const int noteNumber = message.getNoteNumber();

        activeNotes[noteNumber] = false;

        processNote(message, processedMidi, 0);

        ni.dispatched = true;
        updateChordState();
    }
}

void OpenBendAudioProcessor::handleRegularNote(juce::MidiMessage message, juce::MidiBuffer& processedMidi, int samplePosition) {
    bool isPortamentoOn = *apvts.getRawParameterValue("PORTAMENTO_ON") > 0.5f;
    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;

    const int noteNumber = message.getNoteNumber();
    const double initialBend = calculateJIPitchBend(noteNumber, currentRootNote);

    targetPitchBends[noteNumber] = initialBend;

    startPitchBends[noteNumber] = isPortamentoOn ? 0.0 : initialBend;
    currentPitchBends[noteNumber] = startPitchBends[noteNumber];
    
    // Give MPE channel
    noteToChannel[noteNumber] = nextMpeChannel;

    // Increment it for next note
    nextMpeChannel++;
    if (nextMpeChannel > 15) nextMpeChannel = 2;

    // Since we are just gonna highjack the porta loop later
    slidePhase[noteNumber] = isPortamentoOn || isGlideOn ? 0.0 : 1.0;

    // Calculate the pitch bend immediately
    int midiPitchValue = 8192 + static_cast<int>((currentPitchBends[noteNumber] / 48.0) * 8192.0);
    midiPitchValue = juce::jlimit(0, 16383, midiPitchValue);

    // Send that pitch bend before the note on
    auto pitchWheelMessage = juce::MidiMessage::pitchWheel(noteToChannel[noteNumber], midiPitchValue);
    processedMidi.addEvent(pitchWheelMessage, samplePosition);
    lastSentPitchValue[noteNumber] = midiPitchValue;    // Save for portamento

    // Now create the note message
    auto mpeNoteOn = juce::MidiMessage::noteOn(noteToChannel[noteNumber], noteNumber, message.getFloatVelocity());
    processedMidi.addEvent(mpeNoteOn, samplePosition);
}

void OpenBendAudioProcessor::cleanupNote(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition) {

    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;
    const auto glideTimeRange = static_cast<juce::int64>(
        std::llround(*apvts.getRawParameterValue("GLIDE_TIME_RANGE")));

    int noteNumber = message.getNoteNumber();

    targetGlideBends[noteNumber] = 0.0;
    targetPitchBends[noteNumber] = 0.0;
    startPitchBends[noteNumber] = 0.0;
    currentPitchBends[noteNumber] = 0.0;
    lastSentPitchValue[noteNumber] = -1;
    legatoTimers[noteNumber] = 0.0;

    // Update state management
    activeNotes[noteNumber] = false;
    updateChordState();


    if (hijackedBy[noteNumber] == noteNumber) {
        // Assuming that you don't wanna glide from the note if it's been used for true glide or legato
        // Start the timer for this note
        if (isGlideOn) glideTimers[noteNumber] = juce::Time::currentTimeMillis() + glideTimeRange;
        // If we used this note to true legato, we don't turn off the channel

        // Lookup the channel of this note
        int channelToTurnOff = noteToChannel[noteNumber];

        // Send the note off message
        auto mpeNoteOff = juce::MidiMessage::noteOff(channelToTurnOff, hijackingNote[noteNumber]);
        processedMidi.addEvent(mpeNoteOff, samplePosition);

        // Clear the channel and pitch mapping
        noteToChannel[noteNumber] = 0;
    }

    hijackingNote[noteNumber] = noteNumber;
    hijackedBy[noteNumber] = noteNumber;
}

void OpenBendAudioProcessor::processNote(juce::MidiMessage& message, juce::MidiBuffer& processedMidi, int samplePosition) {
    bool isPortamentoOn = *apvts.getRawParameterValue("PORTAMENTO_ON") > 0.5f;
    float portSpeedMs = *apvts.getRawParameterValue("PORT_SPEED");
    int portCurveType = static_cast<int>(*apvts.getRawParameterValue("PORT_CURVE_TYPE"));

    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;
    float glideSpeedMs = *apvts.getRawParameterValue("GLIDE_SPEED");
    int glideCurveType = static_cast<int>(*apvts.getRawParameterValue("GLIDE_CURVE_TYPE"));
    float glideRange = *apvts.getRawParameterValue("GLIDE_RANGE");
    const auto glideTimeRange = static_cast<juce::int64>(
        std::llround(*apvts.getRawParameterValue("GLIDE_TIME_RANGE")));

    bool isLegatoOn = *apvts.getRawParameterValue("LEGATO_ON") > 0.5f;
    bool isLegatoTrue = *apvts.getRawParameterValue("TRUE_LEGATO_ON") > 0.5f;
    float legatoRange = *apvts.getRawParameterValue("LEGATO_RANGE");
    float legatoSpeedMs = *apvts.getRawParameterValue("LEGATO_SPEED");
    int legatoCurveType = static_cast<int>(*apvts.getRawParameterValue("LEGATO_CURVE_TYPE"));

    // How long will we hold off on glide or legato while waiting for finished chord change
    double trueGlideDebounce = *apvts.getRawParameterValue("TRUE_GLIDE_DEBOUNCE");

    double chordChangeDebounce = *apvts.getRawParameterValue("CHORD_CHANGE_DEBOUNCE");

    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    // For calculating the smoothing of the portamento
    double portTimeSeconds = portSpeedMs / 1000.0;
    double glideTimeSeconds = glideSpeedMs / 1000.0;
    double legatoTimeSeconds = legatoSpeedMs / 1000.0;
    double phaseIncrement = 1.0;

    // Updating all the possible glide notes to the time that's passed, since we don't wanna cut it short we update before we set new ones so that the samples can play out.
    double currentTime = juce::Time::currentTimeMillis();

    if (message.isNoteOn()) {

        int noteNumber = message.getNoteNumber();

        // Store it's legato timer so that we don't legato on notes played at the same time
        legatoTimers[noteNumber] = juce::Time::currentTimeMillis() + legatoWaitMs;
        // Stop stale glide timers from leaking through
        glideTimers[noteNumber] = 0;

        // Find the root note
        activeNotes[noteNumber] = true;
        updateChordState();

        // If Legato evaluates, we handle it completely inline now through the refactored findLegato which will handle sending note and regularNote on failures
        if (isLegatoOn) findLegato(message, processedMidi, samplePosition);
        else if (isGlideOn) findGlide(message, processedMidi, samplePosition); // Only glide here if we don't legato
        else handleRegularNote(message, processedMidi, samplePosition);
    }
    else if (message.isNoteOff()) {
        // We are going to defer the note off logic until we finish our true glide debounce
        cleanupNote(message, processedMidi, samplePosition);
    }
    else {
        // Just pass through any other data
        processedMidi.addEvent(message, samplePosition);
    }
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
    float portSpeedMs = *apvts.getRawParameterValue("PORT_SPEED");
    int portCurveType = static_cast<int>(*apvts.getRawParameterValue("PORT_CURVE_TYPE"));

    bool isGlideOn = *apvts.getRawParameterValue("GLIDE_ON") > 0.5f;
    float glideSpeedMs = *apvts.getRawParameterValue("GLIDE_SPEED");
    int glideCurveType = static_cast<int>(*apvts.getRawParameterValue("GLIDE_CURVE_TYPE"));
    float glideRange = *apvts.getRawParameterValue("GLIDE_RANGE");
    const auto glideTimeRange = static_cast<juce::int64>(
        std::llround(*apvts.getRawParameterValue("GLIDE_TIME_RANGE")));

    bool isLegatoOn = *apvts.getRawParameterValue("LEGATO_ON") > 0.5f;
    bool isLegatoTrue = *apvts.getRawParameterValue("TRUE_LEGATO_ON") > 0.5f;
    float legatoRange = *apvts.getRawParameterValue("LEGATO_RANGE");
    float legatoSpeedMs = *apvts.getRawParameterValue("LEGATO_SPEED");
    int legatoCurveType = static_cast<int>(*apvts.getRawParameterValue("LEGATO_CURVE_TYPE"));

    // How long will we hold off on glide or legato while waiting for finished chord change
    double trueGlideDebounce = *apvts.getRawParameterValue("TRUE_GLIDE_DEBOUNCE");

    double chordChangeDebounce = *apvts.getRawParameterValue("CHORD_CHANGE_DEBOUNCE");

    int glideSearchMode = static_cast<int>(*apvts.getRawParameterValue("GLIDE_SEARCH_MODE"));
    int legatoSearchMode = static_cast<int>(*apvts.getRawParameterValue("LEGATO_SEARCH_MODE"));

    // For calculating the smoothing of the portamento
    double sampleRate = getSampleRate();
    if (sampleRate <= 0) sampleRate = 44100.0;  // Just in case

    double portTimeSeconds = portSpeedMs / 1000.0;
    double glideTimeSeconds = glideSpeedMs / 1000.0;
    double legatoTimeSeconds = legatoSpeedMs / 1000.0;
    double phaseIncrement = 1.0;

    if (portTimeSeconds > 0.0 || glideTimeSeconds > 0.0 || legatoTimeSeconds > 0.0) {
        // Amount to add per block to reach 1.0 in portTimeSeconds or glideTimeSeconds
        phaseIncrement = buffer.getNumSamples() / (sampleRate * (isLegatoOn ? legatoTimeSeconds : isGlideOn ? glideTimeSeconds : portTimeSeconds));
    }

    // Updating all the possible glide notes to the time that's passed, since we don't wanna cut it short we update before we set new ones so that the samples can play out.
    double currentTime = juce::Time::currentTimeMillis();

    juce::MidiBuffer processedMidi;

    handleDebounce(processedMidi);

    for (const auto metadata : midiMessages) {
        auto message = metadata.getMessage();
        cacheEvent(message, processedMidi, metadata.samplePosition);
    }

    // This is where we actually apply the portamento since we always wanna apply it, not just at the start of the press
    for (int i = 0; i < 128; i++) {
        if (glideTimers[i] < juce::Time::currentTimeMillis()) glideTimers[i] = 0;

        if (activeNotes[i] && hijackedBy[i] == i) {
            // Recalculate the target in case the root note changed while they were holding the key down
            // For true legato we also need our relative differtence
            double newTarget = calculateJIPitchBend(i, currentRootNote) + (i - hijackingNote[i]);
            // Tracking where we are in the pitch bend to keep pitch bends correct between blocks for each note
            if (newTarget != targetPitchBends[i]) {
                targetPitchBends[i] = newTarget;
                startPitchBends[i] = currentPitchBends[i];
                slidePhase[i] = 0.0;
            }

            // Slide the current pitch by the coefficient to the target
            if ((isPortamentoOn && portTimeSeconds > 0.0) || (isGlideOn && glideTimeSeconds > 0.0) || (isLegatoOn && legatoTimeSeconds > 0.0)) {
                
                slidePhase[i] += phaseIncrement;
                if (slidePhase[i] > 1.0) slidePhase[i] = 1.0;

                double phase = slidePhase[i];
                double t = phase;
                if (isLegatoOn) {
                    if (legatoCurveType == 1) { // If we're using logarithmic curve for legato
                        t = std::log10(1.0 + 9.0 * phase);
                    }
                    else if (legatoCurveType == 2) { // If we are using exponential curve for legato
                        // Just a standard exponential slope since we don't have a custom curve creator or anything yet
                        t = (std::exp(phase * 2.30258509) - 1.0) / 9.0;
                    }
                }
                else if (isGlideOn) {
                    if (glideCurveType == 1) { // If we're using logarithmic curve for glide
                        t = std::log10(1.0 + 9.0 * phase);
                    }
                    else if (glideCurveType == 2) { // If we are using exponential curve for glide
                        // Just a standard exponential slope since we don't have a custom curve creator or anything yet
                        t = (std::exp(phase * 2.30258509) - 1.0) / 9.0;
                    }
                }
                else if (isPortamentoOn) {
                    if (portCurveType == 1) { // If we're using logarithmic curve for portamento
                        t = std::log10(1.0 + 9.0 * phase);
                    }
                    else if (portCurveType == 2) { // If we are using exponential curve for portamento
                        // Just a standard exponential slope since we don't have a custom curve creator or anything yet
                        t = (std::exp(phase * 2.30258509) - 1.0) / 9.0;
                    }
                }
                

                currentPitchBends[i] = startPitchBends[i] + (targetPitchBends[i] - startPitchBends[i]) * t;
            }
            else {
                // Otherwise we just snap
                currentPitchBends[i] = targetPitchBends[i];
                slidePhase[i] = 1.0;    // Since we don't wanna be snapping then gliding
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
