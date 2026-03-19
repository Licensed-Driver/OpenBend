/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class OpenBendAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    OpenBendAudioProcessorEditor (OpenBendAudioProcessor&);
    ~OpenBendAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    OpenBendAudioProcessor& audioProcessor;

    // Setting up the layout graphically

    juce::ToggleButton portamentoToggle{ "Portamento Active" };
    juce::Slider portSpeedSlider;
    juce::ComboBox portCurveTypeComboBox;

    // Note sliding
    juce::ToggleButton glideToggle{ "Glide Active" };
    juce::ToggleButton trueGlideToggle{ "True Glide" };
    juce::Slider glideSpeedSlider;
    juce::ComboBox glideCurveTypeComboBox;
    juce::Slider glideRangeSlider;
    juce::Slider glideTimeRangeSlider;
    juce::ToggleButton legatoToggle{ "Legato Active" };
    juce::ToggleButton trueLegatoToggle{ "True Legato" };
    juce::ComboBox legatoSearchComboBox;
    juce::ComboBox glideSearchComboBox;
    juce::Slider legatoSpeedSlider;
    juce::Slider legatoRangeSlider;
    juce::ComboBox legatoCurveTypeComboBox;
    juce::Slider trueGlideDebounceSlider;
    juce::Slider chordChangeDebounceSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Portamento
    std::unique_ptr<ButtonAttachment> portamentoAttachment;
    std::unique_ptr<SliderAttachment> portSpeedAttachment;
    std::unique_ptr<ComboBoxAttachment> portCurveAttachment;

    // Glide
    std::unique_ptr<ButtonAttachment> glideAttachment;
    std::unique_ptr<ButtonAttachment> trueGlideAttachment;
    std::unique_ptr<SliderAttachment> glideSpeedAttachment;
    std::unique_ptr<ComboBoxAttachment> glideCurveAttachment;
    std::unique_ptr<SliderAttachment> glideRangeAttachment; // For how far from a note we are willing to slide
    std::unique_ptr<SliderAttachment> glideTimeRangeAttachment; // For how long after a note we are willing to wait for another note to be pressed and slide to

    std::unique_ptr<ButtonAttachment> legatoAttachment;
    std::unique_ptr<ButtonAttachment> trueLegatoAttachment;
    std::unique_ptr<ComboBoxAttachment> legatoSearchAttachment;
    std::unique_ptr<ComboBoxAttachment> glideSearchAttachment;
    std::unique_ptr<SliderAttachment> legatoSpeedAttachment;
    std::unique_ptr<SliderAttachment> legatoRangeAttachment;
    std::unique_ptr<ComboBoxAttachment> legatoCurveAttachment;
    std::unique_ptr<SliderAttachment> trueGlideDebounceAttachment;
    std::unique_ptr<SliderAttachment> chordChangeDebounceAttachment;

    void updateVisibility();    // For updating the ui shtuff

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenBendAudioProcessorEditor)
};
