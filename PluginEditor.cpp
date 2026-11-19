#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    Font makeFont (const String& face, float h, int style = Font::plain)
    {
        return Font (FontOptions (face, h, style));
    }

    Image loadImage (const void* data, size_t size)
    {
        return ImageFileFormat::loadFrom (data, size);
    }

    const Colour kCream { 0xfff0e4c8 };
}

// ───────────────────────── Look & feel ─────────────────────────

AmberLookAndFeel::AmberLookAndFeel()
{
    knobSprite = loadImage (BinaryData::knob_png, (size_t) BinaryData::knob_pngSize);
}

Label* AmberLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (makeFont ("Helvetica Neue", 12.0f));
    l->setJustificationType (Justification::centred);
    return l;
}

void AmberLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                         float pos, float a0, float a1, Slider&)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float kr = 55.0f * 0.76f;

    // corps du bouton (laque noire, reflets et ombre pré-rendus)
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (knobSprite, Rectangle<float> (c.x - 56.0f, c.y - 56.0f, 112.0f, 120.0f));

    // repère clair, qui tourne avec la valeur
    const float a  = a0 + pos * (a1 - a0) - MathConstants<float>::pi * 0.5f;
    const float dx = std::cos (a), dy = std::sin (a);
    const float r0 = kr * 0.18f, r1 = kr * 0.80f;

    Path p;
    auto stroke = [&] (float ox, float oy, float thick, Colour col)
    {
        p.clear();
        p.startNewSubPath (c.x + dx * r0 + ox, c.y + dy * r0 + oy);
        p.lineTo          (c.x + dx * r1 + ox, c.y + dy * r1 + oy);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    };
    stroke (0.6f, 1.0f, 3.6f, Colours::black.withAlpha (0.7f));
    stroke (0.0f, 0.0f, 3.0f, Colour (0xfff2efe6));
    stroke (-0.36f, -0.48f, 0.9f, Colours::white.withAlpha (0.95f));
}

// ───────────────────────── Éditeur ─────────────────────────

AmberCompEditor::AmberCompEditor (AmberCompProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    struct Def { const char* id; const char* suffix; int decimals; };
    static const Def defs[8] = {
        { "input",   " dB", 1 },
        { "thresh",  " dB", 1 },
        { "ratio",   " :1", 1 },
        { "drive",   "",    2 },
        { "attack",  " ms", 1 },
        { "release", " ms", 0 },
        { "makeup",  " dB", 1 },
        { "mix",     " %",  0 },
    };

    background = loadImage (BinaryData::bg_png,    (size_t) BinaryData::bg_pngSize);
    meterGlass = loadImage (BinaryData::glass_png, (size_t) BinaryData::glass_pngSize);
    bayFrames[0] = loadImage (BinaryData::bay0_png, (size_t) BinaryData::bay0_pngSize);
    bayFrames[1] = loadImage (BinaryData::bay1_png, (size_t) BinaryData::bay1_pngSize);
    bayFrames[2] = loadImage (BinaryData::bay2_png, (size_t) BinaryData::bay2_pngSize);
    bayFrames[3] = loadImage (BinaryData::bay3_png, (size_t) BinaryData::bay3_pngSize);
    bayFrames[4] = loadImage (BinaryData::bay4_png, (size_t) BinaryData::bay4_pngSize);

    setLookAndFeel (&lnf);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const auto& d = defs[i];

        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
        k.slider.setRotaryParameters (MathConstants<float>::pi * 1.2f,
                                      MathConstants<float>::pi * 2.8f, true);
        k.slider.setTextValueSuffix (d.suffix);
        k.slider.setNumDecimalPlacesToDisplay (d.decimals);
        k.slider.setColour (Slider::textBoxTextColourId, kCream);
        k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        k.slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        addAndMakeVisible (k.slider);

        k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, d.id, k.slider);
    }

    setSize (W, H);
    startTimerHz (30);
}

AmberCompEditor::~AmberCompEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AmberCompEditor::resized()
{
    // même grille que celle de la façade pré-rendue : l'étiquette occupe les 22 px du haut de chaque cellule
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = (int) i % 4, row = (int) i / 4;
        auto cell = Rectangle<int> (352 + col * 116, 72 + row * 165, 116, 165);
        cell.removeFromTop (22);
        knobs[i].slider.setBounds (cell);
    }
}

void AmberCompEditor::timerCallback()
{
    const float gr  = processor.grMeter.load();
    const float lvl = processor.outMeter.load();

    shownGR += (gr - shownGR) * 0.35f;
    shownLevel = jmax (lvl, shownLevel * 0.90f);

    repaint (meterRect.getSmallestIntegerContainer());
    repaint (bayRect.getSmallestIntegerContainer());
}

void AmberCompEditor::drawMeterNeedle (Graphics& g, Rectangle<float> r) const
{
    Path clip;
    clip.addRoundedRectangle (r, 5.0f);
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (clip);

    const float px = r.getCentreX(), py = r.getBottom() - 12.0f;
    const float len = r.getHeight() * 0.86f, aMax = 0.85f;
    const float a = aMax - jlimit (0.0f, 20.0f, shownGR) / 20.0f * (2.0f * aMax);
    const float tx = px + std::sin (a) * (len + 4.0f), ty = py - std::cos (a) * (len + 4.0f);

    auto seg = [&] (float ox, float oy, float thick, Colour col)
    {
        g.setColour (col);
        g.drawLine (px + ox, py + oy, tx + ox, ty + oy, thick);
    };

    seg (4.0f, 5.0f, 2.2f, Colour (0xff281400).withAlpha (0.35f));   // ombre décalée (parallaxe du verre)
    seg (3.0f, 4.0f, 2.2f, Colour (0xff281400).withAlpha (0.20f));
    seg (0.0f, 0.0f, 1.8f, Colour (0xff17110a));
    seg (-0.5f, 0.0f, 0.6f, Colours::white.withAlpha (0.18f));

    g.setColour (Colours::black.withAlpha (0.45f));                   // pivot
    g.fillEllipse (px + 2.0f - 8.5f, py + 3.5f - 8.5f, 17.0f, 17.0f);
    ColourGradient pv (Colour (0xff8a7b5e), px - 3.0f, py - 3.5f, Colour (0xff0b0805), px + 6.0f, py + 5.5f, true);
    pv.addColour (0.5, Colour (0xff2b2418));
    g.setGradientFill (pv);
    g.fillEllipse (px - 8.0f, py - 8.0f, 16.0f, 16.0f);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillEllipse (px - 2.5f - 1.3f, py - 3.0f - 1.3f, 2.6f, 2.6f);
}

void AmberCompEditor::drawBay (Graphics& g) const
{
    // lueur : interpolation entre deux images voisines (de lampes éteintes à pleine lueur)
    const float glow = std::sqrt (jlimit (0.0f, 1.0f, shownLevel));
    const float pos  = glow * (float) (bayFrames.size() - 1);
    const int   i0   = jlimit (0, (int) bayFrames.size() - 2, (int) pos);
    const float f    = pos - (float) i0;

    g.setOpacity (1.0f);
    g.drawImage (bayFrames[(size_t) i0], bayRect);
    if (f > 0.002f)
    {
        g.setOpacity (f);
        g.drawImage (bayFrames[(size_t) i0 + 1], bayRect);
        g.setOpacity (1.0f);
    }
}

void AmberCompEditor::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    drawBay (g);
    drawMeterNeedle (g, meterRect);
    g.drawImage (meterGlass, meterRect);   // reflets du verre par-dessus l'aiguille
}
