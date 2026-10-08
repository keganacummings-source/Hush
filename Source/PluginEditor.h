#pragma once
#include "PluginProcessor.h"
#include "RackCanvas.h"
#include "Viewer.h"
#include "DreamPanel.h"

// Searchable list of the 200 FX and the machine library; rows drag straight onto the patch bay.
class Library : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void (int kind, int index)> onPick;   // kind: 0 fx, 1 machine, 2 widget
    Library();
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    struct Item { int kind; int index; };
    std::vector<Item> items;
    juce::TextEditor search;
    juce::ComboBox filter;
    juce::ListBox list;
    void rebuild();
    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
};

class Inspector : public juce::Component
{
public:
    std::function<void()> onEdited, onRemove;
    explicit Inspector (kv::Rack& r);
    void setNode (int id);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    kv::Rack& rack;
    int node = kv::kIn;
    juce::OwnedArray<ui::Knob> knobs;   // 4 params, mix, level, macro depth
    juce::ComboBox macro;
    juce::TextButton bypass { "BYPASS" }, remove { "REMOVE" };
};

class KyotoEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    enum Tab { Bay, View, Share };
    explicit KyotoEditor (KyotoProcessor&);
    ~KyotoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    KyotoProcessor& proc;
    ui::Lnf lnf;
    RackCanvas canvas;
    Viewer viewer;
    Library library;
    Inspector inspector;
    DreamPanel dream;
    juce::TextButton tabBay { "PATCH BAY" }, tabView { "VIEWER" }, tabShare { "DREAMSHARE" },
                     autoChain { "AUTO-CHAIN" }, clear { "CLEAR" }, save { "EXPORT" }, load { "IMPORT" };
    juce::ComboBox theme;
    juce::OwnedArray<ui::Knob> console;   // in, mix, out, macro1-4
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status { "SYSTEMS NOMINAL" };
    float mIn = 0, mOut = 0;
    int tab = Bay;

    void timerCallback() override;
    void showTab (int t);
    void applyTheme (const juce::String& id);
    void patchLoaded();
    void setStatus (const juce::String& s) { status = s.toUpperCase(); repaint(); }
};
