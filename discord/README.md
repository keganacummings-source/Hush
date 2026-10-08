# DreamShare Discord bridge

A tiny Cloudflare Worker that connects your Discord server to the DreamShare live
chat that the KYOTRIPPAH VST shows. It runs entirely on Cloudflare — nothing on
your machine, nothing to keep open.

**What it does**

| Direction | Result |
| --- | --- |
| Discord → DreamShare | Messages in the channels you allow show up in the VST live chat as `DreamUser:<name>: <text>`, tagged **DIS**. |
| DreamShare → Discord | New live-chat lines are mirrored into one Discord channel. |
| Presence | Members who are online in Discord (bots excluded) appear in the VST **Socials** list with a **DIS** tag. |

Commands: one slash command `/dream` with `ping`, `status`, `channel`, `role`,
`link`, `say`, `online`. **Only the Discord account you nominate as owner can run
them** — everyone else gets "Not allowed."

---

## 1. Create the Discord app (2 minutes)

1. <https://discord.com/developers/applications> → **New Application** → name it `DreamShare`.
2. **Bot** tab → **Reset Token** → copy it (that is `DISCORD_BOT_TOKEN`).
3. **Bot** tab → **Privileged Gateway Intents** → turn **ON**:
   - `SERVER MEMBERS INTENT`
   - `MESSAGE CONTENT INTENT`
   - `PRESENCE INTENT`
   (Presence is what makes the online list work. A private server does not need
   bot verification for it.)
4. **General Information** tab → copy **Application ID** (`DISCORD_APP_ID`) and
   **Public Key** (`DISCORD_PUBLIC_KEY`).
5. **OAuth2 → URL Generator**: scope `bot` + `applications.commands`; bot
   permissions `View Channels`, `Send Messages`, `Read Message History`. Open the
   URL and invite the bot to your server.
6. In Discord, turn on **Developer Mode**, then right-click to copy IDs:
   - your own account → `DISCORD_OWNER_ID` (this is the account that may run commands)
   - your server → `DISCORD_GUILD_ID`
   - the channel the bot posts VST chat into → `DISCORD_RELAY_CHANNEL`

## 2. Deploy the bot worker

```bash
cd discord
npx wrangler deploy

npx wrangler secret put DISCORD_BOT_TOKEN
npx wrangler secret put DISCORD_PUBLIC_KEY
npx wrangler secret put DISCORD_APP_ID
npx wrangler secret put DISCORD_OWNER_ID
npx wrangler secret put DISCORD_GUILD_ID
npx wrangler secret put DISCORD_RELAY_CHANNEL
npx wrangler secret put DREAMSHARE_BRIDGE_KEY      # any long random string
```

Edit `DREAMSHARE_API` in `wrangler.toml` if your DreamShare worker is not at
`https://dreamshare-api.keganacummings.workers.dev`.

Point the Discord app at the worker: **General Information → Interactions
Endpoint URL** = `https://<your-worker>.workers.dev/interactions`, then save.

Register the slash command once:

```bash
curl -X POST https://<your-worker>.workers.dev/ \
  -H 'content-type: application/json' \
  -H 'x-bridge-key: <the DREAMSHARE_BRIDGE_KEY you set>' \
  -d '{"cmd":"register"}'
```

Start the gateway (do this once after every deploy):

```bash
curl -X POST https://<your-worker>.workers.dev/ \
  -H 'content-type: application/json' \
  -H 'x-bridge-key: <DREAMSHARE_BRIDGE_KEY>' \
  -d '{"cmd":"start"}'
```

`/dream status` in Discord should now say `socket: connected`.

## 3. Add the bridge key to DreamShare

The DreamShare worker (`worker.js` at the repo root) needs the same
`DREAMSHARE_BRIDGE_KEY`, otherwise relaying is refused. In the Cloudflare
dashboard for the DreamShare worker: **Settings → Variables → Encrypt** and add
`DREAMSHARE_BRIDGE_KEY` with the same string. Redeploy.

For local Base44 dev, put it in `.dev.vars` at the repo root (gitignored):

```
DREAMSHARE_BRIDGE_KEY=local-dev-key
```

Without the key the bridge routes stay closed — the rest of the API is unchanged.

## 4. Choose what gets relayed

In Discord, as the owner:

```
/dream channel #general on      allow a channel
/dream channel #general off     block it
/dream role @Members on         only relay people with that role
/dream link ktrippah            your Discord account posts as DreamUser:ktrippah
/dream say hello from FL        push a line into the VST chat
/dream online                   who is online right now
```

- A channel is relayed only when it is in the allow list.
- If any relay role is set, only members holding one of those roles are relayed.
- With no roles set, everyone in an allowed channel is relayed.
- Bots are always ignored, in both directions.

## Notes

- Relay text is trimmed to 380 characters to fit the VST chat buffer.
- The bridge never stores chat history; DreamShare remains the source of truth.
- Durable Objects on the Workers free plan use the SQLite backend — this worker
  is configured for that (`new_sqlite_classes`).
