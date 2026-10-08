#pragma once
#include "Ui.h"
#include "Rack.h"
#include "Machines.h"

// Patch bay: modules are hardware cards, cables run from an OUT jack (right) to an IN jack (left).
// Right-click is the designer: rename, faceplate colour, knob face, viewer visibility, routing tools.
class RackCanvas : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    static constexpr float kW = 168.0f, kH = 92.0f, kJack = 7.0f;
    std::function<void()> onPatchChanged, onDesignChanged;
    std::function<void (int)> onSelect;
    int selected = kv::kIn;

    explicit RackCanvas (kv::Rack& r) : rack (r) { setWantsKeyboardFocus (true); startTimerHz (30); }

    void select (int id) { selected = id; selWire = -1; if (onSelect) onSelect (id); repaint(); }
    void changed() { if (onPatchChanged) onPatchChanged(); repaint(); }

    juce::Point<float> addPoint() const
    {
        const int i = (int) rack.nodes.size();
        return { juce::jmin (120.0f + (float) (i % 5) * (kW + 26.0f), (float) getWidth() - kW - 70.0f),
                 juce::jmin (60.0f + (float) ((i / 5) % 4) * (kH + 34.0f), (float) getHeight() - kH - 10.0f) };
    }

    void addFx (int fx, juce::Point<float> at)
    {
        const int id = rack.addNode (fx, at.x, at.y);
        if (id >= 0) { if (auto* n = rack.find (id)) clampNode (*n); autoLink (id); changed(); select (id); }
    }

    void addMachine (int m, juce::Point<float> at)
    {
        auto ids = rack.addMachine (kv::kMachines[m].recipe, at.x, at.y);
        for (auto id : ids) if (auto* n = rack.find (id)) { n->label = kv::kMachines[m].name; clampNode (*n); }
        changed();
        if (! ids.isEmpty()) select (ids.getFirst());
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (ui::bg().darker (0.25f)); g.fillRoundedRectangle (b, 8.0f);
        g.setColour (ui::edge().withAlpha (0.35f));
        for (float x = 24; x < b.getWidth(); x += 24) for (float y = 24; y < b.getHeight(); y += 24) g.fillRect (x, y, 1.2f, 1.2f);
        ui::cornerBrackets (g, b.reduced (6.0f), ui::accent().withAlpha (0.6f));
        g.setColour (ui::muted().withAlpha (0.7f)); g.setFont (ui::hud (11.0f));
        g.drawText ("PATCH BAY  //  " + juce::String ((int) rack.nodes.size()) + " MODULES  //  " + juce::String ((int) rack.wires.size()) + " LINKS  //  RIGHT-CLICK TO DESIGN",
                    b.reduced (16, 10).removeFromTop (14), juce::Justification::left);
        drawEndpoint (g, true); drawEndpoint (g, false);
        for (int i = 0; i < (int) rack.wires.size(); ++i)
        {
            auto& w = rack.wires[(size_t) i];
            const bool off = w.from >= 0 && rack.paramsFor (w.from).bypass.load();
            drawCable (g, outJack (w.from), inJack (w.to), i == selWire ? ui::hot() : (off ? ui::muted() : wireColour (w.from)), ! off);
        }
        if (dragFrom != -999) drawCable (g, outJack (dragFrom), dragPos, ui::hot(), true);
        for (auto& n : rack.nodes) drawNode (g, n);
        if (rack.nodes.empty())
        {
            g.setColour (ui::muted()); g.setFont (ui::hud (16.0f, false));
            g.drawText ("DRAG A UNIT OR MACHINE IN  -  RIGHT JACK TO LEFT JACK TO LINK  -  RIGHT-CLICK FOR THE DESIGNER", b, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        const auto p = e.position;
        if (! e.mods.isPopupMenu())
            for (int src : sources())
                if (outJack (src).getDistanceFrom (p) <= kJack + 5) { dragFrom = src; dragPos = p; return; }
        if (auto* n = nodeAt (p))
        {
            select (n->id);
            if (e.mods.isPopupMenu()) { nodeMenu (n->id); return; }
            moving = n->id; grabOffset = p - juce::Point<float> (n->x, n->y);
            return;
        }
        selWire = wireAt (p);
        if (selWire >= 0 && e.mods.isPopupMenu()) { auto w = rack.wires[(size_t) selWire]; rack.disconnect (w.from, w.to); selWire = -1; changed(); return; }
        if (selWire < 0) { select (kv::kIn); if (e.mods.isPopupMenu()) canvasMenu (p); }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragFrom != -999) { dragPos = e.position; repaint(); return; }
        if (auto* n = rack.find (moving)) { n->x = e.position.x - grabOffset.x; n->y = e.position.y - grabOffset.y; clampNode (*n); repaint(); }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragFrom != -999)
        {
            int target = -999;
            if (inJack (kv::kOut).getDistanceFrom (e.position) <= kJack + 12) target = kv::kOut;
            else if (auto* n = nodeAt (e.position)) target = n->id;
            if (target != -999 && rack.connect (dragFrom, target)) changed();
            dragFrom = -999; repaint();
        }
        moving = -999;
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k != juce::KeyPress::deleteKey && k != juce::KeyPress::backspaceKey) return false;
        if (selWire >= 0) { auto w = rack.wires[(size_t) selWire]; rack.disconnect (w.from, w.to); selWire = -1; changed(); }
        else if (rack.find (selected)) { rack.removeNode (selected); changed(); select (kv::kIn); }
        return true;
    }

    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().containsChar (':'); }
    void itemDropped (const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        const int idx = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
        const auto at = d.localPosition.toFloat() - juce::Point<float> (kW * 0.5f, kH * 0.5f);
        if (s.startsWith ("fx:")) addFx (idx, at); else addMachine (idx, at);
    }

    static juce::Colour nodeColour (const kv::Node& n)
    {
        if (n.colour == 0) return ui::family (kt::kFx[n.fx].family);
        return n.colour <= (juce::uint32) ui::kRoleCount ? ui::role ((int) n.colour - 1) : juce::Colour (n.colour);
    }

private:
    kv::Rack& rack;
    int dragFrom = -999, moving = -999, selWire = -1;
    juce::Point<float> dragPos, grabOffset;
    float phase = 0.0f;

    void timerCallback() override { phase = std::fmod (phase + 0.02f, 0.25f); if (! rack.wires.empty() && isShowing()) repaint(); }

    juce::Colour wireColour (int from) { if (auto* n = rack.find (from)) return nodeColour (*n); return ui::accent(); }

    void clampNode (kv::Node& n) const
    {
        n.x = juce::jlimit (70.0f, juce::jmax (70.0f, (float) getWidth() - kW - 70.0f), n.x);
        n.y = juce::jlimit (30.0f, juce::jmax (30.0f, (float) getHeight() - kH - 8.0f), n.y);
    }

    // New unit joins the end of the main chain (just before OUT).
    void autoLink (int id)
    {
        int last = kv::kIn;
        for (auto& w : rack.wires) if (w.to == kv::kOut) { last = w.from; break; }
        if (last != kv::kIn || rack.wires.empty() || rack.connected (kv::kIn, kv::kOut))
        {
            rack.disconnect (last, kv::kOut);
            rack.connect (last, id); rack.connect (id, kv::kOut);
        }
    }

    std::vector<int> sources() const { std::vector<int> s { kv::kIn }; for (auto& n : rack.nodes) s.push_back (n.id); return s; }
    juce::Point<float> outJack (int id) const
    {
        if (id == kv::kIn) return { 34.0f, (float) getHeight() * 0.5f };
        for (auto& n : rack.nodes) if (n.id == id) return { n.x + kW, n.y + kH * 0.5f };
        return {};
    }
    juce::Point<float> inJack (int id) const
    {
        if (id == kv::kOut) return { (float) getWidth() - 34.0f, (float) getHeight() * 0.5f };
        for (auto& n : rack.nodes) if (n.id == id) return { n.x, n.y + kH * 0.5f };
        return {};
    }
    kv::Node* nodeAt (juce::Point<float> p)
    {
        for (auto it = rack.nodes.rbegin(); it != rack.nodes.rend(); ++it)
            if (juce::Rectangle<float> (it->x, it->y, kW, kH).expanded (6.0f, 0).contains (p)) return &*it;
        return nullptr;
    }
    static juce::Path cable (juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p; const float dx = juce::jmax (40.0f, std::abs (b.x - a.x) * 0.5f);
        p.startNewSubPath (a); p.cubicTo (a.x + dx, a.y, b.x - dx, b.y, b.x, b.y);
        return p;
    }
    int wireAt (juce::Point<float> p) const
    {
        for (int i = 0; i < (int) rack.wires.size(); ++i)
        {
            juce::Point<float> near;
            cable (outJack (rack.wires[(size_t) i].from), inJack (rack.wires[(size_t) i].to)).getNearestPoint (p, near);
            if (near.getDistanceFrom (p) < 6.0f) return i;
        }
        return -1;
    }

    void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, bool live)
    {
        auto p = cable (a, b);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (5.0f), juce::AffineTransform::translation (0, 2));
        g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (7.0f));
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (2.2f));
        if (! live) return;
        const float len = p.getLength();
        for (float t = phase; t < 1.0f; t += 0.25f)
        {
            auto pt = p.getPointAlongPath (t * len);
            g.setColour (c.brighter (0.8f)); g.fillEllipse (pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
        }
    }
    void drawJack (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colours::black); g.fillEllipse (c.x - kJack, c.y - kJack, kJack * 2, kJack * 2);
        g.setColour (ui::accent()); g.drawEllipse (c.x - kJack, c.y - kJack, kJack * 2, kJack * 2, 2.0f);
        g.setColour (ui::panel().brighter (0.4f)); g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
    }
    void drawEndpoint (juce::Graphics& g, bool in)
    {
        auto c = in ? outJack (kv::kIn) : inJack (kv::kOut);
        auto r = juce::Rectangle<float> (40.0f, 120.0f).withCentre (c);
        ui::bezel (g, r, 20.0f);
        g.setColour (ui::accent()); g.setFont (ui::hud (12.0f));
        g.drawText (in ? "IN" : "OUT", r.removeFromTop (28), juce::Justification::centred);
        drawJack (g, c);
    }

    void drawNode (juce::Graphics& g, const kv::Node& n)
    {
        auto r = juce::Rectangle<float> (n.x, n.y, kW, kH);
        auto& p = rack.paramsFor (n.id);
        const auto& def = kt::kFx[n.fx];
        const bool off = p.bypass.load();
        const auto col = nodeColour (n);
        if (n.id == selected) { g.setColour (ui::accent().withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (5.0f), 9.0f); }
        ui::bezel (g, r, 7.0f, n.colour != 0 ? col : juce::Colour());
        auto head = r.reduced (5.0f).removeFromTop (20.0f);
        ui::pill (g, head.removeFromLeft (34.0f), off ? ui::muted() : col);
        g.setColour (off ? ui::muted() : ui::text()); g.setFont (ui::hud (13.0f));
        g.drawText (juce::String (def.name).toUpperCase(), head.withTrimmedLeft (6.0f).withTrimmedRight (14.0f), juce::Justification::centredLeft, true);
        g.setColour (off ? juce::Colours::black : ui::hot()); g.fillEllipse (head.getRight() - 9.0f, head.getCentreY() - 3.5f, 7.0f, 7.0f);
        auto body = r.reduced (10.0f).withTrimmedTop (22.0f);
        g.setColour (ui::muted()); g.setFont (ui::hud (10.0f, false));
        g.drawText ((n.label.isNotEmpty() ? n.label : juce::String (kt::fxFamilyName (def.family))).toUpperCase()
                    + (off ? "  //  BYPASS" : "") + (n.hidden ? "  //  HIDDEN" : ""), body.removeFromTop (13.0f), juce::Justification::centredLeft, true);
        body.removeFromTop (4.0f);
        const float bw = (body.getWidth() - 20.0f) / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            auto cell = body.removeFromLeft (bw); body.removeFromLeft (5.0f);
            const float v = i < 4 ? p.p[i].load() : p.mix.load();
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cell, 2.0f);
            g.setColour ((i < 4 ? col : ui::hot()).withAlpha (off ? 0.3f : 0.9f));
            g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - v)), 2.0f);
        }
        drawJack (g, inJack (n.id)); drawJack (g, outJack (n.id));
    }

    void design() { if (onDesignChanged) onDesignChanged(); repaint(); }

    void nodeMenu (int id)
    {
        auto* n = rack.find (id);
        juce::PopupMenu m, colours, faces;
        colours.addItem (100, "Family default", true, n->colour == 0);
        for (int i = 0; i < ui::kRoleCount; ++i) colours.addItem (101 + i, ui::kRoleNames[i], true, n->colour == (juce::uint32) (i + 1));
        const char* faceNames[] = { "Machined arc", "LED ring", "Chicken-head" };
        for (int i = 0; i < 3; ++i) faces.addItem (200 + i, faceNames[i], true, n->face == i);
        m.addSectionHeader (juce::String (kt::kFx[n->fx].name).toUpperCase());
        m.addItem (1, "Rename...");
        m.addSubMenu ("Faceplate colour", colours);
        m.addSubMenu ("Knob face", faces);
        m.addItem (2, "Show in Viewer", true, ! n->hidden);
        m.addSeparator();
        m.addItem (3, rack.paramsFor (id).bypass.load() ? "Enable" : "Bypass");
        m.addItem (4, "Duplicate");
        m.addItem (5, "Insert after OUT chain");
        m.addItem (6, "Disconnect all");
        m.addSeparator();
        m.addItem (7, "Remove module");
        juce::Component::SafePointer<RackCanvas> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp, id] (int r) {
            if (sp == nullptr) return;
            auto& rk = sp->rack; auto* nd = rk.find (id); if (nd == nullptr || r == 0) return;
            if (r >= 100 && r <= 100 + ui::kRoleCount) { nd->colour = (juce::uint32) (r - 100); sp->design(); }
            else if (r >= 200) { nd->face = r - 200; sp->design(); }
            else if (r == 1) sp->rename (id);
            else if (r == 2) { nd->hidden = ! nd->hidden; sp->design(); }
            else if (r == 3) { auto& pp = rk.paramsFor (id); pp.bypass = ! pp.bypass.load(); sp->select (id); }
            else if (r == 4) { const int c = rk.addNode (nd->fx, nd->x + 24, nd->y + 24);
                               if (c >= 0) { auto copy = *rk.find (id); copy.id = c; copy.x += 24; copy.y += 24; *rk.find (c) = copy; rk.paramsFor (c).copyFrom (rk.paramsFor (id)); sp->changed(); sp->select (c); } }
            else if (r == 5) { rk.unwire (id); sp->autoLink (id); sp->changed(); }
            else if (r == 6) { rk.unwire (id); sp->changed(); }
            else if (r == 7) { rk.removeNode (id); sp->changed(); sp->select (kv::kIn); }
        });
    }

    void rename (int id)
    {
        auto* w = new juce::AlertWindow ("RENAME MODULE", "Label shown on the patch bay and in the Viewer", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("label", rack.find (id)->label);
        w->addButton ("APPLY", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("CANCEL", 0);
        juce::Component::SafePointer<RackCanvas> sp (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([sp, w, id] (int r) {
            if (sp != nullptr && r == 1) if (auto* n = sp->rack.find (id)) { n->label = w->getTextEditorContents ("label").substring (0, 40); sp->design(); }
        }), true);
    }

    void canvasMenu (juce::Point<float> at)
    {
        juce::PopupMenu m, machines;
        juce::PopupMenu fam[kt::kFxFamilyCount], cats[kv::NumMachineCats];
        for (int i = 0; i < kt::kFxCount; ++i) fam[kt::kFx[i].family].addItem (1000 + i, kt::kFx[i].name);
        for (int i = 0; i < kv::kMachineCount; ++i) cats[kv::kMachines[i].cat].addItem (500 + i, juce::String (kv::kMachines[i].name) + "   -   " + kv::kMachines[i].era);
        for (int c = 0; c < kv::NumMachineCats; ++c) machines.addSubMenu (kv::kMachineCatNames[c], cats[c]);
        m.addSectionHeader ("PATCH BAY DESIGNER");
        m.addSubMenu ("Add machine", machines);
        for (int f = 0; f < kt::kFxFamilyCount; ++f) m.addSubMenu ("Add " + juce::String (kt::kFxFamilyNames[f]), fam[f]);
        m.addSeparator();
        m.addItem (1, "Auto-chain left to right");
        m.addItem (2, "Show every module in Viewer");
        m.addItem (3, "Reset all faceplates");
        m.addItem (4, "Clear rack");
        juce::Component::SafePointer<RackCanvas> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp, at] (int r) {
            if (sp == nullptr || r == 0) return;
            if (r >= 1000) sp->addFx (r - 1000, at);
            else if (r >= 500) sp->addMachine (r - 500, at);
            else if (r == 1) { sp->rack.autoChain(); sp->changed(); }
            else if (r == 2) { for (auto& n : sp->rack.nodes) n.hidden = false; sp->design(); }
            else if (r == 3) { for (auto& n : sp->rack.nodes) { n.colour = 0; n.face = 0; } sp->design(); }
            else if (r == 4) { sp->rack.clear(); sp->changed(); sp->select (kv::kIn); }
        });
    }
};
