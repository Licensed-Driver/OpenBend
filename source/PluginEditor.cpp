/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OpenBendAudioProcessorEditor::OpenBendAudioProcessorEditor (OpenBendAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{

    // Setup Toggle
    addAndMakeVisible(portamentoToggle);
    portamentoAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "PORTAMENTO_ON", portamentoToggle);

    // Setup Knob
    glideSpeedSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    glideSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(glideSpeedSlider);
    glideSpeedAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "GLIDE_SPEED", glideSpeedSlider);

    // Setup Curve Type Selection Box
    curveTypeComboBox.addItemList({ "Linear", "Logarithmic", "Exponential" }, 1);
    addAndMakeVisible(curveTypeComboBox);
    curveAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "CURVE_TYPE", curveTypeComboBox);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
}

OpenBendAudioProcessorEditor::~OpenBendAudioProcessorEditor()
{
}

//==============================================================================
void OpenBendAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    
    // Draw a label above the knob
    juce::Rectangle<int> labelArea(glideSpeedSlider.getX(), glideSpeedSlider.getY() - 25, glideSpeedSlider.getWidth(), 20);
    g.drawFittedText("Glide Speed", labelArea, juce::Justification::centred, 1);
}

void OpenBendAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(30);

    // Put the toggle at the top
    portamentoToggle.setBounds(area.removeFromTop(40));

    // Put the knob in the middle
    auto middleArea = area.removeFromTop(150);
    glideSpeedSlider.setBounds(middleArea.withSizeKeepingCentre(120, 120));

    // Put the dropdown at the bottom
    curveTypeComboBox.setBounds(area.removeFromTop(30).withSizeKeepingCentre(200, 30));
}
