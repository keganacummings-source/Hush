#include "PluginEditor.h"

// ================= Library =================
Library::Library()
{
    search.setTextToShowWhenEmpty ("search " + juce::String (kt::kFxCount) + " fx + " + juce::String (kv::kMachineCount) + " machines", ui::muted());
    search.setFont (ui::hud (14.0f, false));
    search.onTextChange = [this] { rebuild(); };
    filter.addItem ("ALL UNITS", 1);
    filter.addItem ("ALL MACHINES (" + juce::String (kv::kMachineCount) + ")", 2);
    for (int c = 0; c < kv::NumMachineCats; ++c) filter.addItem ("MACHINES: " + juce::String (kv::kMachineCatNames[c]).toUpperCase(), 30 + c);
    for (int f = 0; f < kt::kFxFamilyCount; ++f) filter.addItem (juce::String (kt::kFxFamilyNames[f]).toUpperCase(), 10 + f);
    filter.setSelectedId (1, juce::dontSendNotification);
    filter.onChange = [this] { rebuild(); };
    list.setModel (this); list.setRowHeight (30);
    for (auto* c : std::initializer_list<juce::Component*> { &search, &filter, &list }) addAndMakeVisible (c);
    rebuild();
}

void Library::rebuild()
{
    items.clear();
    const auto q = search.getText().trim().toLowerCase();
    const int sel = filter.getSelectedId();
    auto match = [&q] (const juce::String& s) { return q.isEmpty() || s.toLowerCase().contains (q); };
    const bool machineCat = sel >= 30;
    if (sel <= 2 || machineCat)
        for (int i = 0; i < kv::kMachineCount; ++i)
            if ((! machineCat || kv::kMachines[i].cat == sel - 30) && (match (kv::kMachines[i].name) || match (kv::kMachines[i].era) || match (kv::kMachineCatNames[kv::kMachines[i].cat])))
                items.push_back ({ true, i });
    if (sel == 1 || (sel >= 10 && sel < 30))
        for (int i = 0; i < kt::kFxCount; ++i)
            if ((sel == 1 || sel - 10 == kt::kFx[i].family) && match (kt::kFx[i].name)) items.push_back ({ false, i });
    list.updateContent(); repaint();
}

void Library::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (26);
    search.setBounds (r.removeFromTop (30)); r.removeFromTop (6);
    filter.setBounds (r.removeFromTop (28)); r.removeFromTop (8);
    list.setBounds (r);
}

void Library::paint (juce::Graphics& g)
{
    ui::bezel (g, getLocalBounds().toFloat(), 8.0f);
    g.setColour (ui::accent()); g.setFont (ui::hud (13.0f));
    g.drawText ("UNIT LIBRARY  //  " + juce::String ((int) items.size()), 12, 8, getWidth() - 24, 18, juce::Justification::left);
}

void Library::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) items.size()) return;
    const auto it = items[(size_t) row];
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f, 2.0f);
    if (selected) { g.setColour (ui::accent().withAlpha (0.18f)); g.fillRoundedRectangle (r, 3.0f); }
    ui::pill (g, r.removeFromLeft (16.0f).reduced (2.0f, 6.0f), it.machine ? ui::hot() : ui::family (kt::kFx[it.index].family));
    r.removeFromLeft (8.0f);
    g.setColour (ui::text()); g.setFont (ui::hud (13.5f, it.machine));
    g.drawText (it.machine ? juce::String (kv::kMachines[it.index].name).toUpperCase() : juce::String (kt::kFx[it.index].name),
                r.removeFromLeft (r.getWidth() * 0.62f), juce::Justification::centredLeft, true);
    g.setColour (ui::muted()); g.setFont (ui::hud (10.5f, false));
    g.drawText (it.machine ? juce::String (kv::kMachines[it.index].era) : juce::String (kt::fxFamilyName (kt::kFx[it.index].family)).toUpperCase(),
                r.withTrimmedRight (4.0f), juce::Justification::centredRight, true);
}

void Library::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) items.size() && onPick) onPick (items[(size_t) row].machine, items[(size_t) row].index);
}

juce::var Library::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || rows[0] >= (int) items.size()) return {};
    const auto it = items[(size_t) rows[0]];
    return (it.machine ? "machine:" : "fx:") + juce::String (it.index);
}

// ================= Inspector =================
Inspector::Inspector (kv::Rack& r) : rack (r)
{
    const char* caps[] = { "AMOUNT", "TONE", "MOTION", "SHAPE", "MIX", "LEVEL", "MACRO DEPTH" };
    for (int i = 0; i < 7; ++i)
    {
        auto* k = knobs.add (new ui::Knob (caps[i]));
        if (i == 5) k->slider.setRange (0.0, 2.0);
        k->onChange = [this, i] (float v) {
            if (rack.find (node) == nullptr) return;
            auto& p = rack.paramsFor (node);
            if (i < 4) p.p[i] = v; else if (i == 4) p.mix = v; else if (i == 5) p.level = v; else p.depth = v;
            if (onEdited) onEdited();
        };
        addAndMakeVisible (k);
    }
    macro.addItem ("NO MACRO", 1);
    for (int m = 0; m < kv::kMacros; ++m) macro.addItem ("AMOUNT <- MACRO " + juce::String (m + 1), m + 2);
    macro.onChange = [this] { if (rack.find (node)) rack.paramsFor (node).macro = macro.getSelectedId() - 2; };
    bypass.setClickingTogglesState (true);
    bypass.onClick = [this] { if (rack.find (node)) { rack.paramsFor (node).bypass = bypass.getToggleState(); if (onEdited) onEdited(); } };
        remove.onClick = [this] { if (onRemove) onRemove(); };
    for (auto* c : std::initializer_list<juce::Component*> { &macro, &bypass, &remove }) addAndMakeVisible (c);
    setNode (kv::kIn);
}

void Inspector::setNode (int id)
{
    node = id;
    auto* n = rack.find (id);
    for (auto* c : getChildren()) c->setVisible (n != nullptr);
    if (n != nullptr)
    {
        auto& p = rack.paramsFor (id);
        const float v[] = { p.p[0].load(), p.p[1].load(), p.p[2].load(), p.p[3].load(), p.mix.load(), p.level.load(), p.depth.load() };
        for (int i = 0; i < 7; ++i)
        {
            knobs[i]->slider.setValue (v[i], juce::dontSendNotification);
            knobs[i]->slider.getProperties().set ("face", n->face);
            knobs[i]->slider.getProperties().set ("tint", (juce::int64) RackCanvas::nodeColour (*n).getARGB());
            knobs[i]->repaint();
        }
        macro.setSelectedId (p.macro.load() + 2, juce::dontSendNotification);
        bypass.setToggleState (p.bypass.load(), juce::dontSendNotification);
    }
    repaint();
}

void Inspector::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (58);
    const int kh = juce::jlimit (70, 100, (r.getHeight() - 90) / 4);
    for (int row = 0; row < 3; ++row)
    {
        auto line = r.removeFromTop (kh);
        const int w = line.getWidth() / 2;
        for (int c = 0; c < 2; ++c) knobs[row * 2 + c]->setBounds (line.removeFromLeft (w));
    }
    knobs[6]->setBounds (r.removeFromTop (kh).withSizeKeepingCentre (r.getWidth() / 2, kh));
    r.removeFromTop (6);
    macro.setBounds (r.removeFromTop (28)); r.removeFromTop (10);
    auto b = r.removeFromTop (30);
    bypass.setBounds (b.removeFromLeft (b.getWidth() / 2 - 4)); b.removeFromLeft (8);
    remove.setBounds (b);
}

void Inspector::paint (juce::Graphics& g)
{
    ui::bezel (g, getLocalBounds().toFloat(), 8.0f);
    g.setColour (ui::accent()); g.setFont (ui::hud (13.0f));
    g.drawText ("MODULE INSPECTOR", 12, 8, getWidth() - 24, 18, juce::Justification::left);
    if (auto* n = rack.find (node))
    {
        ui::pill (g, { 12.0f, 32.0f, 8.0f, 22.0f }, RackCanvas::nodeColour (*n));
        g.setColour (ui::text()); g.setFont (ui::hud (18.0f));
        g.drawText ((n->label.isNotEmpty() ? n->label : juce::String (kt::kFx[n->fx].name)).toUpperCase(), 28, 30, getWidth() - 40, 26, juce::Justification::left, true);
    }
    else
    {
        g.setColour (ui::muted()); g.setFont (ui::hud (13.0f, false));
        g.drawFittedText ("SELECT A MODULE\n\nDouble-click or drag units from the library.\nDrag right jack -> left jack to patch.\nRight-click modules, cables or empty space for the designer.\nDEL removes the selection.",
                          getLocalBounds().reduced (16).withTrimmedTop (40), juce::Justification::topLeft, 10);
    }
}

// ================= Editor =================
KyotoEditor::KyotoEditor (KyotoProcessor& p)
    : AudioProcessorEditor (p), proc (p), canvas (p.rack), viewer (p.rack), inspector (p.rack), dream (p)
{
    setLookAndFeel (&lnf);
    canvas.onPatchChanged = [this] { proc.rack.commit(); };
    canvas.onDesignChanged = [this] { inspector.setNode (canvas.selected); };
    canvas.onSelect = [this] (int id) { inspector.setNode (id); };
    inspector.onEdited = [this] { canvas.repaint(); };
    inspector.onRemove = [this] { proc.rack.removeNode (canvas.selected); canvas.changed(); canvas.select (kv::kIn); };
    library.onPick = [this] (bool m, int i) { if (m) canvas.addMachine (i, canvas.addPoint()); else canvas.addFx (i, canvas.addPoint()); };
    dream.onStatus = [this] (const juce::String& s) { setStatus (s); };
    dream.onPatchLoaded = [this] { patchLoaded(); showTab (Bay); };

    for (int i = 0; i < kt::kThemeCount; ++i) theme.addItem (juce::String (kt::kThemes[i].name).toUpperCase(), i + 1);
    theme.onChange = [this] { applyTheme (kt::kThemes[theme.getSelectedItemIndex()].id); };

    const char* ids[] = { "in", "mix", "out", "macro1", "macro2", "macro3", "macro4" };
    const char* caps[] = { "INPUT", "MIX", "OUTPUT", "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" };
    for (int i = 0; i < 7; ++i)
    {
        auto* k = console.add (new ui::Knob (caps[i]));
        attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, ids[i], k->slider));
        if (i >= 3) k->slider.getProperties().set ("face", 1);
        addAndMakeVisible (k);
    }
    tabBay.onClick = [this] { showTab (Bay); };
    tabView.onClick = [this] { showTab (tab == View ? Bay : View); };
    tabShare.onClick = [this] { showTab (Share); };
    autoChain.onClick = [this] { proc.rack.autoChain(); canvas.changed(); setStatus ("Modules chained left to right"); };
    clear.onClick = [this] { proc.rack.clear(); canvas.changed(); canvas.select (kv::kIn); setStatus ("Rack cleared"); };
    save.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Export patch", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.kyoto");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (f == juce::File()) return;
            f = f.withFileExtension ("kyoto"); f.replaceWithText (proc.patchJson()); setStatus ("Exported " + f.getFileName()); });
    };
    load.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Import patch", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.kyoto;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult(); if (! f.existsAsFile()) return;
            setStatus (proc.loadPatchJson (f.loadFileAsString()) ? "Imported " + f.getFileName() : "Not a KyotoVST patch");
            patchLoaded(); });
    };
    for (auto* c : std::initializer_list<juce::Component*> { &canvas, &library, &inspector, &tabBay, &tabView, &tabShare, &autoChain, &clear, &save, &load, &theme })
        addAndMakeVisible (c);
    addChildComponent (dream); addChildComponent (viewer);
    for (int i = 0; i < kt::kThemeCount; ++i) if (proc.themeId == kt::kThemes[i].id) theme.setSelectedItemIndex (i, juce::dontSendNotification);
    applyTheme (proc.themeId);

    setResizable (true, true);
    setResizeLimits (1100, 700, 2400, 1500);
    setSize (1320, 820);
    showTab (proc.viewerMode ? View : Bay);
    startTimerHz (20);
}

KyotoEditor::~KyotoEditor() { setLookAndFeel (nullptr); }

void KyotoEditor::applyTheme (const juce::String& id)
{
    ui::palPtr() = &kt::themeById (id);
    proc.themeId = ui::pal().id;
    lnf.refresh();
    for (auto* b : { &tabBay, &tabView, &tabShare }) b->getProperties().set ("role", 1);
    sendLookAndFeelChange();
    if (tab == View) viewer.rebuild();
    inspector.setNode (canvas.selected);
    repaint();
}

void KyotoEditor::showTab (int t)
{
    tab = t;
    proc.viewerMode = t == View;
    tabBay.setToggleState (t == Bay, juce::dontSendNotification);
    tabView.setToggleState (t == View, juce::dontSendNotification);
    tabShare.setToggleState (t == Share, juce::dontSendNotification);
    tabView.setButtonText (t == View ? "EXIT VIEWER" : "VIEWER");
    for (auto* c : std::initializer_list<juce::Component*> { &canvas, &library, &inspector, &autoChain, &clear, &save, &load }) c->setVisible (t == Bay);
    tabBay.setVisible (t != View); tabShare.setVisible (t != View); theme.setVisible (t != View);
    dream.setVisible (t == Share);
    viewer.setVisible (t == View);
    if (t == View) viewer.rebuild();
    if (t == Bay) inspector.setNode (canvas.selected);
    resized(); repaint();
}

void KyotoEditor::patchLoaded()
{
    for (auto& n : proc.rack.nodes) { n.x = juce::jmin (n.x, (float) canvas.getWidth() - 240.0f); n.y = juce::jmin (n.y, (float) canvas.getHeight() - 100.0f); }
    canvas.select (kv::kIn); canvas.repaint();
    if (tab == View) viewer.rebuild();
}

void KyotoEditor::timerCallback()
{
    mIn = juce::jmax (proc.meterIn.load(), mIn * 0.85f);
    mOut = juce::jmax (proc.meterOut.load(), mOut * 0.85f);
    repaint (getLocalBounds().removeFromBottom (120));
}

void KyotoEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg());
    auto head = getLocalBounds().toFloat().removeFromTop (58.0f).reduced (10.0f, 8.0f);
    auto logo = head.removeFromLeft (230.0f);
    ui::pill (g, logo, ui::accent());
    g.setColour (ui::bg()); g.setFont (ui::hud (22.0f));
    g.drawText ("KYOTOVST", logo.withTrimmedLeft (18.0f).withTrimmedBottom (12.0f), juce::Justification::centredLeft);
    g.setFont (ui::hud (9.5f, false));
    g.drawText (tab == View ? "MACHINE VIEW" : "MODULAR FX BUILDER", logo.withTrimmedLeft (19.0f).withTrimmedTop (24.0f), juce::Justification::centredLeft);
    auto rail = head.withTrimmedLeft (tab == View ? 150.0f : 430.0f).withTrimmedRight (tab == View ? 10.0f : 230.0f);
    auto bar = rail.withHeight (10.0f).withY (rail.getY() + 4.0f);
    const float seg = bar.getWidth() / 6.0f;
    for (int i = 0; i < 6; ++i) ui::pill (g, bar.removeFromLeft (seg).reduced (3.0f, 0), i % 2 ? ui::muted() : ui::family (i));
    g.setColour (ui::muted()); g.setFont (ui::hud (11.0f));
    g.drawText ("STATUS  //  " + status, rail.withTrimmedTop (20.0f), juce::Justification::centredLeft, true);

    auto foot = getLocalBounds().removeFromBottom (120).reduced (10).toFloat();
    ui::bezel (g, foot, 8.0f);
    auto meters = foot.removeFromRight (70.0f).reduced (14.0f, 16.0f);
    for (float v : { mIn, mOut })
    {
        auto m = meters.removeFromLeft (14.0f); meters.removeFromLeft (8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (m, 3.0f);
        const float lvl = juce::jlimit (0.0f, 1.0f, v);
        g.setColour (lvl > 0.95f ? ui::hot() : ui::accent());
        g.fillRoundedRectangle (m.withTrimmedTop (m.getHeight() * (1.0f - lvl)), 3.0f);
    }
}

void KyotoEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (58).reduced (10, 10);
    head.removeFromLeft (240);
    if (tab == View) tabView.setBounds (head.removeFromLeft (140));
    else
    {
        tabBay.setBounds (head.removeFromLeft (120)); head.removeFromLeft (6);
        tabView.setBounds (head.removeFromLeft (120)); head.removeFromLeft (6);
        tabShare.setBounds (head.removeFromLeft (130));
        theme.setBounds (head.removeFromRight (210).reduced (0, 4));
    }

    auto foot = r.removeFromBottom (120).reduced (10);
    foot.removeFromRight (70);
    if (tab == Bay)
    {
        auto btns = foot.removeFromLeft (300).reduced (14, 18);
        const int bw = btns.getWidth() / 2 - 4;
        auto row1 = btns.removeFromTop (btns.getHeight() / 2 - 3); btns.removeFromTop (6);
        autoChain.setBounds (row1.removeFromLeft (bw)); clear.setBounds (row1.removeFromRight (bw));
        save.setBounds (btns.removeFromLeft (bw)); load.setBounds (btns.removeFromRight (bw));
    }
    else foot.removeFromLeft (20);
    const int kw = juce::jmin (100, foot.getWidth() / 7);
    for (auto* k : console) k->setBounds (foot.removeFromLeft (kw).reduced (4, 6));

    auto body = r.reduced (10, 0);
    dream.setBounds (body);
    viewer.setBounds (body);
    library.setBounds (body.removeFromLeft (juce::jlimit (240, 320, getWidth() / 5))); body.removeFromLeft (10);
    inspector.setBounds (body.removeFromRight (250)); body.removeFromRight (10);
    canvas.setBounds (body);
}
