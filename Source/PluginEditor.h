#pragma once
#include "PluginProcessor.h"

class LifeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LifeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class LifePedalEditor : public juce::AudioProcessorEditor
{
public:
    explicit LifePedalEditor (LifePedalAudioProcessor&);
    ~LifePedalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);

    LifePedalAudioProcessor& proc;
    LifeLookAndFeel lnf;

    Knob magnitude, octave, distortion, filter, amplitude;
    juce::ToggleButton boostSwitch { "BOOST" }, distSwitch { "DISTORTION" };
    juce::ComboBox clipBox, boostPosBox;
    juce::Label clipLabel, boostPosLabel, outLabel;
    juce::Slider outSlider;

    std::unique_ptr<ButtonAtt> boostAtt, distAtt;
    std::unique_ptr<ComboAtt> clipAtt, boostPosAtt;
    std::unique_ptr<SliderAtt> outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LifePedalEditor)
};
