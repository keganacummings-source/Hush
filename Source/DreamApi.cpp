#include "DreamApi.h"

namespace kt
{
static const char* kEndpoint = "https://dreamshare-api.keganacummings.workers.dev/";

static juce::String readUrl(const juce::URL& url, const juce::String& extraHeaders = {})
{
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n" + extraHeaders)
                    .withConnectionTimeoutMs(15000);
    auto stream = url.createInputStream(opts);
    if (stream == nullptr)
        return {};
    return stream->readEntireStreamAsString();
}

DreamResult postAction(const juce::String& action, juce::var body, const juce::String& token)
{
    DreamResult r;
    if (! body.isObject())
        body = juce::var(new juce::DynamicObject());
    body.getDynamicObject()->setProperty("action", action);
    if (token.isNotEmpty())
        body.getDynamicObject()->setProperty("token", token);
    const auto text = juce::JSON::toString(body);
    const auto raw = readUrl(juce::URL(kEndpoint).withPOSTData(text));
    r.raw = raw;
    r.parsed = juce::JSON::parse(raw);
    if (auto* o = r.parsed.getDynamicObject())
    {
        r.ok = (bool) o->getProperty("ok");
        r.error = o->getProperty("error").toString();
        r.token = o->getProperty("token").toString();
        r.user = o->getProperty("user").toString();
        r.role = o->getProperty("role").toString();
        if (o->hasProperty("chat"))
            r.body = juce::JSON::toString(o->getProperty("chat"));
        else if (o->hasProperty("modules"))
            r.body = juce::JSON::toString(o->getProperty("modules"));
        else if (o->hasProperty("threads"))
            r.body = juce::JSON::toString(o->getProperty("threads"));
        else
            r.body = o->getProperty("body").toString();
        if (! r.ok && r.error.isEmpty() && raw.isNotEmpty())
            r.error = "DreamShare rejected " + action;
    }
    else
        r.error = raw.isEmpty() ? "DreamShare did not answer" : "Bad DreamShare response";
    return r;
}

DreamResult login(const juce::String& user, const juce::String& pass)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("user", user);
    o->setProperty("pass", pass);
    return postAction("login", juce::var(o), {});
}

DreamResult sendChat(const juce::String& token, const juce::String& text)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("text", text);
    return postAction("chat_send", juce::var(o), token);
}

DreamResult getFeed(const juce::String& token)
{
    return postAction("chat_list", juce::var(new juce::DynamicObject()), token);
}

DreamResult getThreads(const juce::String& token)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("limit", 20);
    return postAction("list_threads", juce::var(o), token);
}

DreamResult getCatalog(const juce::String& token)
{
    return postAction("module_list", juce::var(new juce::DynamicObject()), token);
}

DreamResult getModule(const juce::String& token, const juce::String& id)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("id", id);
    return postAction("module_get", juce::var(o), token);
}

DreamResult publishModule(const juce::String& token, const juce::String& name, const juce::String& jsonBody)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("name", name);
    o->setProperty("module", juce::JSON::parse(jsonBody));
    return postAction("module_publish", juce::var(o), token);
}

DreamResult deleteModule(const juce::String& token, const juce::String& id)
{
    auto* o = new juce::DynamicObject(); o->setProperty("id", id);
    return postAction("module_delete", juce::var(o), token);
}

DreamResult approveModule(const juce::String& token, const juce::String& id)
{
    auto* o = new juce::DynamicObject(); o->setProperty("id", id);
    return postAction("module_approve", juce::var(o), token);
}

DreamResult denyModule(const juce::String& token, const juce::String& id)
{
    auto* o = new juce::DynamicObject(); o->setProperty("id", id);
    return postAction("module_deny", juce::var(o), token);
}

DreamResult tagModule(const juce::String& token, const juce::String& id, const juce::String& tags)
{
    auto* o = new juce::DynamicObject(); o->setProperty("id", id); o->setProperty("tags", tags);
    return postAction("module_tag", juce::var(o), token);
}

DreamResult getMyModules(const juce::String& token)
{
    return postAction("module_my", juce::var(new juce::DynamicObject()), token);
}

DreamResult getPendingModules(const juce::String& token)
{
    return postAction("module_pending", juce::var(new juce::DynamicObject()), token);
}

DreamResult getCatalogTagged(const juce::String& token, const juce::String& tags)
{
    auto* o = new juce::DynamicObject(); o->setProperty("tags", tags);
    return postAction("module_list", juce::var(o), token);
}

DreamResult getSocial(const juce::String& token)
{
    return postAction("social_list", juce::var(new juce::DynamicObject()), token);
}

DreamResult getDM(const juce::String& token, const juce::String& peer)
{
    auto* o = new juce::DynamicObject(); o->setProperty("peer", peer);
    return postAction("dm_list", juce::var(o), token);
}

DreamResult sendDM(const juce::String& token, const juce::String& peer, const juce::String& text)
{
    auto* o = new juce::DynamicObject(); o->setProperty("to", peer); o->setProperty("text", text);
    return postAction("dm_send", juce::var(o), token);
}

DreamResult friendRequest(const juce::String& token, const juce::String& action, const juce::String& target)
{
    auto* o = new juce::DynamicObject(); o->setProperty("target", target);
    return postAction(action, juce::var(o), token);
}

DreamResult react(const juce::String& token, const juce::String& kind, const juce::String& id, const juce::String& emoji)
{
    auto* o = new juce::DynamicObject(); o->setProperty("kind", kind); o->setProperty("id", id); o->setProperty("emoji", emoji);
    return postAction("react", juce::var(o), token);
}
bool uploadAttachment(const juce::String& token, const juce::File& file, AttachRef& out, juce::String& error)
{
    if (! file.existsAsFile()) { error = "File not found"; return false; }
    const juce::int64 total = file.getSize();
    if (total <= 0) { error = "File is empty"; return false; }
    if (total > kAttachMaxBytes) { error = "File is over " + humanBytes(kAttachMaxBytes) + " (" + humanBytes(total) + ")"; return false; }
    std::unique_ptr<juce::FileInputStream> in(file.createInputStream());
    if (in == nullptr) { error = "Could not read file"; return false; }

    const int parts = (int) ((total + kAttachChunkBytes - 1) / kAttachChunkBytes);
    const auto upload = "f" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64())
                      + juce::String::toHexString(juce::Time::currentTimeMillis() & 0xffffff);
    juce::MemoryBlock chunk;
    for (int i = 0; i < parts; ++i)
    {
        chunk.reset();
        chunk.setSize((size_t) kAttachChunkBytes);
        const int got = in->read(chunk.getData(), kAttachChunkBytes);
        if (got <= 0) { error = "Read failed"; return false; }
        auto* o = new juce::DynamicObject();
        o->setProperty("upload", upload);
        o->setProperty("index", i);
        o->setProperty("parts", parts);
        o->setProperty("b64", juce::Base64::toBase64(chunk.getData(), (size_t) got));
        auto r = postAction("audio_part_b64", juce::var(o), token);
        if (! r.ok) { error = r.error.isEmpty() ? juce::String("Upload failed") : r.error; return false; }
    }
    out.upload = upload;
    out.name = file.getFileName();
    out.parts = parts;
    out.bytes = total;
    return true;
}

bool downloadAttachment(const juce::String& token, const AttachRef& ref, const juce::File& dest, juce::String& error)
{
    if (! ref.valid()) { error = "Bad attachment"; return false; }
    dest.deleteFile();
    juce::FileOutputStream outStream(dest);
    if (! outStream.openedOk()) { error = "Could not write " + dest.getFullPathName(); return false; }
    for (int i = 0; i < ref.parts; ++i)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("upload", ref.upload);
        o->setProperty("index", i);
        auto r = postAction("file_part", juce::var(o), token);
        if (! r.ok) { error = r.error.isEmpty() ? juce::String("Download failed") : r.error; return false; }
        juce::MemoryOutputStream bytes;
        if (auto* obj = r.parsed.getDynamicObject())
            if (! juce::Base64::convertFromBase64(bytes, obj->getProperty("b64").toString())) { error = "Bad file data"; return false; }
        outStream.write(bytes.getData(), bytes.getDataSize());
    }
    outStream.flush();
    return true;
}

DreamResult getDiscordMessages(const juce::String& token)
{
    return getDiscordMessages(token, {});
}

DreamResult getDiscordMessages(const juce::String& token, const juce::String& channelId)
{
    auto* o = new juce::DynamicObject();
    if (channelId.isNotEmpty())
        o->setProperty("channel", channelId);
    return postAction("discord_messages", juce::var(o), token);
}

DreamResult sendDiscordMessage(const juce::String& token, const juce::String& user, const juce::String& text)
{
    return sendDiscordMessage(token, user, text, {});
}

DreamResult sendDiscordMessage(const juce::String& token, const juce::String& user, const juce::String& text, const juce::String& channelId)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("user", user);
    o->setProperty("text", text);
    if (channelId.isNotEmpty())
        o->setProperty("channel", channelId);
    return postAction("discord_send", juce::var(o), token);
}

DreamResult getDiscordStatus(const juce::String& token)
{
    return postAction("discord_status", juce::var(new juce::DynamicObject()), token);
}

DreamResult getDiscordChannels(const juce::String& token)
{
    return postAction("discord_channels", juce::var(new juce::DynamicObject()), token);
}

}
