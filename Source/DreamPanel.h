#pragma once
#include "Ui.h"
#include "DreamApi.h"
#include "PluginProcessor.h"

// DreamShare + Discord console. One generic feed view drives every DreamAPI channel:
// lounge chat, threads, module catalog (publish/load/moderate), cloud presets, friends + DMs, Discord bridge.
class DreamPanel : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    enum Ch { Lounge, Threads, Catalog, Mine, Pending, Presets, Friends, Discord, NumCh };
    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onPatchLoaded;

    explicit DreamPanel (KyotoProcessor& p) : proc (p)
    {
        for (auto* t : { &user, &pass, &input }) { addAndMakeVisible (*t); t->setFont (ui::hud (15.0f, false)); }
        user.setTextToShowWhenEmpty ("callsign", ui::muted());
        pass.setTextToShowWhenEmpty ("passcode", ui::muted()); pass.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        input.setTextToShowWhenEmpty ("transmit...", ui::muted());
        input.onReturnKey = [this] { primary(); };
        pass.onReturnKey = [this] { doLogin(); };
        addAndMakeVisible (loginBtn); loginBtn.onClick = [this] { doLogin(); };
        const char* names[] = { "LOUNGE", "THREADS", "CATALOG", "MY MODULES", "PENDING", "CLOUD PRESETS", "FRIENDS / DM", "DISCORD" };
        for (int i = 0; i < NumCh; ++i)
        {
            auto* b = chBtns.add (new juce::TextButton (names[i]));
            b->getProperties().set ("role", i);
            b->onClick = [this, i] { setChannel (i); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &sendBtn, &actA, &actB, &actC, &refreshBtn }) addAndMakeVisible (*b);
        sendBtn.onClick = [this] { primary(); };
        refreshBtn.onClick = [this] { refresh(); };
        actA.onClick = [this] { action (0); };
        actB.onClick = [this] { action (1); };
        actC.onClick = [this] { action (2); };
        addChildComponent (discordCh);
        discordCh.onChange = [this] { const int i = discordCh.getSelectedItemIndex(); peer = i >= 0 ? discordChannels[i]["id"].toString() : juce::String(); refresh(); };
        list.setModel (this); list.setRowHeight (46); addAndMakeVisible (list);
        setChannel (Lounge);
        startTimer (8000);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (34);
        loginBtn.setBounds (top.removeFromRight (120)); top.removeFromRight (8);
        pass.setBounds (top.removeFromRight (170)); top.removeFromRight (8);
        user.setBounds (top.removeFromRight (170));
        r.removeFromTop (12);
        auto side = r.removeFromLeft (170);
        for (auto* b : chBtns) { b->setBounds (side.removeFromTop (34)); side.removeFromTop (6); }
        r.removeFromLeft (14);
        auto head = r.removeFromTop (34);
        refreshBtn.setBounds (head.removeFromRight (100)); head.removeFromRight (6);
        for (auto* b : { &actC, &actB, &actA }) if (b->isVisible()) { b->setBounds (head.removeFromRight (120)); head.removeFromRight (6); }
        discordCh.setBounds (head.removeFromRight (220));
        r.removeFromTop (8);
        auto bottom = r.removeFromBottom (36);
        sendBtn.setBounds (bottom.removeFromRight (130)); bottom.removeFromRight (8);
        input.setBounds (bottom);
        r.removeFromBottom (8);
        listArea = r;
        list.setBounds (r.reduced (6));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = listArea.toFloat().expanded (2.0f);
        g.setColour (ui::bg().darker (0.3f)); g.fillRoundedRectangle (r, 6.0f);
        ui::cornerBrackets (g, r, ui::accent().withAlpha (0.5f));
        g.setColour (ui::accent()); g.setFont (ui::hud (18.0f));
        g.drawText (chBtns[channel]->getButtonText() + (channel == Friends && peer.isNotEmpty() ? "  //  " + peer.toUpperCase() : ""),
                    listArea.getX(), listArea.getY() - 42, 420, 34, juce::Justification::centredLeft);
        const bool on = proc.sessionToken.isNotEmpty();
        g.setColour (on ? ui::accent() : ui::muted()); g.setFont (ui::hud (12.0f));
        g.drawText (on ? "LINK ESTABLISHED  //  " + proc.sessionUser.toUpperCase() + "  [" + proc.sessionRole.toUpperCase() + "]" : "NO SUBSPACE LINK  //  SIGN IN TO DREAMSHARE",
                    12, 12, 460, 34, juce::Justification::centredLeft);
        if (rows.empty()) { g.setColour (ui::muted()); g.setFont (ui::hud (14.0f, false)); g.drawText (busy ? "RECEIVING..." : "NO TRANSMISSIONS", listArea, juce::Justification::centred); }
    }

private:
    struct Row { juce::String id, who, text, meta; juce::var raw; };
    KyotoProcessor& proc;
    juce::TextEditor user, pass, input;
    juce::TextButton loginBtn { "SIGN IN" }, sendBtn { "SEND" }, actA, actB, actC, refreshBtn { "REFRESH" };
    juce::OwnedArray<juce::TextButton> chBtns;
    juce::ComboBox discordCh;
    juce::ListBox list;
    juce::Rectangle<int> listArea;
    std::vector<Row> rows;
    int channel = Lounge, ticks = 0;
    juce::String peer;
    bool busy = false;
    juce::Array<juce::var> discordChannels;

    juce::String tok() const { return proc.sessionToken; }
    void status (const juce::String& s) { if (onStatus) onStatus (s); }

    template <typename Fn, typename Done>
    void net (Fn fn, Done done)
    {
        busy = true; repaint();
        juce::Component::SafePointer<DreamPanel> sp (this);
        juce::Thread::launch ([sp, fn, done] {
            auto r = fn();
            juce::MessageManager::callAsync ([sp, r, done] { if (sp != nullptr) { sp->busy = false; done (r); sp->repaint(); } });
        });
    }

    static juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> kv)
    {
        auto* o = new juce::DynamicObject();
        for (auto& p : kv) o->setProperty (p.first, p.second);
        return juce::var (o);
    }

    void doLogin()
    {
        if (user.isEmpty() || pass.isEmpty()) { status ("Enter callsign and passcode"); return; }
        auto u = user.getText(), pw = pass.getText();
        net ([u, pw] { return kt::login (u, pw); }, [this] (const kt::DreamResult& r) {
            if (! r.ok) { status ("Sign-in failed: " + r.error); return; }
            proc.sessionToken = r.token; proc.sessionUser = r.user; proc.sessionRole = r.role;
            pass.clear(); status ("Signed in as " + r.user);
            loadDiscordChannels(); refresh();
        });
    }

    void setChannel (int c)
    {
        channel = c; peer = {}; rows.clear(); list.updateContent();
        for (int i = 0; i < chBtns.size(); ++i) chBtns[i]->setToggleState (i == c, juce::dontSendNotification);
        static const char* labels[NumCh][4] = {
            { "SEND", "", "", "" }, { "NEW THREAD", "REPLY", "REACT +1", "DELETE" },
            { "PUBLISH", "LOAD", "TAG", "UNTAG" }, { "PUBLISH", "LOAD", "DELETE", "" },
            { "", "APPROVE", "DENY", "LOAD" }, { "SAVE", "LOAD", "DELETE", "" },
            { "SEND DM", "ADD FRIEND", "ACCEPT", "REMOVE" }, { "SEND", "", "", "" } };
        sendBtn.setButtonText (labels[c][0]); sendBtn.setVisible (labels[c][0][0] != 0);
        input.setVisible (sendBtn.isVisible() || c == Catalog || c == Friends);
        juce::TextButton* acts[] = { &actA, &actB, &actC };
        for (int i = 0; i < 3; ++i) { acts[i]->setButtonText (labels[c][i + 1]); acts[i]->setVisible (labels[c][i + 1][0] != 0); }
        discordCh.setVisible (c == Discord);
        if (c == Discord && discordCh.getNumItems() > 0) { discordCh.setSelectedItemIndex (0, juce::dontSendNotification); peer = discordChannels[0]["id"].toString(); }
        resized(); refresh();
    }

    const Row* sel() const { const int i = list.getSelectedRow(); return i >= 0 && i < (int) rows.size() ? &rows[(size_t) i] : nullptr; }

    void refresh()
    {
        if (tok().isEmpty()) { repaint(); return; }
        const auto t = tok(); const auto pr = peer; const int c = channel;
        net ([t, pr, c]() -> kt::DreamResult {
            switch (c)
            {
                case Lounge:  return kt::postAction ("chat_list", {}, t);
                case Threads: return kt::getThreads (t);
                case Catalog: return kt::getCatalog (t);
                case Mine:    return kt::getMyModules (t);
                case Pending: return kt::getPendingModules (t);
                case Presets: return kt::postAction ("preset_list", obj ({ { "machine", "kyotovst" } }), t);
                case Friends: return pr.isNotEmpty() ? kt::getDM (t, pr) : kt::postAction ("friends_list", {}, t);
                default:      return kt::getDiscordMessages (t, pr);
            }
        }, [this, c] (const kt::DreamResult& r) { if (c == channel) fill (r); });
    }

    void fill (const kt::DreamResult& r)
    {
        if (! r.ok) { status (r.error); return; }
        rows.clear();
        if (channel == Friends && peer.isEmpty())
        {
            for (auto* k : { "friends", "incoming" })
                if (auto* a = r.parsed[k].getArray()) for (auto& v : *a) rows.push_back (toRow (v, juce::String (k) == "incoming"));
        }
        else
            for (auto* k : { "messages", "chat", "threads", "modules", "presets" })
                if (auto* a = r.parsed[k].getArray()) { for (auto& v : *a) rows.push_back (toRow (v, false)); break; }
        list.updateContent();
        if ((channel == Lounge || channel == Discord || channel == Friends) && ! rows.empty()) list.scrollToEnsureRowIsOnscreen ((int) rows.size() - 1);
        repaint();
    }

    static juce::String pick (const juce::var& v, std::initializer_list<const char*> ks)
    {
        if (! v.isObject()) return v.toString();
        for (auto* k : ks) { auto s = v[k].toString(); if (s.isNotEmpty()) return s; }
        return {};
    }

    static Row toRow (const juce::var& v, bool incoming)
    {
        Row r; r.raw = v;
        r.id = pick (v, { "id", "name", "user" });
        r.who = pick (v, { "author", "user", "from", "username", "name" });
        r.text = incoming ? juce::String ("incoming friend request") : pick (v, { "title", "text", "content", "description", "message", "body" });
        if (auto* tags = v["tags"].getArray()) { juce::StringArray s; for (auto& t : *tags) s.add ("#" + t.toString()); r.meta = s.joinIntoString (" "); }
        if (v["status"].toString().isNotEmpty()) r.meta = v["status"].toString().toUpperCase() + "  " + r.meta;
        if (auto* cm = v["comments"].getArray()) r.meta << "  " << cm->size() << " REPLIES";
        return r;
    }

    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int i, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (i < 0 || i >= (int) rows.size()) return;
        auto& row = rows[(size_t) i];
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 3.0f);
        g.setColour (selected ? ui::accent().withAlpha (0.18f) : ui::panel().withAlpha (0.55f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (selected ? ui::accent() : ui::family (i % 8).withAlpha (0.8f)); g.fillRect (r.removeFromLeft (4.0f));
        r.removeFromLeft (8.0f);
        auto top = r.removeFromTop (18.0f);
        g.setColour (ui::accent()); g.setFont (ui::hud (13.0f));
        g.drawText (row.who.toUpperCase(), top.removeFromLeft (240.0f), juce::Justification::centredLeft, true);
        g.setColour (ui::muted()); g.setFont (ui::hud (11.0f, false));
        g.drawText (row.meta, top.withTrimmedRight (8.0f), juce::Justification::centredRight, true);
        g.setColour (ui::text()); g.setFont (ui::hud (14.0f, false));
        g.drawText (row.text, r.withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override
    {
        if (channel == Friends && peer.isEmpty() && sel()) { peer = sel()->who.isNotEmpty() ? sel()->who : sel()->id; rows.clear(); refresh(); }
        else if (channel == Catalog || channel == Mine || channel == Presets) action (0);
        else if (channel == Pending) action (2);
    }

    void timerCallback() override
    {
        if (tok().isEmpty()) return;
        if (++ticks % 8 == 0) { const auto t = tok(); juce::Thread::launch ([t] { kt::postAction ("heartbeat", {}, t); }); }
        if (isShowing() && ! busy && (channel == Lounge || channel == Discord || (channel == Friends && peer.isNotEmpty()))) refresh();
    }

    void loadDiscordChannels()
    {
        const auto t = tok();
        net ([t] { return kt::getDiscordChannels (t); }, [this] (const kt::DreamResult& r) {
            discordCh.clear (juce::dontSendNotification); discordChannels.clear();
            if (auto* a = r.parsed["channels"].getArray())
                for (auto& c : *a) { discordChannels.add (c); discordCh.addItem ("#" + c["name"].toString() + "  " + c["server"].toString(), discordCh.getNumItems() + 1); }
        });
    }

    void done (const kt::DreamResult& r, const juce::String& okMsg) { status (r.ok ? okMsg : "DreamShare: " + r.error); if (r.ok) refresh(); }

    juce::var modulePayload (const juce::String& name) const
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("format", "kyoteppah-module-1"); m->setProperty ("face", "chain");
        m->setProperty ("name", name); m->setProperty ("theme", proc.themeId);
        m->setProperty ("tags", juce::Array<juce::var> { "kyotovst" });
        m->setProperty ("machineDesign", proc.rack.toVar());
        return juce::var (m);
    }

    void loadModule (const juce::String& id)
    {
        const auto t = tok();
        net ([t, id] { return kt::getModule (t, id); }, [this] (const kt::DreamResult& r) {
            auto m = r.parsed["module"];
            if (r.ok && proc.loadPatchJson (juce::JSON::toString (m.isObject() ? m : r.parsed))) { status ("Module loaded into rack"); if (onPatchLoaded) onPatchLoaded(); }
            else status (r.ok ? "Module has no KyotoVST patch" : r.error);
        });
    }

    void primary()
    {
        if (tok().isEmpty()) { status ("Sign in first"); return; }
        const auto t = tok(), text = input.getText().trim(), pr = peer, u = proc.sessionUser;
        if (text.isEmpty() && channel != Presets) { status ("Type a name or message first"); return; }
        input.clear();
        switch (channel)
        {
            case Lounge:  net ([=] { return kt::sendChat (t, text); }, [this] (auto r) { done (r, "Sent"); }); break;
            case Threads: net ([=] { return kt::postAction ("create_thread", obj ({ { "title", text }, { "text", text }, { "type", "text" } }), t); }, [this] (auto r) { done (r, "Thread posted"); }); break;
            case Catalog: case Mine:
            {
                const auto body = juce::JSON::toString (modulePayload (text));
                net ([=] { return kt::publishModule (t, text, body); }, [this] (auto r) { done (r, "Published - pending approval"); });
                break;
            }
            case Presets:
            {
                const auto name = text.isNotEmpty() ? text : "KyotoVST " + juce::Time::getCurrentTime().formatted ("%H%M%S");
                const auto state = proc.patchJson();
                net ([=] { return kt::postAction ("preset_save", obj ({ { "machine", "kyotovst" }, { "name", name }, { "state", state } }), t); }, [this] (auto r) { done (r, "Preset saved to cloud"); });
                break;
            }
            case Friends:
                if (pr.isEmpty()) { status ("Double-click a friend to open DMs"); return; }
                net ([=] { return kt::sendDM (t, pr, text); }, [this] (auto r) { done (r, "DM sent"); }); break;
            default: net ([=] { return kt::sendDiscordMessage (t, u, text, pr); }, [this] (auto r) { done (r, "Sent to Discord"); }); break;
        }
    }

    void action (int slot)
    {
        if (tok().isEmpty()) { status ("Sign in first"); return; }
        const auto t = tok(); const auto* s = sel();
        const auto text = input.getText().trim();
        if (channel == Friends && slot == 0) { net ([=] { return kt::friendRequest (t, "friend_request", text); }, [this] (auto r) { done (r, "Friend request sent"); }); return; }
        if (s == nullptr) { status ("Select an entry first"); return; }
        const auto id = s->id, who = s->who;
        switch (channel * 4 + slot)
        {
            case Threads * 4 + 0: net ([=] { return kt::postAction ("comment", obj ({ { "threadId", id }, { "text", text } }), t); }, [this] (auto r) { input.clear(); done (r, "Reply posted"); }); break;
            case Threads * 4 + 1: net ([=] { return kt::react (t, "thread", id, "+1"); }, [this] (auto r) { done (r, "Reacted"); }); break;
            case Threads * 4 + 2: net ([=] { return kt::postAction ("delete_thread", obj ({ { "id", id }, { "threadId", id } }), t); }, [this] (auto r) { done (r, "Thread deleted"); }); break;
            case Catalog * 4 + 0: case Mine * 4 + 0: case Pending * 4 + 2: loadModule (id); break;
            case Catalog * 4 + 1: net ([=] { return kt::tagModule (t, id, text); }, [this] (auto r) { done (r, "Tagged"); }); break;
            case Catalog * 4 + 2: net ([=] { return kt::postAction ("module_untag", obj ({ { "id", id }, { "tag", text } }), t); }, [this] (auto r) { done (r, "Tag removed"); }); break;
            case Mine * 4 + 1: net ([=] { return kt::deleteModule (t, id); }, [this] (auto r) { done (r, "Module deleted"); }); break;
            case Pending * 4 + 0: net ([=] { return kt::approveModule (t, id); }, [this] (auto r) { done (r, "Approved"); }); break;
            case Pending * 4 + 1: net ([=] { return kt::denyModule (t, id); }, [this] (auto r) { done (r, "Denied"); }); break;
            case Presets * 4 + 0:
                if (proc.loadPatchJson (s->raw["state"].toString())) { status ("Preset loaded"); if (onPatchLoaded) onPatchLoaded(); } else status ("Preset is not a KyotoVST patch");
                break;
            case Presets * 4 + 1: net ([=] { return kt::postAction ("preset_delete", obj ({ { "machine", "kyotovst" }, { "name", id } }), t); }, [this] (auto r) { done (r, "Preset deleted"); }); break;
            case Friends * 4 + 1: net ([=] { return kt::friendRequest (t, "friend_accept", who); }, [this] (auto r) { done (r, "Friend accepted"); }); break;
            case Friends * 4 + 2: net ([=] { return kt::friendRequest (t, "friend_remove", who); }, [this] (auto r) { done (r, "Friend removed"); }); break;
            default: break;
        }
    }
};
