#include "PluginEditor.h"

namespace {
const juce::Colour kBackground { 0xff0e0e12 };
const juce::Colour kAccent     { 0xff4ac4d4 };
const juce::Colour kText       { 0xffe6e6ec };
const juce::Colour kTextDim    { 0xff6a6a76 };

void styleKnob(juce::Slider& s) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    s.setColour(juce::Slider::rotarySliderFillColourId,    kAccent);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, kTextDim);
    s.setColour(juce::Slider::textBoxTextColourId,         kText);
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

void styleLabel(juce::Label& l, const juce::String& text) {
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, kTextDim);
}
}  // namespace

ShifterAudioProcessorEditor::ShifterAudioProcessorEditor(ShifterAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      mixAttachment_       (p.parameters, "mix",        mixSlider_),
      transientsAttachment_(p.parameters, "transients", transientsSlider_)
{
    for (int s = 0; s >= -12; --s) {
        shiftCombo_.addItem(juce::String(s), s + 13);  // IDs 1..13 (-12 → 1, 0 → 13)
    }
    shiftCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a22));
    shiftCombo_.setColour(juce::ComboBox::textColourId,       kText);
    shiftCombo_.setColour(juce::ComboBox::outlineColourId,    kTextDim);
    shiftCombo_.setColour(juce::ComboBox::arrowColourId,      kAccent);
    addAndMakeVisible(shiftCombo_);
    shiftAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.parameters, "shift", shiftCombo_);

    styleLabel(shiftLabel_, "SHIFT (st)");
    addAndMakeVisible(shiftLabel_);

    styleKnob(mixSlider_);
    addAndMakeVisible(mixSlider_);
    styleLabel(mixLabel_, "MIX");
    addAndMakeVisible(mixLabel_);

    styleKnob(transientsSlider_);
    addAndMakeVisible(transientsSlider_);
    styleLabel(transientsLabel_, "TRANSIENTS");
    addAndMakeVisible(transientsLabel_);

    setSize(420, 300);
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
    g.drawText(juce::String::fromUTF8("M2 · transient preserve"),
               bounds.removeFromBottom(20.0f),
               juce::Justification::centred, false);
}

void ShifterAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(44);      // title
    bounds.removeFromBottom(20);   // footer

    // Shift selector row.
    auto shiftRow = bounds.removeFromTop(56);
    shiftLabel_.setBounds(shiftRow.removeFromTop(20));
    shiftCombo_.setBounds(shiftRow.reduced(40, 0));

    bounds.removeFromTop(10);

    // Two knobs side by side.
    auto knobRow = bounds;
    const int half = knobRow.getWidth() / 2;
    auto mixArea  = knobRow.removeFromLeft(half);
    auto transArea = knobRow;

    mixLabel_.setBounds(mixArea.removeFromTop(20));
    mixSlider_.setBounds(mixArea);

    transientsLabel_.setBounds(transArea.removeFromTop(20));
    transientsSlider_.setBounds(transArea);
}
