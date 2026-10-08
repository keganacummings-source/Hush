#pragma once
#include "RackCanvas.h"
#include "Parts.h"
#include "PluginProcessor.h"

// VIEWER — the payoff screen. Every effect you placed in the builder is fused into ONE
// machine: each unit becomes a recognisable hardware part (scope, speaker, valve, reels,
// coils, lamp bank, VU gauge...) chosen deterministically from the effect, detailed with
// procedural decals gated by the effect's numeric values, and animated by live audio.
class Viewer : public juce::Component, private juce::Timer
{
public:
    explicit Viewer (KyotoProcessor& p) : proc (p), rack (p.rack) { startTimerHz (30); }

    void rebuild()
    {
        parts.clear();
        for (auto& n : rack.nodes)
            addAndMakeVisible (parts.add (new Part (rack, n.id)));
        resized(); repaint();
    }
    void syncValues() { for (auto* p : parts) p->sync(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (22, 18);
        r.removeFromTop (118);                       // master header + scope painted below
        if (parts.isEmpty()) return;
        const int n = parts.size();
        const int cols = juce::jlimit (1, 4, (int) std::ceil (std::sqrt ((double) n * r.getWidth() / juce::jmax (1, r.getHeight()))));
        const int rows = (n + cols - 1) / cols;
        const int w = r.getWidth() / cols;
        const int h = juce::jlimit (150, 280, r.getHeight() / juce::jmax (1, rows));
        for (int i = 0; i < n; ++i)
            parts[i]->setBounds (juce::Rectangle<int> (r.getX() + (i % cols) * w, r.getY() + (i / cols) * h, w, h).reduced (7));
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // chassis: one brushed faceplate for the whole machine
        g.setGradientFill (juce::ColourGradient (ui::panel().brighter (0.08f), b.getX(), b.getY(),
                                                 ui::bg().darker (0.3f), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 12.0f);
        for (float y = b.getY(); y < b.getBottom(); y += 2.5f) { g.setColour (juce::Colours::white.withAlpha (0.015f)); g.fillRect (b.getX(), y, b.getWidth(), 1.0f); }
        g.setColour (ui::edge()); g.drawRoundedRectangle (b.reduced (1.0f), 12.0f, 1.4f);
        ui::cornerBrackets (g, b.reduced (8.0f), ui::accent(), 24.0f);
        for (auto c : { b.getTopLeft() + juce::Point<float> (16, 16), b.getTopRight() + juce::Point<float> (-16, 16), b.getBottomLeft() + juce::Point<float> (16, -16), b.getBottomRight() + juce::Point<float> (-16, -16) })
            parts::screw (g, c, 5.0f, ui::accent());

        // ---- master header: name plate + big live output scope ----
        auto head = b.reduced (26, 20).removeFromTop (96.0f);
        auto plate = head.removeFromLeft (260.0f);
        auto nameRow = plate.removeFromTop (30.0f);
        ui::pill (g, nameRow, ui::accent());
        g.setColour (ui::bg()); g.setFont (ui::hud (20.0f));
        g.drawText ("KYOTOVST  MACHINE", nameRow.reduced (14, 2), juce::Justification::centredLeft);
        g.setColour (ui::muted()); g.setFont (ui::hud (11.0f, false));
        g.drawText (juce::String (parts.size()) + " PARTS FUSED  //  LIVE", plate.removeFromTop (18.0f), juce::Justification::left);

        head.removeFromLeft (16.0f);
        parts::recessed (g, head, ui::accent());
        auto scope = head.reduced (8.0f);
        g.setColour (ui::accent().withAlpha (0.08f)); for (float x = scope.getX(); x < scope.getRight(); x += 10) g.fillRect (x, scope.getY(), 1.0f, scope.getHeight());
        const float out = juce::jlimit (0.0f, 1.0f, master);
        juce::Path w; const float cy = scope.getCentreY(); w.startNewSubPath (scope.getX(), cy);
        for (float x = 0; x <= scope.getWidth(); x += 2.0f)
        {
            const float u = x / scope.getWidth();
            const float y = std::sin (u * 22.0f + (float) phase * 2.2f) * (0.2f + out) + 0.3f * std::sin (u * 60.0f + (float) phase * 5.0f) * out;
            w.lineTo (scope.getX() + x, cy + y * scope.getHeight() * 0.42f);
        }
        g.setColour (ui::hot().withAlpha (0.25f)); g.strokePath (w, juce::PathStrokeType (4.0f));
        g.setColour (ui::hot()); g.strokePath (w, juce::PathStrokeType (1.6f));

        if (parts.isEmpty())
        {
            g.setColour (ui::muted()); g.setFont (ui::hud (15.0f, false));
            g.drawText ("PLACE EFFECTS IN THE PATCH BAY, THEN RETURN HERE TO SEE YOUR MACHINE", b, juce::Justification::centred);
        }
    }

private:
    struct Part : public juce::Component
    {
        Part (kv::Rack& r, int nodeId) : rack (r), id (nodeId)
        {
            auto* n = rack.find (id);
            hero = parts::heroFor (n ? n->fx : 0);
            seed = parts::hashString ((n && n->label.isNotEmpty() ? n->label : juce::String (kt::kFx[n ? n->fx : 0].name)) + juce::String (id));
            const char* caps[] = { "AMT", "TONE", "MOT", "SHP", "MIX" };
            for (int i = 0; i < 5; ++i)
            {
                auto* k = knobs.add (new ui::Knob (caps[i]));
                k->slider.getProperties().set ("face", n ? n->face : 0);
                k->slider.getProperties().set ("tint", (juce::int64) (n ? RackCanvas::nodeColour (*n).getARGB() : ui::accent().getARGB()));
                k->onChange = [this, i] (float v) { auto& p = rack.paramsFor (id); (i < 4 ? p.p[i] : p.mix) = v; };
                addAndMakeVisible (k);
            }
            power.setClickingTogglesState (true);
            power.onClick = [this] { rack.paramsFor (id).bypass = ! power.getToggleState(); };
            addAndMakeVisible (power);
            sync();
        }
        void sync()
        {
            auto& p = rack.paramsFor (id);
            for (int i = 0; i < 5; ++i) knobs[i]->slider.setValue (i < 4 ? p.p[i].load() : p.mix.load(), juce::dontSendNotification);
            power.setToggleState (! p.bypass.load(), juce::dontSendNotification);
        }
        void tick (double t)
        {
            time = t;
            const float target = rack.find (id) ? rack.paramsFor (id).vu.load() : 0.0f;
            level = juce::jmax (target, level * 0.86f);
            repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10);
            power.setBounds (r.removeFromTop (20).removeFromRight (54));
            auto controls = r.removeFromBottom (58);
            heroArea = r.withTrimmedBottom (6).withTrimmedTop (18).toFloat();
            const int w = controls.getWidth() / 5;
            for (auto* k : knobs) k->setBounds (controls.removeFromLeft (w).reduced (1));
        }
        void paint (juce::Graphics& g) override
        {
            auto* n = rack.find (id); if (n == nullptr) return;
            const auto col = RackCanvas::nodeColour (*n);
            auto r = getLocalBounds().toFloat();
            // recessed panel seam so the whole viewer reads as one chassis
            g.setColour (juce::Colours::black.withAlpha (0.22f)); g.fillRoundedRectangle (r, 8.0f);
            g.setColour (ui::edge().withAlpha (0.5f)); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            const bool on = ! rack.paramsFor (id).bypass.load();

            // title strip
            ui::pill (g, { r.getX() + 8.0f, r.getY() + 8.0f, 10.0f, 16.0f }, on ? col : ui::muted());
            g.setColour (ui::text()); g.setFont (ui::hud (13.0f));
            const auto title = (n->label.isNotEmpty() ? n->label : juce::String (kt::kFx[n->fx].name));
            g.drawText (title.toUpperCase(), (int) r.getX() + 24, (int) r.getY() + 7, getWidth() - 90, 18, juce::Justification::centredLeft, true);
            g.setColour (ui::muted()); g.setFont (ui::hud (8.5f, false));
            g.drawText (juce::String (parts::heroName (hero)), (int) r.getX() + 24, (int) r.getY() + 23, getWidth() - 70, 12, juce::Justification::centredLeft, true);

            float p[5]; auto& pp = rack.paramsFor (id);
            for (int i = 0; i < 4; ++i) p[i] = pp.p[i].load();
            p[4] = pp.mix.load();
            const float lv = on ? level : 0.0f;
            parts::drawHero (g, hero, heroArea, on ? col : ui::muted(), p, lv, time, seed);
            parts::dressAround (g, r.reduced (2.0f).withTrimmedBottom (62.0f).withTrimmedTop (34.0f), on ? col : ui::muted(), p, lv, seed);

            g.setColour (on ? ui::hot() : juce::Colours::black); g.fillEllipse (r.getRight() - 72.0f, r.getY() + 11.0f, 8.0f, 8.0f);
            if (! on) { g.setColour (ui::bg().withAlpha (0.45f)); g.fillRoundedRectangle (heroArea, 4.0f); }
        }
        kv::Rack& rack; int id;
        parts::Hero hero; juce::uint32 seed = 0;
        double time = 0.0; float level = 0.0f;
        juce::Rectangle<float> heroArea;
        juce::OwnedArray<ui::Knob> knobs;
        juce::TextButton power { "ON" };
    };

    void timerCallback() override
    {
        if (! isShowing()) return;
        phase += 0.03;
        master = juce::jmax (rack.outVu.load(), master * 0.86f);
        for (auto* p : parts) p->tick (phase);
        repaint (getLocalBounds().removeFromTop (130));
    }

    KyotoProcessor& proc;
    kv::Rack& rack;
    juce::OwnedArray<Part> parts;
    double phase = 0.0;
    float master = 0.0f;
};
