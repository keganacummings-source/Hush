#pragma once
#include <JuceHeader.h>
#include "Themes.h"

// Sci-fi console look: LCARS-style pill segments, machined knobs, glowing readouts.
// Every colour derives from the active kt::ThemePalette so all site themes re-skin the hardware.
namespace ui
{
inline const kt::ThemePalette*& palPtr() { static const kt::ThemePalette* p = &kt::kThemes[0]; return p; }
inline const kt::ThemePalette& pal() { return *palPtr(); }
inline juce::Colour bg()     { return juce::Colour (pal().bg); }
inline juce::Colour panel()  { return juce::Colour (pal().panel); }
inline juce::Colour accent() { return juce::Colour (pal().accent); }
inline juce::Colour hot()    { return juce::Colour (pal().pegHot); }
inline juce::Colour text()   { return juce::Colour (pal().text); }
inline juce::Colour muted()  { return juce::Colour (pal().muted); }
inline juce::Colour edge()   { return juce::Colour (pal().border); }
inline juce::Colour peg()    { return juce::Colour (pal().peg); }
// Theme-role colours: everything (families, faceplates, rails) is a blend of the active theme, never a fixed hue.
constexpr int kRoleCount = 8;
inline const char* kRoleNames[kRoleCount] = { "Theme accent", "Theme highlight", "Accent/highlight blend", "Deep accent", "Deep highlight", "Bright accent", "Muted steel", "Panel text" };
inline juce::Colour role (int i)
{
    switch (((i % kRoleCount) + kRoleCount) % kRoleCount)
    {
        case 0:  return accent();
        case 1:  return hot();
        case 2:  return accent().interpolatedWith (hot(), 0.5f);
        case 3:  return accent().darker (0.35f);
        case 4:  return hot().darker (0.35f);
        case 5:  return accent().brighter (0.35f);
        case 6:  return muted().brighter (0.15f);
        default: return text().interpolatedWith (accent(), 0.25f);
    }
}
inline juce::Colour family (int f) { return role (f); }
inline float radius (float scale = 1.0f) { return juce::jlimit (2.0f, 14.0f, pal().cornerRadius * scale); }

inline juce::Font hud (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (bold ? juce::String ("Bahnschrift") : juce::String (pal().fontFamily), size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.06f);
}

inline void pill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { g.setColour (c); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }

inline void bezel (juce::Graphics& g, juce::Rectangle<float> r, float radiusHint = 6.0f, juce::Colour tint = {})
{
    const float radius = juce::jmin (radiusHint, juce::jlimit (2.0f, 14.0f, pal().cornerRadius + 2.0f));
    auto base = tint.isTransparent() ? panel() : panel().interpolatedWith (tint, 0.18f);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.10f), r.getX(), r.getY(), base.darker (0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.reduced (1.0f), radius, 1.0f);
    g.setColour (edge()); g.drawRoundedRectangle (r, radius, 1.2f);
}

inline void cornerBrackets (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float len = 14.0f)
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getY() + len); p.lineTo (r.getX(), r.getY()); p.lineTo (r.getX() + len, r.getY());
    p.startNewSubPath (r.getRight() - len, r.getY()); p.lineTo (r.getRight(), r.getY()); p.lineTo (r.getRight(), r.getY() + len);
    p.startNewSubPath (r.getRight(), r.getBottom() - len); p.lineTo (r.getRight(), r.getBottom()); p.lineTo (r.getRight() - len, r.getBottom());
    p.startNewSubPath (r.getX() + len, r.getBottom()); p.lineTo (r.getX(), r.getBottom()); p.lineTo (r.getX(), r.getBottom() - len);
    g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f));
}

class Lnf : public juce::LookAndFeel_V4
{
public:
    Lnf() { refresh(); }
    void refresh()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg());
        setColour (juce::TextEditor::backgroundColourId, bg().darker (0.3f));
        setColour (juce::TextEditor::textColourId, text());
        setColour (juce::TextEditor::outlineColourId, edge());
        setColour (juce::TextEditor::focusedOutlineColourId, accent());
        setColour (juce::TextEditor::highlightColourId, accent().withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, accent());
        setColour (juce::ComboBox::backgroundColourId, bg().darker (0.2f));
        setColour (juce::ComboBox::textColourId, text());
        setColour (juce::ComboBox::outlineColourId, edge());
        setColour (juce::ComboBox::arrowColourId, accent());
        setColour (juce::PopupMenu::backgroundColourId, panel().darker (0.2f));
        setColour (juce::PopupMenu::textColourId, text());
        setColour (juce::PopupMenu::headerTextColourId, accent());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.8f));
        setColour (juce::PopupMenu::highlightedTextColourId, bg());
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text());
        setColour (juce::ScrollBar::thumbColourId, accent().withAlpha (0.5f));
        setColour (juce::TextButton::textColourOffId, bg());
        setColour (juce::TextButton::textColourOnId, bg());
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return hud (juce::jmin (15.0f, (float) h * 0.52f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return hud (14.0f, false); }
    juce::Font getPopupMenuFont() override { return hud (15.0f, false); }
    juce::Font getLabelFont (juce::Label& l) override { return hud ((float) juce::jmax (11, l.getHeight() - 8), false); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto base = b.getToggleState() ? accent() : (b.getProperties().contains ("role") ? role ((int) b.getProperties()["role"]).withMultipliedSaturation (0.8f) : muted());
        if (! b.isEnabled()) base = base.withAlpha (0.35f);
        if (over) base = base.brighter (0.18f);
        if (down) base = base.darker (0.2f);
        pill (g, r, base);
        if (b.getToggleState()) { g.setColour (accent().withAlpha (0.25f)); g.drawRoundedRectangle (r.expanded (1.5f), r.getHeight() * 0.5f, 2.0f); }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (bg().darker (0.4f));
        g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (10, 0), juce::Justification::centredRight, 1);
    }

    // Slider property "face": 0 machined, 1 LED ring, 2 chicken-head pointer.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const auto c = r.getCentre();
        const float rad = d * 0.5f, ang = a0 + pos * (a1 - a0);
        const int face = (int) s.getProperties()["face"];
        const auto col = s.getProperties().contains ("tint") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["tint"]) : accent();

        const int ticks = face == 1 ? 24 : 10;
        for (int i = 0; i <= ticks; ++i)
        {
            const float t = a0 + (float) i / (float) ticks * (a1 - a0);
            const bool lit = (float) i / (float) ticks <= pos;
            g.setColour (lit ? col : edge());
            if (face == 1) { auto pt = c.getPointOnCircumference (rad - 2.0f, t); g.fillEllipse (pt.x - 2, pt.y - 2, 4, 4); }
            else g.drawLine (juce::Line<float> (c.getPointOnCircumference (rad, t), c.getPointOnCircumference (rad - 3.5f, t)), 1.4f);
        }
        if (face != 1)
        {
            juce::Path track; track.addCentredArc (c.x, c.y, rad - 7.0f, rad - 7.0f, 0, a0, a1, true);
            g.setColour (bg().darker (0.4f)); g.strokePath (track, juce::PathStrokeType (3.0f));
            juce::Path val; val.addCentredArc (c.x, c.y, rad - 7.0f, rad - 7.0f, 0, a0, ang, true);
            g.setColour (col.withAlpha (0.25f)); g.strokePath (val, juce::PathStrokeType (7.0f));
            g.setColour (col); g.strokePath (val, juce::PathStrokeType (3.0f));
        }
        auto cap = r.reduced (d * (face == 2 ? 0.2f : 0.26f));
        g.setGradientFill (juce::ColourGradient (panel().brighter (0.35f), cap.getX(), cap.getY(), bg().darker (0.5f), cap.getRight(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (cap, 1.2f);
        if (face == 2)
        {
            juce::Path ptr; ptr.addTriangle (-cap.getWidth() * 0.12f, 0, cap.getWidth() * 0.12f, 0, 0, -cap.getWidth() * 0.62f);
            g.setColour (text().withAlpha (0.9f)); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c));
        }
        else
        {
            g.setColour (hot());
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (cap.getWidth() * 0.12f, ang), c.getPointOnCircumference (cap.getWidth() * 0.46f, ang)), 2.2f);
        }
        if (s.isMouseOverOrDragging()) { g.setColour (col.withAlpha (0.12f)); g.fillEllipse (cap); }
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (bg().darker (0.25f)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (edge()); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (accent()); g.fillRect (r.removeFromLeft (3.0f));
        juce::Path arrow; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.fillPath (arrow);
    }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override { l.setBounds (8, 0, b.getWidth() - 26, b.getHeight()); l.setFont (getComboBoxFont (b)); }
};

class Knob : public juce::Component
{
public:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String caption;
    std::function<void (float)> onChange;

    explicit Knob (juce::String cap = {}) : caption (cap)
    {
        slider.setRange (0.0, 1.0);
        slider.setDoubleClickReturnValue (true, 0.5);
        slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        slider.onValueChange = [this] { repaint(); if (onChange) onChange ((float) slider.getValue()); };
        addAndMakeVisible (slider);
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (28)); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().removeFromBottom (28);
        g.setColour (muted()); g.setFont (hud (11.0f));
        g.drawText (caption, b.removeFromTop (13), juce::Justification::centred);
        g.setColour (accent()); g.setFont (hud (13.0f, false));
        g.drawText (slider.getTextFromValue (slider.getValue()), b, juce::Justification::centred);
    }
};
} // namespace ui
