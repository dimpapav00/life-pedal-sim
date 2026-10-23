#include "PluginEditor.h"

namespace
{
const juce::Colour kGold   { 0xffd4af37 };
const juce::Colour kBlack  { 0xff111111 };
const juce::Colour kPanel  { 0xff1b1b1b };
}

LifeLookAndFeel::LifeLookAndFeel()
{
    setColour (juce::Label::textColourId, kGold);
    setColour (juce::ComboBox::backgroundColourId, kBlack);
    setColour (juce::ComboBox::textColourId, kGold);
    setColour (juce::ComboBox::outlineColourId, kGold.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, kGold);
    setColour (juce::PopupMenu::backgroundColourId, kBlack);
    setColour (juce::PopupMenu::textColourId, kGold);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kGold.withAlpha (0.3f));
    setColour (juce::Slider::trackColourId, kGold);
    setColour (juce::Slider::thumbColourId, kGold);
    setColour (juce::Slider::backgroundColourId, kBlack);
    setColour (juce::TextEditor::textColourId, kGold);
    setColour (juce::TextEditor::backgroundColourId, kBlack);
}

void LifeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // value arc
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (kGold.withAlpha (0.18f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kGold);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float kr = radius - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.x, centre.y - kr,
                                             juce::Colour (0xff0b0b0b), centre.x, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (kGold.withAlpha (0.7f));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.8f, -kr + 3.0f, 3.6f, kr * 0.55f, 1.5f);
    g.setColour (kGold);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void LifeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const float d = std::min (area.getWidth() * 0.55f, area.getHeight() - 28.0f);
    const auto sw = juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getY() + 20.0f + d * 0.5f });

    // LED
    const bool on = b.getToggleState();
    const juce::Rectangle<float> led (area.getCentreX() - 5.0f, area.getY() + 2.0f, 10.0f, 10.0f);
    if (on)
    {
        g.setColour (kGold.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? kGold : juce::Colour (0xff3b3216));
    g.fillEllipse (led);

    // footswitch
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9c9c9), sw.getX(), sw.getY(),
                                             juce::Colour (0xff6a6a6a), sw.getRight(), sw.getBottom(), false));
    g.fillEllipse (sw);
    g.setColour (highlighted ? kGold : juce::Colour (0xff222222));
    g.drawEllipse (sw, 2.0f);

    g.setColour (kGold);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

LifePedalEditor::LifePedalEditor (LifePedalAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (620, 430);

    addKnob (magnitude,  "magnitude",  "MAGNITUDE");
    addKnob (octave,     "octave",     "OCTAVE");
    addKnob (distortion, "distortion", "DISTORTION");
    addKnob (filter,     "filter",     "FILTER");
    addKnob (amplitude,  "amplitude",  "AMPLITUDE");

    addAndMakeVisible (boostSwitch);
    addAndMakeVisible (distSwitch);
    boostAtt = std::make_unique<ButtonAtt> (proc.apvts, "boost", boostSwitch);
    distAtt  = std::make_unique<ButtonAtt> (proc.apvts, "dist", distSwitch);

    clipBox.addItemList ({ "Silicon", "Germanium", "LED" }, 1);
    boostPosBox.addItemList ({ "Post-distortion", "Pre-distortion" }, 1);
    addAndMakeVisible (clipBox);
    addAndMakeVisible (boostPosBox);
    clipAtt     = std::make_unique<ComboAtt> (proc.apvts, "clip", clipBox);
    boostPosAtt = std::make_unique<ComboAtt> (proc.apvts, "boostpos", boostPosBox);

    for (auto* l : { &clipLabel, &boostPosLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    clipLabel.setText ("CLIPPING", juce::dontSendNotification);
    boostPosLabel.setText ("BOOST POSITION", juce::dontSendNotification);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);

    outSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    outSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (outSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outSlider);
}

LifePedalEditor::~LifePedalEditor() { setLookAndFeel (nullptr); }

void LifePedalEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void LifePedalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0a));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kPanel.brighter (0.05f), 0, 0, kBlack, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 14.0f);
    g.setColour (kGold);
    g.drawRoundedRectangle (body, 14.0f, 2.5f);

    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("LIFE PEDAL SIM", getLocalBounds().removeFromTop (62).withTrimmedTop (14), juce::Justification::centred);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kGold.withAlpha (0.6f));
    g.drawText ("distortion  /  analog octave  /  booster  -  unofficial emulation, not affiliated",
                getLocalBounds().removeFromTop (78).withTrimmedTop (60), juce::Justification::centred);
}

void LifePedalEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (74);

    // five knobs
    auto row = r.removeFromTop (150);
    const int w = row.getWidth() / 5;
    for (auto* k : { &magnitude, &octave, &distortion, &filter, &amplitude })
    {
        auto cell = row.removeFromLeft (w);
        k->label.setBounds (cell.removeFromBottom (20));
        k->slider.setBounds (cell);
    }

    r.removeFromTop (6);
    // footswitches under the two circuits
    auto sw = r.removeFromTop (100);
    const int half = sw.getWidth() / 2;
    boostSwitch.setBounds (sw.removeFromLeft (half).withSizeKeepingCentre (110, 100));
    distSwitch.setBounds (sw.withSizeKeepingCentre (110, 100));

    r.removeFromTop (6);
    auto bottom = r.removeFromTop (30);
    clipLabel.setBounds (bottom.removeFromLeft (70));
    clipBox.setBounds (bottom.removeFromLeft (110));
    bottom.removeFromLeft (20);
    boostPosLabel.setBounds (bottom.removeFromLeft (110));
    boostPosBox.setBounds (bottom.removeFromLeft (130));
    bottom.removeFromLeft (20);
    outLabel.setBounds (bottom.removeFromLeft (52));
    outSlider.setBounds (bottom);
}
