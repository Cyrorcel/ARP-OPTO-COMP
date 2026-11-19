#pragma once
#include "PluginProcessor.h"

// Look & feel : bouton noir laqué (image pré-rendue) + repère dessiné en code, qui tourne avec la valeur
class AmberLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmberLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    juce::Image knobSprite;   // 112 x 120 px (rendu @2x), centré sur le bouton

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmberLookAndFeel)
};

class AmberCompEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit AmberCompEditor (AmberCompProcessor&);
    ~AmberCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeterNeedle (juce::Graphics&, juce::Rectangle<float>) const;
    void drawBay (juce::Graphics&) const;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    AmberCompProcessor& processor;
    AmberLookAndFeel lnf;
    std::array<Knob, 8> knobs;

    juce::Image background;               // façade complète (1680 x 840)
    juce::Image meterGlass;               // reflets du verre du VU-mètre (transparent)
    std::array<juce::Image, 5> bayFrames; // alcôve avec lampes, de éteintes (0) à pleine lueur (4)

    float shownGR = 0.0f;
    float shownLevel = 0.0f;

    static constexpr int W = 840;
    static constexpr int H = 420;

    const juce::Rectangle<float> meterRect { 32.f, 82.f, 284.f, 180.f };
    const juce::Rectangle<float> bayRect   { 30.f, 272.f, 288.f, 138.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmberCompEditor)
};
