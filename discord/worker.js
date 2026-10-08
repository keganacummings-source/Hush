// DreamShare Discord bridge — Cloudflare Worker + one Durable Object.
//
// What it does
//   Discord  -> DreamShare live chat : every message posted in an allowed channel
//                                     is relayed as  DreamUser:<name>: <text>
//                                     with a `dis` flag the VST draws a DIS tag for.
//   DreamShare -> Discord            : new live-chat lines are mirrored into one
//                                     Discord channel.
//   Presence                         : members who are online in the guild (bots
//                                     excluded) are pushed to DreamShare so the
//                                     Socials list shows them with a DIS tag.
//
// Commands (slash): /dream link|channel|online|status|say|ping — owner only.
//
// Deploy + setup: see discord/README.md. Nothing here runs locally in the sandbox.

const DISCORD_API = 'https://discord.com/api/v10';
const GATEWAY_URL = 'wss://gateway.discord.gg/?v=10&encoding=json';
// GUILDS | GUILD_MEMBERS | GUILD_PRESENCES | GUILD_MESSAGES | MESSAGE_CONTENT
const INTENTS = 1 + 2 + 256 + 512 + 32768;
const SWEEP_MS = 15000;   // alarm tick: heartbeats, reconnect, outbound poll, presence

// Guilds the bridge is allowed to listen on (Kyoto + user servers).
const ALLOWED_GUILDS = [
  '1518252339864014929', // Kyoto original
  '1440066181624234106', // TurboNerdos → TRBN tag
];
function guildAllowed(guildId, env) {
  const id = String(guildId || '');
  if (!id) return false;
  if (ALLOWED_GUILDS.indexOf(id) >= 0) return true;
  const envOne = String((env && env.DISCORD_GUILD_ID) || '');
  if (envOne && envOne === id) return true;
  const envMany = String((env && env.DISCORD_GUILD_IDS) || '');
  if (envMany && envMany.split(/[,\s]+/).filter(Boolean).indexOf(id) >= 0) return true;
  return false;
}

// Kyoto → KYTO, TurboNerdos → TRBN
function originTagForGuild(guildId) {
  const id = String(guildId || '');
  if (id === '1440066181624234106') return 'TRBN';
  if (id === '1518252339864014929') return 'KYTO';
  return 'KYTO';
}

function json(o, status = 200) {
  return new Response(JSON.stringify(o), {
    status,
    headers: { 'content-type': 'application/json', 'access-control-allow-origin': '*' }
  });
}

function hexBytes(hex) {
  const clean = String(hex || '').trim();
  if (clean.length === 0 || clean.length % 2) return null;
  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    const b = parseInt(clean.substr(i * 2, 2), 16);
    if (Number.isNaN(b)) return null;
    out[i] = b;
  }
  return out;
}

// Ed25519 check of every interaction request (Discord requires it).
async function verifyInteraction(request, raw, publicKey) {
  const sig = request.headers.get('x-signature-ed25519');
  const ts = request.headers.get('x-signature-timestamp');
  const key = hexBytes(publicKey);
  const bytes = hexBytes(sig);
  if (!sig || !ts || !key || !bytes) return false;
  try {
    const imported = await crypto.subtle.importKey('raw', key, { name: 'Ed25519' }, false, ['verify']);
    return await crypto.subtle.verify({ name: 'Ed25519' }, imported, bytes, new TextEncoder().encode(ts + raw));
  } catch (_) {
    return false;
  }
}

function botHeaders(env) {
  return { authorization: 'Bot ' + env.DISCORD_BOT_TOKEN, 'content-type': 'application/json' };
}

async function discordPost(env, path, body) {
  const res = await fetch(DISCORD_API + path, {
    method: 'POST',
    headers: botHeaders(env),
    body: JSON.stringify(body)
  });
  return res;
}

// ---------------------------------------------------------------- gateway DO
// Holds the Discord gateway socket, the member/presence cache and all config.
export class DiscordGateway {
  constructor(state, env) {
    this.state = state;
    this.env = env;
    this.ws = null;
    this.seq = null;
    this.sessionId = null;
    this.resumeUrl = null;
    this.heartbeatMs = 41250;
    this.lastBeat = 0;
    this.ready = false;
    this.members = new Map();   // id -> { name, bot }
    this.presence = new Map();  // id -> 'online' | 'idle' | 'dnd' | 'offline'
    this.cursor = null;         // last DreamShare chat timestamp mirrored to Discord
    this.cfg = { channels: [], roles: [], relayChannel: '', links: {} };
  }

  async fetch(request) {
    const cmd = new URL(request.url).pathname.replace(/^\/+/, '') || 'status';
    if (cmd !== 'config') await this.loadCfg();

    if (cmd === 'start') {
      this.connect();
      await this.state.storage.setAlarm(Date.now() + SWEEP_MS);
      return json({ ok: true, ready: this.ready });
    }
    if (cmd === 'stop') {
      if (this.ws) { try { this.ws.close(1000); } catch (_) {} this.ws = null; }
      this.ready = false;
      return json({ ok: true });
    }
    if (cmd === 'online') {
      const online = this.onlineList();
      return json({ ok: true, count: online.length, online });
    }
    if (cmd === 'config') {
      const body = await request.json().catch(() => ({}));
      const cfg = await this.getCfg();
      if (Array.isArray(body.channels)) cfg.channels = body.channels.map(String).slice(0, 50);
      if (Array.isArray(body.roles)) cfg.roles = body.roles.map(String).slice(0, 50);
      if (typeof body.relayChannel === 'string') cfg.relayChannel = body.relayChannel;
      if (body.link && body.link.id) cfg.links[String(body.link.id)] = String(body.link.name || '').slice(0, 48);
      await this.state.storage.put('cfg', cfg);
      this.cfg = cfg;
      return json({ ok: true, cfg });
    }
    return json({
      ok: true,
      ready: this.ready,
      socket: this.ws ? this.ws.readyState : null,
      members: this.members.size,
      online: this.onlineList().length,
      cursor: this.cursor,
      cfg: this.cfg
    });
  }

  async loadCfg() {
    const saved = await this.state.storage.get('cfg');
    if (saved) this.cfg = Object.assign({ channels: [], roles: [], relayChannel: '', links: {} }, saved);
    if (this.cursor === null) this.cursor = (await this.state.storage.get('cursor')) ?? null;
    return this.cfg;
  }

  async getCfg() { await this.loadCfg(); return this.cfg; }

  // --------------------------------------------------------------- socket
  connect() {
    if (this.ws && (this.ws.readyState === 0 || this.ws.readyState === 1)) return;
    let ws;
    try {
      ws = new WebSocket(this.sessionId && this.resumeUrl ? this.resumeUrl : GATEWAY_URL);
    } catch (_) {
      return;
    }
    this.ws = ws;
    ws.addEventListener('message', (event) => {
      let payload = null;
      try { payload = JSON.parse(event.data); } catch (_) { return; }
      this.onPacket(payload);
    });
    ws.addEventListener('close', () => { this.ws = null; this.ready = false; });
    ws.addEventListener('error', () => {});
  }

  send(payload) {
    if (!this.ws || this.ws.readyState !== 1) return;
    try { this.ws.send(JSON.stringify(payload)); } catch (_) {}
  }

  beat() {
    this.lastBeat = Date.now();
    this.send({ op: 1, d: this.seq });
  }

  async onPacket(p) {
    if (p.s !== null && p.s !== undefined) this.seq = p.s;

    if (p.op === 10) {                                  // HELLO
      this.heartbeatMs = p.d.heartbeat_interval || 41250;
      this.beat();
      if (this.sessionId) this.send({ op: 6, d: { token: this.env.DISCORD_BOT_TOKEN, session_id: this.sessionId, seq: this.seq } });
      else this.identify();
      return;
    }
    if (p.op === 11) return;                            // HEARTBEAT_ACK
    if (p.op === 1) { this.beat(); return; }            // server asked for a beat
    if (p.op === 7) { if (this.ws) { try { this.ws.close(); } catch (_) {} } this.ws = null; return; }
    if (p.op !== 0) return;                             // DISPATCH

    const d = p.d || {};
    if (p.t === 'READY') {
      this.sessionId = d.session_id;
      this.resumeUrl = (d.resume_gateway_url || GATEWAY_URL.replace('wss://', 'wss://')) + '/?v=10&encoding=json';
      this.ready = true;
      this.sweepPresence();
      return;
    }
    if (p.t === 'GUILD_CREATE') {
      for (const m of d.members || []) this.rememberMember(m);
      for (const pres of d.presences || []) this.rememberPresence(pres);
      this.sweepPresence();
      return;
    }
    if (p.t === 'GUILD_MEMBER_ADD' || p.t === 'GUILD_MEMBER_UPDATE') { this.rememberMember(d); return; }
    if (p.t === 'GUILD_MEMBER_REMOVE') { this.members.delete(String(d.user && d.user.id)); this.presence.delete(String(d.user && d.user.id)); return; }
    if (p.t === 'PRESENCE_UPDATE') { this.rememberPresence(d); this.sweepPresence(); return; }
    if (p.t === 'MESSAGE_CREATE') { await this.relayIn(d); return; }
  }

  identify() {
    this.send({
      op: 2,
      d: {
        token: this.env.DISCORD_BOT_TOKEN,
        intents: INTENTS,
        properties: { os: 'cloudflare', browser: 'dreamshare-bridge', device: 'dreamshare-bridge' }
      }
    });
  }

  rememberMember(m) {
    const u = m && m.user;
    if (!u || !u.id) return;
    this.members.set(String(u.id), {
      name: m.nick || u.global_name || u.username || String(u.id),
      bot: !!u.bot || !!u.system
    });
  }

  rememberPresence(p) {
    const u = p && p.user;
    if (!u || !u.id) return;
    if (u.bot || u.system) return;
    if (!this.members.has(String(u.id))) this.members.set(String(u.id), { name: u.global_name || u.username || String(u.id), bot: false });
    this.presence.set(String(u.id), String(p.status || 'offline'));
  }

  // Live (non-offline, non-bot) members, DreamShare-linked names where known.
  onlineList() {
    const cfg = this.cfg || { links: {} };
    const out = [];
    for (const [id, status] of this.presence) {
      if (status === 'offline') continue;
      const member = this.members.get(id);
      if (!member || member.bot) continue;
      const linked = cfg.links ? cfg.links[id] : '';
      out.push({ id, name: linked || member.name, dis: true, tag: 'KYTO' });
    }
    out.sort((a, b) => a.name.localeCompare(b.name));
    return out.slice(0, 100);
  }

  displayName(author, member) {
    const id = String(author.id);
    const linked = this.cfg.links ? this.cfg.links[id] : '';
    if (linked) return linked;
    return (member && member.nick) || author.global_name || author.username || id;
  }

  memberHasRole(member, roles) {
    if (!Array.isArray(roles) || roles.length === 0) return true;
    const owned = (member && Array.isArray(member.roles)) ? member.roles : [];
    return roles.some((r) => owned.indexOf(r) >= 0);
  }

  // ------------------------------------------------------- Discord -> DreamShare
  async relayIn(d) {
    if (!d || !d.author || d.author.bot || !d.guild_id) return;
    if (!guildAllowed(d.guild_id, this.env)) return;
    if (!String(d.content || '').trim()) return;
    const cfg = await this.getCfg();
    // If no channel allow-list configured yet, accept any channel in an allowed guild.
    if (cfg.channels.length && cfg.channels.indexOf(String(d.channel_id)) < 0) return;
    const isOwner = String(d.author.id) === String(this.env.DISCORD_OWNER_ID || '');
    if (!isOwner && !this.memberHasRole(d.member, cfg.roles)) return;
    if (d.member) this.rememberMember(d.member);

    const name = this.displayName(d.author, d.member);
    const text = String(d.content).replace(/\s+/g, ' ').trim().slice(0, 380);
    const originTag = originTagForGuild(d.guild_id); // KYTO or TRBN
    try {
      await fetch(String(this.env.DREAMSHARE_API || '').replace(/\/+$/, '') + '/', {
        method: 'POST',
        headers: { 'content-type': 'application/json', 'x-dreamshare-bridge': this.env.DREAMSHARE_BRIDGE_KEY || '' },
        body: JSON.stringify({
          action: 'chat_send',
          text: text,
          dis: true,
          tag: originTag,
          guildId: String(d.guild_id || ''),
          user: 'DreamUser:' + name
        })
      });
    } catch (_) {}
  }

  // ---------------------------------------------------- DreamShare -> Discord
  async pollOutbound() {
    const cfg = this.cfg;
    const target = cfg.relayChannel || this.env.DISCORD_RELAY_CHANNEL || '';
    let feed = null;
    try {
      const res = await fetch(String(this.env.DREAMSHARE_API || '').replace(/\/+$/, '') + '/');
      feed = await res.json();
    } catch (_) {
      return;
    }
    const chat = Array.isArray(feed && feed.chat) ? feed.chat : [];
    const fresh = chat.filter((m) => m && !m.dis && Number(m.at) > 0);
    if (fresh.length === 0) return;
    const newest = fresh[fresh.length - 1];

    // First sweep: remember where we are instead of replaying 100 old lines.
    if (this.cursor === null) {
      this.cursor = Number(newest.at) || 0;
      await this.state.storage.put('cursor', this.cursor);
      return;
    }

    let moved = false;
    for (const m of fresh) {
      const at = Number(m.at) || 0;
      if (at <= this.cursor) continue;
      this.cursor = at;
      moved = true;
      if (!target || !this.env.DISCORD_BOT_TOKEN) continue;
      const line = String(m.text || '').slice(0, 900);
      try { await discordPost(this.env, '/channels/' + target + '/messages', { content: '**' + String(m.user || '?') + '**: ' + line }); } catch (_) {}
    }
    if (moved) await this.state.storage.put('cursor', this.cursor);
  }

  // ---------------------------------------------------------- presence push
  async sweepPresence() {
    const base = String(this.env.DREAMSHARE_API || '').replace(/\/+$/, '');
    if (!base) return;
    try {
      await fetch(base + '/?op=discord_presence', {
        method: 'POST',
        headers: { 'content-type': 'application/json', 'x-dreamshare-bridge': this.env.DREAMSHARE_BRIDGE_KEY || '' },
        body: JSON.stringify({ users: this.onlineList() })
      });
    } catch (_) {}
  }

  async alarm() {
    try {
      await this.loadCfg();
      if (!this.ws || this.ws.readyState !== 1) this.connect();
      else if (Date.now() - this.lastBeat > this.heartbeatMs) this.beat();
      await this.pollOutbound();
      await this.sweepPresence();
    } catch (_) {}
    await this.state.storage.setAlarm(Date.now() + SWEEP_MS);
  }
}

// ------------------------------------------------------------------ commands
function subOptions(options) {
  const out = {};
  for (const o of options || []) out[o.name] = o.value !== undefined ? o.value : (o.options || []);
  return out;
}

async function handleInteraction(interaction, gateway, env) {
  const sub = (interaction.data && interaction.data.options && interaction.data.options[0]) || {};
  const name = sub.name || 'ping';
  const args = subOptions(sub.options);
  const owner = String(env.DISCORD_OWNER_ID || '');
  const who = (interaction.member && interaction.member.user && interaction.member.user.id) || (interaction.user && interaction.user.id) || '';

  if (!owner || String(who) !== owner) {
    return json({ type: 4, data: { content: 'Not allowed.', flags: 64 } });
  }

  const cfg = await gateway.getCfg();
  const call = (path, body) => gateway.fetch('https://do/' + path, body ? { method: 'POST', body: JSON.stringify(body) } : undefined);

  if (name === 'ping') {
    const status = await (await call('status')).json();
    return json({ type: 4, data: { content: 'Bridge online. Discord socket: ' + (status.ready ? 'connected' : 'reconnecting') + '.', flags: 64 } });
  }
  if (name === 'status') {
    const status = await (await call('status')).json();
    return json({
      type: 4,
      data: {
        content: [
          '**DreamShare bridge**',
          'socket: ' + (status.ready ? 'connected' : 'reconnecting'),
          'guild members cached: ' + status.members,
          'discord online: ' + status.online,
          'relay channels: ' + (status.cfg.channels.length ? status.cfg.channels.map((c) => '<#' + c + '>').join(' ') : 'none'),
          'relay roles: ' + (status.cfg.roles.length ? status.cfg.roles.join(' ') : 'everyone'),
          'chat -> discord: ' + (status.cfg.relayChannel ? '<#' + status.cfg.relayChannel + '>' : 'off')
        ].join('\n'),
        flags: 64
      }
    });
  }
  if (name === 'channel') {
    const id = String(args.channel || '').trim();
    const on = String(args.state || 'on') !== 'off';
    let channels = cfg.channels.slice();
    if (!id) {
      await call('config', { channels: [] });
      return json({ type: 4, data: { content: 'Relay channels cleared.', flags: 64 } });
    }
    if (on) { if (channels.indexOf(id) < 0) channels.push(id); }
    else channels = channels.filter((c) => c !== id);
    const saved = await (await call('config', { channels })).json();
    return json({ type: 4, data: { content: 'Relay channels: ' + (saved.cfg.channels.length ? saved.cfg.channels.map((c) => '<#' + c + '>').join(' ') : 'none'), flags: 64 } });
  }
  if (name === 'role') {
    const id = String(args.role || '').trim();
    const on = String(args.state || 'on') !== 'off';
    let roles = cfg.roles.slice();
    if (!id) {
      await call('config', { roles: [] });
      return json({ type: 4, data: { content: 'Relay roles cleared (everyone allowed).', flags: 64 } });
    }
    if (on) { if (roles.indexOf(id) < 0) roles.push(id); }
    else roles = roles.filter((r) => r !== id);
    const saved = await (await call('config', { roles })).json();
    return json({ type: 4, data: { content: 'Relay roles: ' + (saved.cfg.roles.length ? saved.cfg.roles.map((r) => '<@&' + r + '>').join(' ') : 'everyone'), flags: 64 } });
  }
  if (name === 'link') {
    const dreamName = String(args.name || '').trim().slice(0, 48);
    if (!dreamName) return json({ type: 4, data: { content: 'Give a DreamShare name.', flags: 64 } });
    const saved = await (await call('config', { link: { id: who, name: dreamName } })).json();
    return json({ type: 4, data: { content: 'You are `DreamUser:' + dreamName + ':`.', flags: 64 } });
  }
  if (name === 'say') {
    const text = String(args.text || '').trim().slice(0, 380);
    if (!text) return json({ type: 4, data: { content: 'Nothing to send.', flags: 64 } });
    const linked = cfg.links[who] || 'ktrippah';
    const base = String(env.DREAMSHARE_API || '').replace(/\/+$/, '');
    const res = await fetch(base + '/', {
      method: 'POST',
      headers: { 'content-type': 'application/json', 'x-dreamshare-bridge': String(env.DREAMSHARE_BRIDGE_KEY || '') },
      body: JSON.stringify({ action: 'chat_send', text, dis: true, tag: originTagForGuild(env.DISCORD_GUILD_ID), user: 'DreamUser:' + linked })
    }).catch(() => null);
    const ok = res ? (await res.json().catch(() => ({ ok: false }))).ok : false;
    return json({ type: 4, data: { content: ok ? 'Posted to DreamShare chat.' : 'DreamShare API did not accept it.', flags: 64 } });
  }
  if (name === 'online') {
    const status = await (await call('online')).json();
    const list = status.online || [];
    const lines = list.slice(0, 40).map((u) => 'DIS  ' + u.name);
    const body = [
      '**Online in Discord (' + list.length + ')**',
      lines.length ? lines.join('\n') : 'nobody'
    ].join('\n');
    return json({ type: 4, data: { content: body.slice(0, 1900), flags: 64 } });
  }
  return json({ type: 4, data: { content: 'Unknown command.', flags: 64 } });
}

function commandDefinition() {
  const roleOpt = { type: 3, name: 'role', description: 'Discord role id', required: true };
  const stateOpt = { type: 3, name: 'state', description: 'on or off', choices: [{ name: 'on', value: 'on' }, { name: 'off', value: 'off' }] };
  return {
    name: 'dream',
    description: 'DreamShare bridge controls (owner only)',
    options: [
      { type: 1, name: 'ping', description: 'Is the bridge connected?' },
      { type: 1, name: 'status', description: 'Bridge + relay configuration' },
      {
        type: 1, name: 'channel', description: 'Allow or block relaying for a channel',
        options: [{ type: 7, name: 'channel', description: 'Channel', required: true }, stateOpt]
      },
      { type: 1, name: 'role', description: 'Only relay members with this role', options: [roleOpt, stateOpt] },
      { type: 1, name: 'link', description: 'Link your Discord account to a DreamShare name', options: [{ type: 3, name: 'name', description: 'DreamShare username', required: true }] },
      { type: 1, name: 'say', description: 'Post a line into DreamShare live chat', options: [{ type: 3, name: 'text', description: 'Message', required: true }] },
      { type: 1, name: 'online', description: 'Who is online in Discord' }
    ]
  };
}

// -------------------------------------------------------------------- worker
export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname.replace(/\/+$/, '') || '/';
    const method = request.method.toUpperCase();
    const gateway = env.GATEWAY.get(env.GATEWAY.idFromName('main'));
    const doFetch = (cmd, body) => gateway.fetch('https://do/' + cmd, body ? { method: 'POST', body: JSON.stringify(body) } : undefined);
    const authed = (request.headers.get('x-bridge-key') || '') === String(env.DREAMSHARE_BRIDGE_KEY || '') && !!env.DREAMSHARE_BRIDGE_KEY;

    if (method === 'OPTIONS') return new Response(null, { status: 204, headers: { 'access-control-allow-origin': '*' } });

    if (path === '/' && method === 'GET') {
      return json({ ok: true, service: 'dreamshare-discord-bridge', api: env.DREAMSHARE_API || null, owner: env.DISCORD_OWNER_ID || null });
    }

    // Presence list for anything that wants it (the VST reads it through DreamShare).
    if (path === '/online' && method === 'GET') {
      const res = await doFetch('online');
      return new Response(res.body, { status: res.status, headers: { 'content-type': 'application/json', 'access-control-allow-origin': '*' } });
    }

    // Bridge control: {"cmd":"start"|"stop"|"status"|"config", ...}
    if (path === '/' && method === 'POST') {
      if (!authed) return json({ ok: false, error: 'bridge key required' }, 401);
      const body = await request.json().catch(() => ({}));
      const cmd = String(body.cmd || 'status');
      if (cmd === 'register') {
        const guild = String(env.DISCORD_GUILD_ID || '');
        const target = guild
          ? '/applications/' + env.DISCORD_APP_ID + '/guilds/' + guild + '/commands'
          : '/applications/' + env.DISCORD_APP_ID + '/commands';
        const res = await fetch(DISCORD_API + target, { method: 'PUT', headers: botHeaders(env), body: JSON.stringify([commandDefinition()]) });
        return json({ ok: res.ok, status: res.status, body: await res.text() });
      }
      const res = await doFetch(cmd, body);
      return new Response(res.body, { status: res.status, headers: { 'content-type': 'application/json' } });
    }

    // Discord slash commands.
    if (path === '/interactions' && method === 'POST') {
      const raw = await request.text();
      if (!await verifyInteraction(request, raw, env.DISCORD_PUBLIC_KEY || '')) {
        return new Response('invalid request signature', { status: 401 });
      }
      const interaction = JSON.parse(raw || '{}');
      if (interaction.type === 1) return json({ type: 1 });           // PING
      if (interaction.type === 2) return handleInteraction(interaction, gateway, env);
      return json({ type: 4, data: { content: 'Unsupported interaction.', flags: 64 } });
    }

    return json({ ok: false, error: 'not found' }, 404);
  }
};
