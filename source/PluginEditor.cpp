/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OpenBendAudioProcessorEditor::OpenBendAudioProcessorEditor(OpenBendAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Portamento ouioui
    addAndMakeVisible(portamentoToggle);
    portamentoAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "PORTAMENTO_ON", portamentoToggle);

    portSpeedSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    portSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(portSpeedSlider);
    portSpeedAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "PORT_SPEED", portSpeedSlider);

    portCurveTypeComboBox.addItemList({ "Linear", "Logarithmic", "Exponential" }, 1);
    addAndMakeVisible(portCurveTypeComboBox);
    portCurveAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "PORT_CURVE_TYPE", portCurveTypeComboBox);

    // Glide
    addAndMakeVisible(glideToggle);
    glideAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "GLIDE_ON", glideToggle);

    addAndMakeVisible(trueGlideToggle);
    trueGlideAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "TRUE_GLIDE_ON", trueGlideToggle);

    glideSearchComboBox.addItemList({ "Closest", "Highest Priority", "Lowest Priority", "Next High", "Next Low", "Chord Change (True Glide)" }, 1);
    addAndMakeVisible(glideSearchComboBox);
    glideSearchAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "GLIDE_SEARCH_MODE", glideSearchComboBox);

    glideSpeedSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    glideSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(glideSpeedSlider);
    glideSpeedAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "GLIDE_SPEED", glideSpeedSlider);

    glideRangeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    glideRangeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(glideRangeSlider);
    glideRangeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "GLIDE_RANGE", glideRangeSlider);

    glideTimeRangeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    glideTimeRangeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(glideTimeRangeSlider);
    glideTimeRangeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "GLIDE_TIME_RANGE", glideTimeRangeSlider);

    glideCurveTypeComboBox.addItemList({ "Linear", "Logarithmic", "Exponential" }, 1);
    addAndMakeVisible(glideCurveTypeComboBox);
    glideCurveAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "GLIDE_CURVE_TYPE", glideCurveTypeComboBox);

    trueGlideDebounceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    trueGlideDebounceSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(trueGlideDebounceSlider);
    trueGlideDebounceAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "TRUE_GLIDE_DEBOUNCE", trueGlideDebounceSlider);

    // Legato mamamia
    addAndMakeVisible(legatoToggle);
    legatoAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "LEGATO_ON", legatoToggle);

    addAndMakeVisible(trueLegatoToggle);
    trueLegatoAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "TRUE_LEGATO_ON", trueLegatoToggle);

    legatoSearchComboBox.addItemList({ "Closest", "Highest Priority", "Lowest Priority", "Next High", "Next Low", "Chord Change (True Glide)" }, 1);
    addAndMakeVisible(legatoSearchComboBox);
    legatoSearchAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "LEGATO_SEARCH_MODE", legatoSearchComboBox);

    legatoSpeedSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    legatoSpeedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(legatoSpeedSlider);
    legatoSpeedAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "LEGATO_SPEED", legatoSpeedSlider);

    legatoRangeSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    legatoRangeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 75, 25);
    addAndMakeVisible(legatoRangeSlider);
    legatoRangeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "LEGATO_RANGE", legatoRangeSlider);

    legatoCurveTypeComboBox.addItemList({ "Linear", "Logarithmic", "Exponential" }, 1);
    addAndMakeVisible(legatoCurveTypeComboBox);
    legatoCurveAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "LEGATO_CURVE_TYPE", legatoCurveTypeComboBox);

    chordChangeDebounceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    chordChangeDebounceSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addChildComponent(chordChangeDebounceSlider); // Use addChildComponent so it can be hidden initially
    chordChangeDebounceAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "CHORD_CHANGE_DEBOUNCE", chordChangeDebounceSlider);

    // Listeners for our ui changing
    glideToggle.onClick = [this] { updateVisibility(); };
    legatoToggle.onClick = [this] { updateVisibility(); };
    glideSearchComboBox.onChange = [this] { updateVisibility(); };
    legatoSearchComboBox.onChange = [this] { updateVisibility(); };

    setSize(960, 560);

    // Set initial state
    updateVisibility();
}

OpenBendAudioProcessorEditor::~OpenBendAudioProcessorEditor()
{
}

void OpenBendAudioProcessorEditor::updateVisibility()
{
    // True toggles shouldn't be available if the regular counter part isn't selected tey
    trueGlideToggle.setEnabled(glideToggle.getToggleState());
    trueLegatoToggle.setEnabled(legatoToggle.getToggleState());

    // Check if chord change is selected for search mode to show the slider
    bool showInGlide = (glideSearchComboBox.getSelectedItemIndex() == 5);
    bool showInLegato = (legatoSearchComboBox.getSelectedItemIndex() == 5);

    chordChangeDebounceSlider.setVisible(showInGlide || showInLegato);

    resized();
    repaint();
}

//==============================================================================
void OpenBendAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff222222));

    auto area = getLocalBounds().reduced(15);
    int panelWidth = area.getWidth() / 3;

    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(area.removeFromLeft(panelWidth).reduced(5).toFloat(), 8.0f);
    g.fillRoundedRectangle(area.removeFromLeft(panelWidth).reduced(5).toFloat(), 8.0f);
    g.fillRoundedRectangle(area.reduced(5).toFloat(), 8.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14.0f));

    auto drawLabel = [&](juce::Component& comp, const juce::String& text, int yOffset = 25) {
        juce::Rectangle<int> labelArea(comp.getX(), comp.getY() - yOffset, comp.getWidth(), 20);
        g.drawFittedText(text, labelArea, juce::Justification::centred, 1);
    };

    drawLabel(portSpeedSlider, "Portamento Speed");
    drawLabel(glideSpeedSlider, "Glide Speed");
    drawLabel(legatoSpeedSlider, "Legato Speed");

    drawLabel(glideRangeSlider, "Range");
    drawLabel(glideTimeRangeSlider, "Time Range");
    drawLabel(legatoRangeSlider, "Legato Range");

    drawLabel(trueGlideDebounceSlider, "True Glide Window", 15);

    // Only label it if it's visible obvi
    if (chordChangeDebounceSlider.isVisible()) {
        drawLabel(chordChangeDebounceSlider, "Chord Change Window", 15);
    }
}

void OpenBendAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    auto portArea = area.removeFromLeft(area.getWidth() / 3).reduced(15);
    auto glideArea = area.removeFromLeft(area.getWidth() / 2).reduced(15);
    auto legatoArea = area.reduced(15);

    // Portamento six sept
    portamentoToggle.setBounds(portArea.removeFromTop(25));

    portArea.removeFromTop(40);
    portSpeedSlider.setBounds(portArea.removeFromTop(110).withSizeKeepingCentre(100, 100));

    portArea.removeFromTop(20);
    portCurveTypeComboBox.setBounds(portArea.removeFromTop(25).reduced(10, 0));

    // Glide
    auto glideToggles = glideArea.removeFromTop(25);
    glideToggle.setBounds(glideToggles.removeFromLeft(glideToggles.getWidth() / 2));
    trueGlideToggle.setBounds(glideToggles);

    glideArea.removeFromTop(10);
    glideSearchComboBox.setBounds(glideArea.removeFromTop(25).reduced(10, 0));

    glideArea.removeFromTop(30);
    glideSpeedSlider.setBounds(glideArea.removeFromTop(110).withSizeKeepingCentre(100, 100));

    glideArea.removeFromTop(25);
    auto glideRangesArea = glideArea.removeFromTop(90);
    glideRangeSlider.setBounds(glideRangesArea.removeFromLeft(glideRangesArea.getWidth() / 2));
    glideTimeRangeSlider.setBounds(glideRangesArea);

    glideArea.removeFromTop(20);
    glideCurveTypeComboBox.setBounds(glideArea.removeFromTop(25).reduced(10, 0));

    glideArea.removeFromTop(20);
    trueGlideDebounceSlider.setBounds(glideArea.removeFromTop(25));

    // The dynamic rendering thigns
    if (glideSearchComboBox.getSelectedItemIndex() == 5) {
        glideArea.removeFromTop(20);
        chordChangeDebounceSlider.setBounds(glideArea.removeFromTop(25));
    }

    // Legato
    auto legatoToggles = legatoArea.removeFromTop(25);
    legatoToggle.setBounds(legatoToggles.removeFromLeft(legatoToggles.getWidth() / 2));
    trueLegatoToggle.setBounds(legatoToggles);

    legatoArea.removeFromTop(10);
    legatoSearchComboBox.setBounds(legatoArea.removeFromTop(25).reduced(10, 0));

    legatoArea.removeFromTop(30);
    legatoSpeedSlider.setBounds(legatoArea.removeFromTop(110).withSizeKeepingCentre(100, 100));

    legatoArea.removeFromTop(25);
    legatoRangeSlider.setBounds(legatoArea.removeFromTop(90).withSizeKeepingCentre(80, 90));

    legatoArea.removeFromTop(20);
    legatoCurveTypeComboBox.setBounds(legatoArea.removeFromTop(25).reduced(10, 0));

    // Dynamic stuff again
    if (legatoSearchComboBox.getSelectedItemIndex() == 5 && glideSearchComboBox.getSelectedItemIndex() != 5) {
        legatoArea.removeFromTop(20);
        chordChangeDebounceSlider.setBounds(legatoArea.removeFromTop(25));
    }
}