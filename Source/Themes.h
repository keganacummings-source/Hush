#pragma once

#include <JuceHeader.h>

// Shared theme registry for the native editor and every modular surface.
// Theme ids are stable serialization keys: modules should store themeFamily/theme, not raw colours.
namespace kt {

struct ThemePalette
{
    const char* id;
    const char* name;
    juce::uint32 bg;
    juce::uint32 panel;
    juce::uint32 accent;
    juce::uint32 text;
    juce::uint32 muted;
    juce::uint32 peg;
    juce::uint32 pegHot;
    juce::uint32 knob;
    juce::uint32 border;
    const char* fontFamily = "Segoe UI";
    float textScale = 1.0f;
    float cornerRadius = 8.0f;
    const char* backgroundStyle = "flat";
    const char* bodyMaterial = "panel";
    const char* borderStyle = "rounded";
    const char* screenStyle = "crt";
    const char* knobStyle = "soft";
    const char* ledStyle = "dot";
    const char* textStyle = "clean";
    const char* panelTexture = "none";
    const char* waveformStyle = "line";
    const char* meterStyle = "bar";
    const char* highlightStyle = "glow";
};

inline constexpr ThemePalette kThemes[] = {
    { "trippah", "Trippah", 0xff0e0c14, 0xff1a1624, 0xffc77dff, 0xfff0e6ff, 0xff8a7aa8, 0xff3a2e4a, 0xffe0a0ff, 0xff2a2238, 0xff4a3a5e, "Segoe UI", 1.0f, 8.0f },
    { "goonr", "Goonr", 0xff0a1210, 0xff121c18, 0xff3dffb0, 0xffe0fff0, 0xff6a9a80, 0xff1e3a30, 0xff80ffc0, 0xff1a2a22, 0xff2a4a3a, "Trebuchet MS", 1.0f, 8.0f },
    { "abyss", "Abyss", 0xff05080f, 0xff0c121c, 0xff3a8cff, 0xffd0e4ff, 0xff5a7aaa, 0xff1a2838, 0xff80b0ff, 0xff121a28, 0xff2a3a50, "Arial", 1.0f, 8.0f },
    { "amber", "Amber", 0xff140e08, 0xff221810, 0xffff9a3c, 0xfffff0e0, 0xffa08060, 0xff3a2a18, 0xffffc080, 0xff2a1e12, 0xff4a3a20, "Segoe UI", 1.0f, 8.0f },
    { "bloodmoon", "Bloodmoon", 0xff120808, 0xff1e1010, 0xffff4060, 0xffffe0e4, 0xffa06070, 0xff3a1a20, 0xffff80a0, 0xff2a1418, 0xff4a2028, "Arial", 1.0f, 8.0f },
    { "cobalt", "Cobalt", 0xff080c14, 0xff10182a, 0xff4080ff, 0xffe0ecff, 0xff6080b0, 0xff1a2840, 0xff80b0ff, 0xff121c30, 0xff2a3a58, "Tahoma", 1.0f, 8.0f },
    { "ember", "Ember", 0xff120a06, 0xff1e140c, 0xffff6030, 0xffffece0, 0xffa07050, 0xff3a2418, 0xffffa080, 0xff2a1a10, 0xff4a3020, "Trebuchet MS", 1.0f, 8.0f },
    { "fog", "Fog", 0xff101418, 0xff1a2028, 0xffa0c0d0, 0xffe8f0f4, 0xff708090, 0xff2a3038, 0xffc0d8e0, 0xff1e242c, 0xff384048, "Verdana", 1.0f, 8.0f },
    { "graphite", "Graphite", 0xff101010, 0xff1a1a1a, 0xffb0b0b0, 0xfff0f0f0, 0xff707070, 0xff2a2a2a, 0xffd0d0d0, 0xff1e1e1e, 0xff3a3a3a, "Segoe UI", 1.0f, 8.0f },
    { "honey", "Honey", 0xff14100a, 0xff221c12, 0xffffc040, 0xfffff8e0, 0xffa09050, 0xff3a3018, 0xffffe080, 0xff2a2210, 0xff4a3a20, "Segoe UI", 1.0f, 8.0f },
    { "ice", "Ice", 0xff0a1014, 0xff121c24, 0xff80d0ff, 0xffe8f8ff, 0xff60a0c0, 0xff1a3040, 0xffb0e8ff, 0xff142028, 0xff2a4050, "Arial", 1.0f, 8.0f },
    { "ink", "Ink", 0xff08080c, 0xff101018, 0xff6080ff, 0xffe0e4ff, 0xff5060a0, 0xff1a1a30, 0xffa0b0ff, 0xff121220, 0xff2a2a48, "Segoe UI", 1.0f, 8.0f },
    { "lagoon", "Lagoon", 0xff061210, 0xff0c1e1a, 0xff30d0a0, 0xffe0fff4, 0xff50a080, 0xff1a3a30, 0xff80ffc0, 0xff102820, 0xff2a4a3a, "Trebuchet MS", 1.0f, 8.0f },
    { "lilac", "Lilac", 0xff100e14, 0xff1a1622, 0xffc080ff, 0xfff4e8ff, 0xff8070a0, 0xff2a2438, 0xffe0c0ff, 0xff1e1a2a, 0xff3a3048, "Segoe UI", 1.0f, 8.0f },
    { "mint", "Mint", 0xff0a1210, 0xff121c18, 0xff40e0a0, 0xffe0fff0, 0xff60a080, 0xff1a3a2a, 0xff80ffc0, 0xff12281e, 0xff2a4a38, "Segoe UI", 1.0f, 8.0f },
    { "neon", "Neon", 0xff08080c, 0xff101018, 0xff00ffc0, 0xffe0fff8, 0xff40a080, 0xff1a2a28, 0xff80ffe0, 0xff121a1a, 0xff2a3a38, "Segoe UI", 1.0f, 8.0f },
    { "pine", "Pine", 0xff0a100c, 0xff121a14, 0xff40c060, 0xffe0ffe8, 0xff509060, 0xff1a3020, 0xff80e0a0, 0xff122018, 0xff2a4030, "Verdana", 1.0f, 8.0f },
    { "plum", "Plum", 0xff10080e, 0xff1a1018, 0xffc040a0, 0xffffe0f0, 0xffa06080, 0xff3a1a30, 0xffff80d0, 0xff24121e, 0xff4a2840, "Trebuchet MS", 1.0f, 8.0f },
    { "rust", "Rust", 0xff120c08, 0xff1e1610, 0xffe07030, 0xfffff0e0, 0xffa07050, 0xff3a2818, 0xffffa060, 0xff2a1c12, 0xff4a3020, "Segoe UI", 1.0f, 8.0f },
    { "steel", "Steel", 0xff0c1014, 0xff141c24, 0xff80a0c0, 0xffe8f0f8, 0xff6080a0, 0xff1a2838, 0xffb0c8e0, 0xff121a24, 0xff2a3a4a, "Arial", 1.0f, 8.0f },
    { "void", "Void", 0xff060608, 0xff0c0c10, 0xffa080ff, 0xfff0e8ff, 0xff7060a0, 0xff1a1830, 0xffc0a0ff, 0xff10101a, 0xff2a2848, "Segoe UI", 1.0f, 8.0f },
    { "wine", "Wine", 0xff10080a, 0xff1a1014, 0xffc04060, 0xffffe0e8, 0xffa06070, 0xff3a1a24, 0xffff80a0, 0xff241218, 0xff4a2030, "Segoe UI", 1.0f, 8.0f },
    { "default", "Default", 0xff12100e, 0xff1c1814, 0xffefe6d6, 0xffefe6d6, 0xff8a8070, 0xff3a3128, 0xffffe0a0, 0xff2a241e, 0xff4a4038, "Segoe UI", 1.0f, 8.0f },
    { "sakura", "Sakura", 0xff140a10, 0xff21111b, 0xffff79b0, 0xffffeaf3, 0xffa66f86, 0xff3a1d2b, 0xffffa8c9, 0xff2c1824, 0xff523044, "Segoe UI", 1.0f, 8.0f },
    { "moss", "Moss", 0xff0b1109, 0xff151d11, 0xff9acb52, 0xffefffe1, 0xff718d55, 0xff27361d, 0xffc0e982, 0xff1d2917, 0xff3d512e, "Verdana", 1.0f, 8.0f },
    { "ultraviolet", "Ultraviolet", 0xff090615, 0xff161026, 0xff9b6cff, 0xfff0eaff, 0xff7763a8, 0xff2b1d4a, 0xffc5aaff, 0xff21163a, 0xff45336a, "Tahoma", 1.0f, 8.0f },
    { "vapor", "Vapor", 0xff080d16, 0xff111d2c, 0xff59d9ff, 0xffe8fbff, 0xff6397aa, 0xff1d3541, 0xff9ceaff, 0xff172936, 0xff315363, "Trebuchet MS", 1.0f, 8.0f },
    { "arctic", "Arctic", 0xff071116, 0xff10202a, 0xff9be7ff, 0xffe9fbff, 0xff6d9da9, 0xff1b3942, 0xffc5f2ff, 0xff142b35, 0xff35525e, "Arial", 1.0f, 8.0f },
    { "sunset", "Sunset", 0xff160b0a, 0xff261512, 0xffff7657, 0xffffeee7, 0xffa67868, 0xff40251e, 0xffffaa90, 0xff302019, 0xff5a382f, "Segoe UI", 1.0f, 8.0f },
    { "ocean", "Ocean", 0xff061018, 0xff0c1b29, 0xff2ec4ff, 0xffe2f6ff, 0xff5f8ea8, 0xff183646, 0xff7ee0ff, 0xff102938, 0xff2b4b5f, "Segoe UI", 1.0f, 8.0f },
    { "carbon", "Carbon", 0xff090a0b, 0xff141618, 0xffd2d6da, 0xfff3f5f6, 0xff7c8389, 0xff25292d, 0xffedf0f2, 0xff1b1f22, 0xff3a4045, "Arial", 1.0f, 8.0f },
    { "rose", "Rose", 0xff130a0f, 0xff21121a, 0xfff18ab0, 0xffffedf4, 0xffa7798b, 0xff3a2230, 0xffffb0c9, 0xff2c1822, 0xff513140, "Trebuchet MS", 1.0f, 8.0f },
    { "mono", "Mono", 0xff090b0d, 0xff12161a, 0xffb8ff72, 0xffeaffdc, 0xff77965f, 0xff24351c, 0xffd0ff9b, 0xff1b2817, 0xff3b5130, "Segoe UI", 1.0f, 8.0f },
    { "terminal", "Terminal", 0xff020705, 0xff07110c, 0xff38ff88, 0xffd6ffe6, 0xff4c9f6a, 0xff12301d, 0xff8dffb5, 0xff0d2416, 0xff255536, "Segoe UI", 1.0f, 8.0f },
    { "candy", "Candy", 0xff120b14, 0xff211425, 0xffff72d0, 0xffffeffa, 0xffa9789e, 0xff3b2340, 0xffffa6df, 0xff2c1a31, 0xff52364e, "Trebuchet MS", 1.0f, 8.0f },
    { "solar", "Solar", 0xff130f06, 0xff231c0c, 0xffffd34d, 0xfffff7d0, 0xffa99a6a, 0xff3d3314, 0xffffe28a, 0xff2d2510, 0xff56481e, "Verdana", 1.0f, 8.0f },
    { "forest", "Forest", 0xff07100a, 0xff0e1a11, 0xff4fe07c, 0xffe3ffe9, 0xff5d936d, 0xff173323, 0xff8bf0a6, 0xff11251a, 0xff2b4a35, "Segoe UI", 1.0f, 8.0f },
    { "midnight", "Midnight", 0xff05070d, 0xff0c1020, 0xff7d8dff, 0xffe8ebff, 0xff626d9d, 0xff1b2340, 0xffaab4ff, 0xff121a31, 0xff303c62, "Segoe UI", 1.0f, 8.0f },
    { "orchid", "Orchid", 0xff100812, 0xff1c1020, 0xffe08cff, 0xffffebff, 0xff966fa4, 0xff35203f, 0xfff0b0ff, 0xff28172f, 0xff503657, "Segoe UI", 1.0f, 8.0f },
    { "copper", "Copper", 0xff120c08, 0xff21150f, 0xffe19a63, 0xfffff0df, 0xff9b7659, 0xff3b281b, 0xffffbd8a, 0xff2d1d14, 0xff533d2d, "Tahoma", 1.0f, 8.0f },
    { "ghost", "Ghost", 0xff0b0d0f, 0xff171a1d, 0xffdbe4ea, 0xfff7fbff, 0xff85919a, 0xff2b3339, 0xffffffff, 0xff20262b, 0xff414a51, "Arial", 1.0f, 8.0f },
    { "arcade", "Arcade", 0xff0a0712, 0xff171024, 0xffff4dd8, 0xfff5e9ff, 0xffa45f98, 0xff321d42, 0xffff8ee6, 0xff21152d, 0xff4a2f58, "Trebuchet MS", 1.0f, 8.0f },
    // Batch 2 - deeper, more in-depth themes
    { "magma", "Magma", 0xff100404, 0xff1e0a08, 0xffff3010, 0xffffe8e0, 0xffa05040, 0xff3a1810, 0xffff7050, 0xff2a120e, 0xff4a2418, "Segoe UI", 1.0f, 8.0f },
    { "aurora", "Aurora", 0xff040810, 0xff0a1420, 0xff40ffa0, 0xffe0fff0, 0xff50a080, 0xff1a3040, 0xff80ffc0, 0xff102030, 0xff2a4a50, "Trebuchet MS", 1.0f, 8.0f },
    { "brass", "Brass", 0xff120c04, 0xff221808, 0xffd4a040, 0xfffff4d8, 0xffa08040, 0xff3a2810, 0xffffd070, 0xff2a1c0a, 0xff4a3818, "Tahoma", 1.0f, 8.0f },
    { "toxic", "Toxic", 0xff080a04, 0xff101408, 0xff80ff20, 0xffe8ffe0, 0xff508040, 0xff1a2810, 0xffc0ff60, 0xff142010, 0xff2a4020, "Arial", 1.0f, 8.0f },
    { "crimson", "Crimson", 0xff100406, 0xff1e0a0e, 0xffff2040, 0xffffe0e4, 0xffa04050, 0xff3a1018, 0xffff5070, 0xff2a0e12, 0xff4a1820, "Segoe UI", 1.0f, 8.0f },
    { "jade", "Jade", 0xff040a08, 0xff0a1814, 0xff20c080, 0xffe0fff0, 0xff50a070, 0xff103020, 0xff50e0a0, 0xff0e2018, 0xff204030, "Verdana", 1.0f, 8.0f },
    { "plasma", "Plasma", 0xff0a0410, 0xff14081e, 0xffff30c0, 0xffffe8f8, 0xffa05090, 0xff2a103a, 0xffff70d0, 0xff1e0a28, 0xff4a1850, "Trebuchet MS", 1.0f, 8.0f },
    { "gold", "Gold", 0xff100a02, 0xff1e1408, 0xffffd030, 0xfffff8d8, 0xffa09030, 0xff3a2e10, 0xffffe060, 0xff2a200a, 0xff4a3818, "Segoe UI", 1.0f, 8.0f },
    { "cobalt", "Cobalt2", 0xff040810, 0xff0a1422, 0xff3080ff, 0xffd0e8ff, 0xff5070a0, 0xff102040, 0xff60a0ff, 0xff0e1828, 0xff2a3a58, "Arial", 1.0f, 8.0f },
    { "magma2", "Inferno", 0xff100200, 0xff1e0604, 0xffff2010, 0xffffe4d8, 0xffa04030, 0xff3a1008, 0xffff6040, 0xff2a0e0a, 0xff4a1810, "Tahoma", 1.0f, 8.0f },
    { "cyber", "Cyber", 0xff040608, 0xff0a1018, 0xff00e0ff, 0xffe0f8ff, 0xff408090, 0xff102028, 0xff60e0ff, 0xff0e1820, 0xff2a3848, "Consolas", 1.0f, 8.0f },
    { "lavender", "Lavender", 0xff0c0814, 0xff161020, 0xffb090ff, 0xfff4f0ff, 0xff8070b0, 0xff282038, 0xffd0b0ff, 0xff1c1428, 0xff3a2848, "Segoe UI", 1.0f, 8.0f },
    { "sand", "Sand", 0xff100c08, 0xff1c1814, 0xffd0b080, 0xfff8f0e0, 0xffa09070, 0xff383020, 0xffe0c090, 0xff28201a, 0xff484030, "Verdana", 1.0f, 8.0f },
    { "neon2", "Synthwave", 0xff0a0414, 0xff14082a, 0xffff40ff, 0xffffe0ff, 0xffa060a0, 0xff2a1040, 0xffff80ff, 0xff1e0a38, 0xff4a1858, "Trebuchet MS", 1.0f, 8.0f },
    { "matrix", "Matrix", 0xff020a02, 0xff041404, 0xff20ff40, 0xffd0ffd0, 0xff40a050, 0xff103018, 0xff60ff80, 0xff082008, 0xff184028, "Consolas", 1.0f, 8.0f },
    { "amber2", "Old Amber", 0xff100a04, 0xff1c1408, 0xffe0a020, 0xfffff0d0, 0xffa07030, 0xff3a2810, 0xffffc050, 0xff281c0a, 0xff483818, "Georgia", 1.0f, 8.0f },
    { "frost", "Frost", 0xff040810, 0xff0a1420, 0xffa0d0ff, 0xfff0f8ff, 0xff6090b0, 0xff1a3040, 0xffc0e0ff, 0xff0e1c28, 0xff2a4050, "Arial", 1.0f, 8.0f },
    { "moss2", "Deep Moss", 0xff040a06, 0xff0a1810, 0xff60c040, 0xffe0ffe0, 0xff508040, 0xff103018, 0xff90e060, 0xff0e2010, 0xff204028, "Verdana", 1.0f, 8.0f },
    { "rose2", "Deep Rose", 0xff0c0608, 0xff1a0e14, 0xffff6090, 0xffffe8f0, 0xffa05070, 0xff301820, 0xffff90b0, 0xff24101a, 0xff482030, "Trebuchet MS", 1.0f, 8.0f },
    // Site themes (keganacummings-source.github.io/Site)
    { "ash", "Ash", 0xff12151c, 0xff1b212b, 0xff9bb4c8, 0xffe7eef4, 0xff8b97a6, 0xff10141b, 0xff7f96aa, 0xff10141b, 0xff2c3542, "Segoe UI", 1.0f, 0.0f },
    { "bone", "Bone", 0xff161310, 0xff24201b, 0xfff0e2c8, 0xfff7f1e6, 0xffa39888, 0xff1c1814, 0xffd4b483, 0xff1c1814, 0xff3a332c, "Georgia", 1.0f, 2.0f },
    { "cherry", "Cherry", 0xff140810, 0xff241018, 0xffff6a9a, 0xfffff0f4, 0xffc498a8, 0xff10060c, 0xffff8ab0, 0xff10060c, 0xff4a2434, "Segoe UI", 1.0f, 10.0f },
    { "cinder", "Cinder", 0xff120c0c, 0xff201414, 0xffe08a8a, 0xfff8eeee, 0xffb89898, 0xff0e0808, 0xfff0b0b0, 0xff0e0808, 0xff402828, "Segoe UI", 1.0f, 6.0f },
    { "dawn", "Dawn", 0xff1a120c, 0xff2a1c14, 0xffe8a05a, 0xfff6efe6, 0xffb9a48f, 0xff140e0a, 0xffc46a3a, 0xff140e0a, 0xff4a3424, "Segoe UI", 1.0f, 10.0f },
    { "dusk", "Dusk", 0xff0c1020, 0xff14182c, 0xff7aa2ff, 0xffe8eeff, 0xff8b97b8, 0xff0a0e1a, 0xff9a7adf, 0xff0a0e1a, 0xff2a3350, "Segoe UI", 1.0f, 0.0f },
    { "light", "Light", 0xffefe6d6, 0xfff7f1e6, 0xff9a3412, 0xff1c1410, 0xff6b5344, 0xfff3eadc, 0xffb45309, 0xfff3eadc, 0xffddcbb6, "Georgia", 1.0f, 2.0f },
    { "marrow", "Marrow", 0xff16130e, 0xff262218, 0xffe6d2b0, 0xfff8f4ec, 0xffb4a890, 0xff12100c, 0xfff0e0c4, 0xff12100c, 0xff463c30, "Segoe UI", 1.0f, 4.0f },
    { "olive", "Olive", 0xff101208, 0xff1c2010, 0xffc6d26a, 0xfff4f6e4, 0xffa4aa84, 0xff0c0e06, 0xffd8e48a, 0xff0c0e06, 0xff3a4024, "Segoe UI", 1.0f, 2.0f },
    { "paper", "Paper", 0xffefe8dc, 0xfff7f2ea, 0xffb4482c, 0xff2a221c, 0xff7a6e62, 0xffe7e0d4, 0xffc46a48, 0xffe7e0d4, 0xffd8d0c4, "Segoe UI", 1.0f, 8.0f },
    { "static", "Static", 0xff050806, 0xff0c120e, 0xff8dff6a, 0xffe8ffe4, 0xff7a9a70, 0xff040604, 0xffb6ff9a, 0xff040604, 0xff1c3020, "Segoe UI", 1.0f, 0.0f },
    { "sulfur", "Sulfur", 0xff070804, 0xff12180c, 0xffc6f531, 0xfff3f8d8, 0xff8d9a68, 0xff0c1008, 0xffd24a22, 0xff0c1008, 0xff2c3814, "Segoe UI", 1.0f, 0.0f },
    { "velvet", "Velvet", 0xff12060a, 0xff220c12, 0xffc43a58, 0xfff8e8ec, 0xffb48894, 0xff0e0408, 0xffe06078, 0xff0e0408, 0xff401820, "Segoe UI", 1.0f, 8.0f },
    { "violet", "Violet", 0xff0c0814, 0xff181028, 0xffc084fc, 0xfff3e8ff, 0xffa78bb8, 0xff120c1c, 0xffa855f7, 0xff120c1c, 0xff342450, "Segoe UI", 1.0f, 2.0f },
    { "vsttrippah", "Vsttrippah", 0xff0e0608, 0xff1a0c12, 0xffc04068, 0xfff4e4ea, 0xff9a7884, 0xff12080c, 0xffa02848, 0xff12080c, 0xff341820, "Segoe UI", 1.0f, 0.0f },
};

inline constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
static_assert(kThemeCount >= 61, "KYOTRIPPAH theme registry must retain the 61 stable presets");

inline const ThemePalette& themeById(const juce::String& id)
{
    for (int i = 0; i < kThemeCount; ++i)
        if (id.equalsIgnoreCase(kThemes[i].id))
            return kThemes[i];
    return kThemes[kThemeCount - 1];
}

inline juce::Colour c(juce::uint32 argb) { return juce::Colour(argb); }

inline juce::Font font(const ThemePalette& t, float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(t.fontFamily, size * t.textScale, bold ? juce::Font::bold : juce::Font::plain));
}

// ---- DreamShare readability scale -------------------------------------------------------
// DreamShare (chat, threads, catalog cards) uses dsFont() so text can be made larger or smaller
// without touching the builders. The value is remembered between sessions.
inline float& dsScale()
{
    // Default a touch larger so Discord/chat/threads stay readable in the VST frame.
    static float s = 1.22f;
    return s;
}

inline juce::File dsScaleFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("KyotoSpxrit").getChildFile("dreamshare_ui_scale.txt");
}

inline void loadDsScale()
{
    auto f = dsScaleFile();
    if (f.existsAsFile())
    {
        const float v = f.loadFileAsString().trim().getFloatValue();
        if (v >= 0.8f && v <= 1.6f) dsScale() = v;
    }
}

inline void saveDsScale()
{
    auto f = dsScaleFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::String(dsScale(), 2));
}

inline juce::Font dsFont(const ThemePalette& t, float size, bool bold = false)
{
    return font(t, size * dsScale(), bold);
}

} // namespace kt
