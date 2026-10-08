#pragma once
#include "RackCanvas.h"

// Viewer mode: the built plugin as a standalone instrument face. No builder chrome, just the machine.
class Viewer : public juce::Component
{
public:
    explicit Viewer (kv::Rack& r) : rack (r) {}

    void rebuild()
    {
        plates.clear();
        for (auto& n : rack.nodes)
            if (! n.hidden) addAndMakeVisible (plates.add (new Plate (rack, n.id)));
        resized(); repaint();
    }
    void syncValues() { for (auto* p : plates) p->sync(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 20).withTrimmedTop (40);
        const int cols = juce::jmax (1, juce::jmin ((int) plates.size(), r.getWidth() / 300));
        const int rows = juce::jmax (1, ((int) plates.size() + cols - 1) / cols);
        const int w = r.getWidth() / cols, h = juce::jmin (230, r.getHeight() / rows);
        for (int i = 0; i < plates.size(); ++i)
            plates[i]->setBounds (juce::Rectangle<int> (r.getX() + (i % cols) * w, r.getY() + (i / cols) * h, w, h).reduced (8));
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (ui::bg().darker (0.35f)); g.fillRoundedRectangle (b, 10.0f);
        for (float y = 0; y < b.getHeight(); y += 3) { g.setColour (ui::text().withAlpha (0.025f)); g.fillRect (0.0f, y, b.getWidth(), 1.0f); }
        ui::cornerBrackets (g, b.reduced (8.0f), ui::accent(), 22.0f);
        g.setColour (ui::accent()); g.setFont (ui::hud (16.0f));
        g.drawText ("VIEWER  //  " + juce::String (plates.size()) + " STAGES ONLINE", 30, 16, 600, 24, juce::Justification::left);
        if (plates.isEmpty()) { g.setColour (ui::muted()); g.drawText ("NO VISIBLE MODULES - USE 'SHOW IN VIEWER' IN THE DESIGNER", b, juce::Justification::centred); }
    }

private:
    struct Plate : public juce::Component
    {
        Plate (kv::Rack& r, int nodeId) : rack (r), id (nodeId)
        {
            const char* caps[] = { "AMOUNT", "TONE", "MOTION", "SHAPE", "MIX" };
            auto* n = rack.find (id);
            for (int i = 0; i < 5; ++i)
            {
                auto* k = knobs.add (new ui::Knob (caps[i]));
                k->slider.getProperties().set ("face", n->face);
                k->slider.getProperties().set ("tint", (juce::int64) RackCanvas::nodeColour (*n).getARGB());
                k->onChange = [this, i] (float v) { auto& p = rack.paramsFor (id); (i < 4 ? p.p[i] : p.mix) = v; };
                addAndMakeVisible (k);
            }
            power.setClickingTogglesState (true);
            power.onClick = [this] { rack.paramsFor (id).bypass = ! power.getToggleState(); repaint(); };
            addAndMakeVisible (power);
            sync();
        }
        void sync()
        {
            auto& p = rack.paramsFor (id);
            for (int i = 0; i < 5; ++i) knobs[i]->slider.setValue (i < 4 ? p.p[i].load() : p.mix.load(), juce::dontSendNotification);
            power.setToggleState (! p.bypass.load(), juce::dontSendNotification);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (12).withTrimmedTop (44);
            power.setBounds (getWidth() - 74, 12, 60, 22);
            const int w = r.getWidth() / 5;
            for (auto* k : knobs) k->setBounds (r.removeFromLeft (w).reduced (2));
        }
        void paint (juce::Graphics& g) override
        {
            auto* n = rack.find (id); if (n == nullptr) return;
            const auto col = RackCanvas::nodeColour (*n);
            auto r = getLocalBounds().toFloat();
            ui::bezel (g, r, 10.0f, col);
            for (auto pt : { r.getTopLeft() + juce::Point<float> (7, 7), r.getTopRight() + juce::Point<float> (-7, 7), r.getBottomLeft() + juce::Point<float> (7, -7), r.getBottomRight() + juce::Point<float> (-7, -7) })
            { g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6); }
            ui::pill (g, { 16.0f, 12.0f, 46.0f, 22.0f }, col);
            g.setColour (ui::text()); g.setFont (ui::hud (17.0f));
            const auto title = n->label.isNotEmpty() ? n->label + "  /  " + kt::kFx[n->fx].name : juce::String (kt::kFx[n->fx].name);
            g.drawText (title.toUpperCase(), 72, 10, getWidth() - 160, 26, juce::Justification::centredLeft, true);
            const bool on = ! rack.paramsFor (id).bypass.load();
            g.setColour (on ? ui::hot() : juce::Colours::black); g.fillEllipse ((float) getWidth() - 90.0f, 18.0f, 9.0f, 9.0f);
        }
        kv::Rack& rack; int id;
        juce::OwnedArray<ui::Knob> knobs;
        juce::TextButton power { "ON" };
    };
    kv::Rack& rack;
    juce::OwnedArray<Plate> plates;
};
