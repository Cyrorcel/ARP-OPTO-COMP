#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "CompCore.h"

class AmberCompProcessor : public juce::AudioProcessor
{
public:
    AmberCompProcessor();
    ~AmberCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ARP-Opto-comp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // lus par l'interface (thread GUI)
    std::atomic<float> grMeter  { 0.0f };  // réduction de gain en dB (>= 0)
    std::atomic<float> outMeter { 0.0f };  // crête de sortie (linéaire)

private:
    arp::CompCore core;   // DSP du compresseur (dont les deux filtres fixes), voir CompCore.h

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmberCompProcessor)
};
