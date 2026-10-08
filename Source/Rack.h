#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "FxCatalog.h"
#include "KtDspEngine.h"

// KyotoVST signal graph. Message thread edits nodes/wires, commit() compiles a Program,
// the audio thread swaps it in via try-lock and keeps DSP state of unchanged nodes.
namespace kv
{
constexpr int kMaxNodes = 64;
constexpr int kMacros = 4;
constexpr int kIn = -1, kOut = -2;
constexpr const char* kPatchFormat = "kyotovst-patch-1";

inline int fxIndexOf (const juce::String& id)
{
    for (int i = 0; i < kt::kFxCount; ++i)
        if (id == kt::kFx[i].id) return i;
    return -1;
}

struct NodeParams
{
    std::atomic<float> p[4];
    std::atomic<float> mix { 1.0f }, level { 1.0f }, depth { 0.5f };
    std::atomic<int> macro { -1 };
    std::atomic<bool> bypass { false };
    std::atomic<float> vu { 0.0f };   // live output level of this node (for the Viewer), audio-thread write / UI read
    NodeParams() { reset(); }
    void reset() { for (auto& v : p) v = 0.5f; mix = 1.0f; level = 1.0f; depth = 0.5f; macro = -1; bypass = false; vu = 0.0f; }
    void copyFrom (const NodeParams& o) { for (int i = 0; i < 4; ++i) p[i] = o.p[i].load(); mix = o.mix.load(); level = o.level.load(); depth = o.depth.load(); macro = o.macro.load(); bypass = o.bypass.load(); }
};

// Designer data lives on the node: label, faceplate colour (0 = family colour), viewer visibility, knob style.
struct Node { int id = 0, fx = 0; float x = 0, y = 0; juce::String label; juce::uint32 colour = 0; bool hidden = false; int face = 0; };
struct Wire { int from = kIn, to = kOut; bool operator== (const Wire& o) const { return from == o.from && to == o.to; } };

class Rack
{
public:
    std::vector<Node> nodes;
    std::vector<Wire> wires;
    std::atomic<float> outVu { 0.0f };   // live master output level for the Viewer

    int addNode (int fx, float x, float y)
    {
        if (fx < 0 || fx >= kt::kFxCount) return -1;
        for (int id = 0; id < kMaxNodes; ++id)
            if (find (id) == nullptr)
            {
                params[(size_t) id].reset();
                Node n; n.id = id; n.fx = fx; n.x = x; n.y = y;
                nodes.push_back (n);
                return id;
            }
        return -1;
    }

    void removeNode (int id)
    {
        nodes.erase (std::remove_if (nodes.begin(), nodes.end(), [id] (auto& n) { return n.id == id; }), nodes.end());
        unwire (id);
    }
    void unwire (int id) { wires.erase (std::remove_if (wires.begin(), wires.end(), [id] (auto& w) { return w.from == id || w.to == id; }), wires.end()); }

    Node* find (int id) { for (auto& n : nodes) if (n.id == id) return &n; return nullptr; }
    NodeParams& paramsFor (int id) { return params[(size_t) juce::jlimit (0, kMaxNodes - 1, id)]; }
    bool connected (int from, int to) const { return std::find (wires.begin(), wires.end(), Wire { from, to }) != wires.end(); }

    // Rejects self-loops, duplicates, wires into IN / out of OUT, and cycles.
    bool connect (int from, int to)
    {
        if (from == to || from == kOut || to == kIn || connected (from, to)) return false;
        if (to != kOut && from != kIn && reaches (to, from)) return false;
        wires.push_back ({ from, to });
        return true;
    }
    void disconnect (int from, int to) { wires.erase (std::remove (wires.begin(), wires.end(), Wire { from, to }), wires.end()); }

    void autoChain()
    {
        wires.clear();
        auto order = nodes;
        std::sort (order.begin(), order.end(), [] (auto& a, auto& b) { return a.x < b.x; });
        int prev = kIn;
        for (auto& n : order) { wires.push_back ({ prev, n.id }); prev = n.id; }
        wires.push_back ({ prev, kOut });
    }

    void clear() { nodes.clear(); wires.clear(); }

    // Machine recipe: branches split by '|', stages by '>', params "fx@amount,tone,motion,shape,mix".
    juce::Array<int> addMachine (const juce::String& recipe, float x, float y)
    {
        juce::Array<int> made, entries, exits;
        auto branches = juce::StringArray::fromTokens (recipe, "|", "");
        for (int b = 0; b < branches.size(); ++b)
        {
            int prev = -999;
            auto stages = juce::StringArray::fromTokens (branches[b].trim(), ">", "");
            for (int s = 0; s < stages.size(); ++s)
            {
                auto spec = stages[s].trim();
                const int id = addNode (fxIndexOf (spec.upToFirstOccurrenceOf ("@", false, false).trim()), x + 190.0f * (float) s, y + 120.0f * (float) b);
                if (id < 0) continue;
                made.add (id);
                if (spec.containsChar ('@'))
                {
                    auto v = juce::StringArray::fromTokens (spec.fromFirstOccurrenceOf ("@", false, false), ",", "");
                    auto& p = paramsFor (id);
                    for (int i = 0; i < v.size() && i < 5; ++i) (i < 4 ? p.p[i] : p.mix) = juce::jlimit (0.0f, 1.0f, v[i].getFloatValue());
                }
                if (prev == -999) entries.add (id); else connect (prev, id);
                prev = id;
            }
            if (prev != -999) exits.add (prev);
        }
        if (std::none_of (wires.begin(), wires.end(), [] (auto& w) { return w.to == kOut; }))
        {
            for (auto e : entries) connect (kIn, e);
            for (auto e : exits) connect (e, kOut);
        }
        return made;
    }

    juce::var toVar() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", kPatchFormat);
        juce::Array<juce::var> ns, ws;
        for (auto& n : nodes)
        {
            auto& p = params[(size_t) n.id];
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", n.id); o->setProperty ("fx", kt::kFx[n.fx].id);
            o->setProperty ("x", n.x); o->setProperty ("y", n.y);
            o->setProperty ("label", n.label); o->setProperty ("colour", juce::String::toHexString ((int) n.colour));
            o->setProperty ("hidden", n.hidden); o->setProperty ("face", n.face);
            juce::Array<juce::var> pv;
            for (auto& v : p.p) pv.add (v.load());
            o->setProperty ("p", pv);
            o->setProperty ("mix", p.mix.load()); o->setProperty ("level", p.level.load());
            o->setProperty ("macro", p.macro.load()); o->setProperty ("depth", p.depth.load());
            o->setProperty ("bypass", p.bypass.load());
            ns.add (juce::var (o));
        }
        for (auto& w : wires) ws.add (juce::Array<juce::var> { w.from, w.to });
        root->setProperty ("nodes", ns);
        root->setProperty ("wires", ws);
        return juce::var (root);
    }

    bool fromVar (const juce::var& v)
    {
        if (! v.isObject() || v["format"].toString() != kPatchFormat) return false;
        clear();
        if (auto* ns = v["nodes"].getArray())
            for (auto& o : *ns)
            {
                const int id = (int) o["id"], fx = fxIndexOf (o["fx"].toString());
                if (id < 0 || id >= kMaxNodes || fx < 0 || find (id) != nullptr) continue;
                Node n; n.id = id; n.fx = fx; n.x = (float) o["x"]; n.y = (float) o["y"]; n.label = o["label"].toString();
                n.colour = (juce::uint32) o["colour"].toString().getHexValue32(); n.hidden = (bool) o["hidden"]; n.face = (int) o["face"];
                nodes.push_back (n);
                auto& p = params[(size_t) id];
                p.reset();
                if (auto* pv = o["p"].getArray())
                    for (int i = 0; i < 4 && i < pv->size(); ++i) p.p[i] = (float) (*pv)[i];
                if (o.hasProperty ("mix")) p.mix = (float) o["mix"];
                if (o.hasProperty ("level")) p.level = (float) o["level"];
                if (o.hasProperty ("depth")) p.depth = (float) o["depth"];
                if (o.hasProperty ("macro")) p.macro = (int) o["macro"];
                p.bypass = (bool) o["bypass"];
            }
        if (auto* ws = v["wires"].getArray())
            for (auto& w : *ws)
            {
                const int a = (int) w[0], b = (int) w[1];
                if ((a == kIn || find (a)) && (b == kOut || find (b))) connect (a, b);
            }
        return true;
    }

    void prepare (double sr, int maxBlock)
    {
        blockSize = juce::jmax (1, maxBlock);
        dsp.prepare (sr);
        inL.assign ((size_t) blockSize, 0.0f); inR.assign ((size_t) blockSize, 0.0f);
        auto next = compile();
        const juce::SpinLock::ScopedLockType l (lock);
        current = std::move (next); pending.reset(); hasPending = false;
    }

    void commit()
    {
        auto next = compile();
        const juce::SpinLock::ScopedLockType l (lock);
        pending = std::move (next);   // retired programs are freed here, never on the audio thread
        hasPending = true;
    }

    void process (float* L, float* R, int n, const float* macros)
    {
        if (n > blockSize)
        {
            for (int o = 0; o < n; o += blockSize) process (L + o, R + o, juce::jmin (blockSize, n - o), macros);
            return;
        }
        if (hasPending.load())
        {
            const juce::SpinLock::ScopedTryLockType l (lock);
            if (l.isLocked() && pending != nullptr)
            {
                carryState (*current, *pending);
                std::swap (current, pending);
                hasPending = false;
            }
        }
        auto& prog = *current;
        if (prog.outSrcs.empty() || (int) inL.size() < n) return; // unpatched = dry through
        std::copy (L, L + n, inL.begin()); std::copy (R, R + n, inR.begin());

        for (auto& c : prog.order)
        {
            gather (prog, c.srcs, c.l.data(), c.r.data(), n);
            auto& p = params[(size_t) c.id];
            if (p.bypass.load()) continue;
            float a[4];
            for (int i = 0; i < 4; ++i) a[i] = p.p[i].load();
            const int m = p.macro.load();
            if (m >= 0 && m < kMacros) a[0] = juce::jlimit (0.0f, 1.0f, a[0] + (macros[m] - 0.5f) * 2.0f * p.depth.load());
            const float mix = p.mix.load(), lvl = p.level.load();
            float pk = 0.0f;
            for (size_t i = 0; i < (size_t) n; ++i)
            {
                float wr = 0.0f;
                const float dl = c.l[i], dr = c.r[i];
                const float wl = dsp.processOne (c.st, c.fx, a[0], a[1], a[2], a[3], dl, dr, wr);
                c.l[i] = (dl + (wl - dl) * mix) * lvl;
                c.r[i] = (dr + (wr - dr) * mix) * lvl;
                const float m = juce::jmax (std::abs (c.l[i]), std::abs (c.r[i]));
                if (m > pk) pk = m;
            }
            // Smoothed peak for the Viewer: fast attack, slow release (audio thread write only).
            const float prev = p.vu.load();
            p.vu.store (pk > prev ? pk : prev * 0.82f + pk * 0.18f);
        }
        gather (prog, prog.outSrcs, L, R, n);
        float opk = 0.0f;
        for (int i = 0; i < n; ++i) opk = juce::jmax (opk, juce::jmax (std::abs (L[i]), std::abs (R[i])));
        outVu.store (opk > outVu.load() ? opk : outVu.load() * 0.82f + opk * 0.18f);
    }

private:
    struct CNode { int id = 0, fx = 0; std::vector<int> srcs; kt::dsp::DspEngine::EffectState st; std::vector<float> l, r; };
    struct Program { std::vector<CNode> order; std::vector<int> outSrcs; };

    bool reaches (int from, int target) const
    {
        if (from == target) return true;
        for (auto& w : wires) if (w.from == from && w.to != kOut && reaches (w.to, target)) return true;
        return false;
    }
    bool feedsOut (int id) const
    {
        for (auto& w : wires) if (w.from == id && (w.to == kOut || feedsOut (w.to))) return true;
        return false;
    }

    // Topological order of nodes that reach OUT; srcs are program indices (-1 = IN).
    std::unique_ptr<Program> compile()
    {
        auto prog = std::make_unique<Program>();
        std::vector<int> live, sorted;
        for (auto& n : nodes) if (feedsOut (n.id)) live.push_back (n.id);
        auto has = [] (const std::vector<int>& v, int id) { return std::find (v.begin(), v.end(), id) != v.end(); };
        while (sorted.size() < live.size())
        {
            bool progressed = false;
            for (int id : live)
            {
                if (has (sorted, id)) continue;
                bool ready = true;
                for (auto& w : wires) if (w.to == id && w.from != kIn && has (live, w.from) && ! has (sorted, w.from)) ready = false;
                if (ready) { sorted.push_back (id); progressed = true; }
            }
            if (! progressed) break;
        }
        auto srcsFor = [&] (int to) {
            std::vector<int> s;
            for (auto& w : wires)
                if (w.to == to && (w.from == kIn || has (sorted, w.from)))
                    s.push_back (w.from == kIn ? -1 : (int) (std::find (sorted.begin(), sorted.end(), w.from) - sorted.begin()));
            return s;
        };
        for (int id : sorted)
        {
            CNode c; c.id = id; c.fx = find (id)->fx; c.srcs = srcsFor (id);
            c.l.assign ((size_t) blockSize, 0.0f); c.r.assign ((size_t) blockSize, 0.0f);
            dsp.prepareState (c.st, c.fx);
            prog->order.push_back (std::move (c));
        }
        prog->outSrcs = srcsFor (kOut);
        return prog;
    }

    void gather (const Program& prog, const std::vector<int>& srcs, float* l, float* r, int n) const
    {
        std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f);
        for (int s : srcs)
        {
            const float* sl = s < 0 ? inL.data() : prog.order[(size_t) s].l.data();
            const float* sr = s < 0 ? inR.data() : prog.order[(size_t) s].r.data();
            for (int i = 0; i < n; ++i) { l[i] += sl[i]; r[i] += sr[i]; }
        }
    }

    static void carryState (Program& from, Program& to)
    {
        for (auto& t : to.order)
            for (auto& f : from.order)
                if (f.id == t.id && f.fx == t.fx) { std::swap (f.st, t.st); break; }
    }

    std::array<NodeParams, kMaxNodes> params;
    kt::dsp::DspEngine dsp;
    int blockSize = 512;
    std::vector<float> inL, inR;
    juce::SpinLock lock;
    std::unique_ptr<Program> current { std::make_unique<Program>() }, pending;
    std::atomic<bool> hasPending { false };
};
} // namespace kv
