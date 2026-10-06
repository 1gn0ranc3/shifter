#include "PluginEditor.h"

namespace {
const juce::Colour kBackground { 0xff0e0e12 };
const juce::Colour kAccent     { 0xff4ac4d4 };
const juce::Colour kText       { 0xffe6e6ec };
const juce::Colour kTextDim    { 0xff6a6a76 };

void styleLabel(juce::Label& l, const juce::String& text) {
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, kTextDim);
}
}  // namespace

ShifterAudioProcessorEditor::ShifterAudioProcessorEditor(ShifterAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p)
{
    // Items populated in ascending-value order so the ComboBox item index maps
    // directly through the AudioParameterInt range: index 0 → -12, index 12 → 0.
    for (int s = -12; s <= 0; ++s) {
        shiftCombo_.addItem(juce::String(s), s + 13);  // IDs 1..13 (-12 → 1, 0 → 13)
    }
    shiftCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a22));
    shiftCombo_.setColour(juce::ComboBox::textColourId,       kText);
    shiftCombo_.setColour(juce::ComboBox::outlineColourId,    kTextDim);
    shiftCombo_.setColour(juce::ComboBox::arrowColourId,      kAccent);
    addAndMakeVisible(shiftCombo_);
    shiftAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.parameters, "shift", shiftCombo_);

    styleLabel(shiftLabel_, "SHIFT (semitones)");
    addAndMakeVisible(shiftLabel_);

    setSize(320, 180);
}

ShifterAudioProcessorEditor::~ShifterAudioProcessorEditor() = default;

void ShifterAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(kBackground);

    auto bounds = getLocalBounds().toFloat();

    g.setColour(kAccent);
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("SHIFTER",
               bounds.removeFromTop(44.0f),
               juce::Justification::centred, false);

    g.setColour(kTextDim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(juce::String::fromUTF8("M2.5 · phase-locked, 10.7 ms"),
               bounds.removeFromBottom(20.0f),
               juce::Justification::centred, false);
}

void ShifterAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(44);      // title
    bounds.removeFromBottom(20);   // footer

    shiftLabel_.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);
    shiftCombo_.setBounds(bounds.removeFromTop(36).reduced(30, 0));
}
