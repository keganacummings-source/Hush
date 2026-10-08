#pragma once
#include <JuceHeader.h>

// DreamShare file attachments.
// A dropped file is uploaded in chunks through the worker (audio_part_b64) and the chat message or
// thread comment carries a small token, so the existing chat/comment storage needs no schema change:
//     [[file:<uploadId>:<parts>:<bytes>:<name>]]
// Chat and thread views strip the token from the text and draw a clickable file chip instead.
namespace kt
{
struct AttachRef
{
    juce::String upload, name;
    int parts = 0;
    juce::int64 bytes = 0;
    bool valid() const { return upload.isNotEmpty() && parts > 0; }
};

static constexpr juce::int64 kAttachMaxBytes = 20 * 1024 * 1024;
static constexpr int kAttachChunkBytes = 1500 * 1024;

inline juce::String humanBytes(juce::int64 n)
{
    if (n < 1024) return juce::String(n) + " B";
    if (n < 1024 * 1024) return juce::String((double) n / 1024.0, 1) + " KB";
    return juce::String((double) n / (1024.0 * 1024.0), 1) + " MB";
}

inline juce::String cleanAttachName(const juce::String& raw)
{
    auto n = raw.replaceCharacters(":[]|\\/", "______").trim();
    if (n.length() > 40)
    {
        const auto ext = n.fromLastOccurrenceOf(".", true, false);
        n = n.substring(0, 40 - juce::jmin(8, ext.length())) + (ext.length() <= 8 ? ext : juce::String());
    }
    return n.isEmpty() ? juce::String("file") : n;
}

inline juce::String attachToken(const AttachRef& a)
{
    return "[[file:" + a.upload + ":" + juce::String(a.parts) + ":" + juce::String(a.bytes) + ":" + cleanAttachName(a.name) + "]]";
}

// Returns the attachment found in `text` (invalid if none) and writes the text without the token to `cleaned`.
inline AttachRef parseAttach(const juce::String& text, juce::String& cleaned)
{
    cleaned = text;
    AttachRef a;
    const int s = text.indexOf("[[file:");
    if (s < 0) return a;
    const int e = text.indexOf(s, "]]");
    if (e < 0) return a;
    juce::StringArray p;
    p.addTokens(text.substring(s + 7, e), ":", "");
    if (p.size() < 4) return a;
    a.upload = p[0];
    a.parts = p[1].getIntValue();
    a.bytes = p[2].getLargeIntValue();
    a.name = p[3];
    cleaned = (text.substring(0, s) + " " + text.substring(e + 2)).trim();
    return a;
}
} // namespace kt

// ---- Multi-attachment posts (DreamShare thread board) ---------------------------------------------
// A post can carry several files plus one shared plugin:
//     [[file:<id>:<parts>:<bytes>:<name>]]   (up to 4 per thread post, 3 per reply)
//     [[mod:<moduleId>:<name>]]              (a plugin from the catalog / "My plugins")
namespace kt
{
struct ParsedPost
{
    juce::String text, modId, modName;
    juce::Array<AttachRef> files;
};

inline ParsedPost parsePost(const juce::String& raw)
{
    ParsedPost out;
    juce::String rest = raw, clean;
    for (;;)
    {
        const int s = rest.indexOf("[[");
        if (s < 0) { clean += rest; break; }
        const int e = rest.indexOf(s, "]]");
        if (e < 0) { clean += rest; break; }
        const auto inner = rest.substring(s + 2, e);
        bool used = false;
        if (inner.startsWith("file:"))
        {
            juce::StringArray p;
            p.addTokens(inner.substring(5), ":", "");
            if (p.size() >= 4)
            {
                AttachRef a;
                a.upload = p[0]; a.parts = p[1].getIntValue(); a.bytes = p[2].getLargeIntValue(); a.name = p[3];
                if (a.valid()) { out.files.add(a); used = true; }
            }
        }
        else if (inner.startsWith("mod:"))
        {
            juce::StringArray p;
            p.addTokens(inner.substring(4), ":", "");
            if (p.size() >= 1 && p[0].isNotEmpty()) { out.modId = p[0]; out.modName = p.size() > 1 ? p[1] : p[0]; used = true; }
        }
        clean += rest.substring(0, s);
        if (! used) clean += rest.substring(s, e + 2);
        rest = rest.substring(e + 2);
    }
    out.text = clean.trim();
    return out;
}

inline juce::String modToken(const juce::String& id, const juce::String& name)
{
    return "[[mod:" + id.replaceCharacters(":[]|", "____") + ":" + cleanAttachName(name) + "]]";
}

inline juce::String fileKindOf(const juce::String& name)
{
    const auto ext = name.fromLastOccurrenceOf(".", false, false).toLowerCase();
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp") return "image";
    if (ext == "zip" || ext == "7z" || ext == "rar") return "zip";
    if (ext == "wav" || ext == "mp3" || ext == "flac" || ext == "ogg" || ext == "aif" || ext == "aiff") return "audio";
    return "file";
}
} // namespace kt
