#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AmberCompProcessor::AmberCompProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout AmberCompProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using R = NormalisableRange<float>;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back (std::make_unique<P> (ParameterID { "input",   1 }, "Input",     R (-12.f, 24.f, 0.1f),        0.f));
    params.push_back (std::make_unique<P> (ParameterID { "thresh",  1 }, "Threshold", R (-40.f, 0.f, 0.1f),        -12.f));
    params.push_back (std::make_unique<P> (ParameterID { "ratio",   1 }, "Ratio",     R (1.f, 20.f, 0.1f, 0.5f),    3.f));
    params.push_back (std::make_unique<P> (ParameterID { "drive",   1 }, "Tube Drive",R (0.f, 1.f, 0.01f),          0.25f));
    params.push_back (std::make_unique<P> (ParameterID { "attack",  1 }, "Attack",    R (0.1f, 100.f, 0.01f, 0.4f), 10.f));
    params.push_back (std::make_unique<P> (ParameterID { "release", 1 }, "Release",   R (20.f, 2000.f, 1.f, 0.4f),  200.f));
    params.push_back (std::make_unique<P> (ParameterID { "makeup",  1 }, "Makeup",    R (0.f, 24.f, 0.1f),          3.f));
    params.push_back (std::make_unique<P> (ParameterID { "mix",     1 }, "Mix",       R (0.f, 100.f, 1.f),          100.f));
    return { params.begin(), params.end() };
}

void AmberCompProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
}

bool AmberCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void AmberCompProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int n   = buffer.getNumSamples();
    const int nCh = jmin (buffer.getNumChannels(), 8);

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    arp::CompParams cp;
    cp.inputDb     = p ("input");
    cp.thresholdDb = p ("thresh");
    cp.ratio       = p ("ratio");
    cp.drive       = p ("drive");
    cp.attackMs    = p ("attack");
    cp.releaseMs   = p ("release");
    cp.makeupDb    = p ("makeup");
    cp.mix         = p ("mix") * 0.01f;

    // tout le traitement (dont passe-bas 10 kHz, sidechain 50 Hz et EQ fixe de sortie) est dans CompCore.h
    core.process (buffer.getArrayOfWritePointers(), nCh, n, cp);

    grMeter.store (core.getGainReductionDb());
    outMeter.store (core.getBlockPeak());
}

AudioProcessorEditor* AmberCompProcessor::createEditor()
{
    return new AmberCompEditor (*this);
}

void AmberCompProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AmberCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmberCompProcessor();
}
