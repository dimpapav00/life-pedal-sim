#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout LifePedalAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    knob ("magnitude",  "Magnitude",  0.40f);
    knob ("octave",     "Octave",     0.30f);
    knob ("distortion", "Distortion", 0.60f);
    knob ("filter",     "Filter",     0.35f);
    knob ("amplitude",  "Amplitude",  0.50f);

    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output",
                                                              juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "boost", 1 }, "Boost On", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "dist", 1 }, "Distortion On", true));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "clip", 1 }, "Clipping",
                                                               juce::StringArray { "Silicon", "Germanium", "LED" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "boostpos", 1 }, "Boost Position",
                                                               juce::StringArray { "Post", "Pre" }, 0));
    return { p.begin(), p.end() };
}

LifePedalAudioProcessor::LifePedalAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMag = apvts.getRawParameterValue ("magnitude");
    pOct = apvts.getRawParameterValue ("octave");
    pDist = apvts.getRawParameterValue ("distortion");
    pFilter = apvts.getRawParameterValue ("filter");
    pAmp = apvts.getRawParameterValue ("amplitude");
    pOut = apvts.getRawParameterValue ("output");
    pBoost = apvts.getRawParameterValue ("boost");
    pDistOn = apvts.getRawParameterValue ("dist");
    pClip = apvts.getRawParameterValue ("clip");
    pBoostPos = apvts.getRawParameterValue ("boostpos");
}

void LifePedalAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool LifePedalAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void LifePedalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    lifepedal::Params p;
    p.magnitude  = pMag->load();
    p.octave     = pOct->load();
    p.distortion = pDist->load();
    p.filter     = pFilter->load();
    p.amplitude  = pAmp->load();
    p.outputDb   = pOut->load();
    p.boostOn    = pBoost->load() > 0.5f;
    p.distOn     = pDistOn->load() > 0.5f;
    p.clip       = (int) std::lround (pClip->load());
    p.boostPost  = (int) std::lround (pBoostPos->load()) == 0;

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* LifePedalAudioProcessor::createEditor() { return new LifePedalEditor (*this); }

void LifePedalAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void LifePedalAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LifePedalAudioProcessor(); }
