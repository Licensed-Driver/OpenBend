/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

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

    juce::ToggleButton portamentoToggle{ "Portamento Active " };
    juce::Slider glideSpeedSlider;
    juce::ComboBox curveTypeComboBox;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> portamentoAttachment;
    std::unique_ptr<SliderAttachment> glideSpeedAttachment;
    std::unique_ptr<ComboBoxAttachment> curveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenBendAudioProcessorEditor)
};
