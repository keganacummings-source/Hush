/**
 * Kyoto module/catalog rules for DreamShare.
 * Canonical worker entry point: ../worker.js
 *
 * The worker passes its environment/dependencies explicitly so this module
 * does not rely on cross-module globals.
 *
 * Approval flow:
 *   - module_publish auto-uploads with status "pending".
 *   - Super admins approve (→ "approved", visible to all) or deny (→ "denied",
 *     visible only to creator, their friends, and admins).
 *   - Only the original poster can edit/delete their own uploaded versions.
 *   - Tags can be assigned by admins via module_tag.
 */
async function handleKyotoModule(action, body, sess, env, deps) {
  const STORAGE = deps.STORAGE;
  const KYOTO_API_VERSION = deps.KYOTO_API_VERSION;
  const KYOTO_MODULE_FORMAT = deps.KYOTO_MODULE_FORMAT;
  const isSuper = deps.isSuper;
  const communityIndex = deps.communityIndex;
  const kv = env.DREAMSHARE_KV;
  if (!kv) return { ok: false, error: "DREAMSHARE_KV missing", code: "no-kv" };

  // --- helper: get friends list for a user ---
  async function getFriends(username) {
    if (!username) return [];
    try {
      const feed = JSON.parse((await kv.get("feed:main")) || "{}");
      const friends = feed.friends || {};
      return (friends[username.toLowerCase()] || []).map(s => String(s).toLowerCase());
    } catch (_) { return []; }
  }

  // --- helper: check if viewer is friend of owner ---
  async function isFriendOf(viewer, owner) {
    if (!viewer || !owner || viewer === owner) return false;
    const friends = await getFriends(owner);
    return friends.includes(viewer.toLowerCase());
  }

  // --- helper: filter modules by visibility ---
  async function filterVisible(list, viewer, viewerIsAdmin) {
    if (viewerIsAdmin) return list; // admins see everything
    const viewerLower = (viewer || "").toLowerCase();
    const result = [];
    for (const m of list) {
      const status = m.status || "approved"; // legacy items default to approved
      if (status === "approved") { result.push(m); continue; }
      // pending or denied: only creator, friends, admins
      if (m.author && m.author.toLowerCase() === viewerLower) { result.push(m); continue; }
      if (status === "denied" && await isFriendOf(viewer, m.author)) { result.push(m); continue; }
      // pending items: only creator sees them (admin already returned above)
    }
    return result;
  }

  if (action === "module_list" || action === "community" || action === "catalog") {
    const face = String(body.face || body.filter || "").toLowerCase();
    const tags = body.tags ? (Array.isArray(body.tags) ? body.tags : String(body.tags).split(",").map(s => s.trim().toLowerCase()).filter(Boolean)) : [];
    const tagQuery = body.tag ? String(body.tag).toLowerCase().trim() : "";
    if (tagQuery) tags.push(tagQuery);
    const viewerIsAdmin = isSuper(sess.user) || sess.role === "super" || sess.role === "mod";
    let list = [];
    try { list = JSON.parse((await kv.get("module-index")) || "[]"); } catch (_) { list = []; }
    if (!Array.isArray(list)) list = [];
    // Enrich index entries with status/tags from the full docs (best effort)
    const enriched = [];
    for (const m of list) {
      let status = m.status || "approved";
      let modTags = m.tags || [];
      // Try to get full doc for status/tags if not in index
      if (!m.status || !m.tags) {
        try {
          const doc = JSON.parse((await kv.get("module:" + m.id)) || "null");
          if (doc) { status = doc.status || status; modTags = doc.tags || modTags; }
        } catch (_) {}
      }
      enriched.push({ ...m, status, tags: modTags });
    }
    list = enriched;
    // Filter by face
    if (face) list = list.filter((m) => String(m.face || "").toLowerCase() === face);
    // Filter by tags
    if (tags.length > 0) list = list.filter(m => {
      const modTags = (m.tags || []).map(t => String(t).toLowerCase());
      return tags.some(t => modTags.includes(t));
    });
    // Merge community instruments (legacy)
    if (!face || face === "kyoto") {
      try {
        const ci = await communityIndex(env);
        for (const p of (ci || []).slice(0, 80)) {
          if (!list.some(x => x.id === p.id)) list.push({ id:p.id, name:p.name, face:"kyoto", author:p.author, at:p.updated||p.created||0, community:true, status:"approved", tags:[] });
        }
      } catch (_) {}
    }
    // Filter by visibility
    list = await filterVisible(list, sess.user, viewerIsAdmin);
    list = list.sort((a,b) => (b.at||0) - (a.at||0)).slice(0, 100);
    return { ok:true, modules:list, community:list, storage:STORAGE, kyotoApi:KYOTO_API_VERSION };
  }

  // --- User's own saved modules ---
  if (action === "module_my" || action === "my_modules") {
    const ownedKey = "user-modules:" + String(sess.user || "").toLowerCase();
    let owned = [];
    try { owned = JSON.parse((await kv.get(ownedKey)) || "[]"); } catch (_) { owned = []; }
    if (!Array.isArray(owned)) owned = [];
    let list = [];
    for (const id of owned) {
      try {
        const doc = JSON.parse((await kv.get("module:" + id)) || "null");
        if (doc) list.push({ id: doc.id, name: doc.name, face: doc.face, author: doc.author, at: doc.at, status: doc.status || "approved", tags: doc.tags || [] });
      } catch (_) {}
    }
    list = list.sort((a,b) => (b.at||0) - (a.at||0)).slice(0, 100);
    return { ok:true, modules:list, storage:STORAGE };
  }

  if (action === "module_get" || action === "community_get") {
    const id = String(body.id || body.community || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    if (!id) return { ok:false, error:"missing id" };
    const raw = await kv.get("module:" + id);
    if (!raw) {
      const ci = await kv.get("community-instrument:" + id, "json");
      if (ci) return { ok:true, module:ci, instrument:ci };
      return { ok:false, error:"not found" };
    }
    const module = JSON.parse(raw);
    // Visibility check: denied/pending only for creator, friends, admins
    const status = module.status || "approved";
    const viewerIsAdmin = isSuper(sess.user) || sess.role === "super" || sess.role === "mod";
    if (status !== "approved" && !viewerIsAdmin) {
      const isOwner = module.author && module.author.toLowerCase() === String(sess.user || "").toLowerCase();
      if (!isOwner && status === "denied") {
        const friend = await isFriendOf(sess.user, module.author);
        if (!friend) return { ok:false, error:"not found" };
      } else if (!isOwner) {
        return { ok:false, error:"not found" };
      }
    }
    try {
      const index = JSON.parse((await kv.get("module-index")) || "[]");
      const hit = index.find((m) => m && m.id === id);
      if (hit) { hit.downloads = (hit.downloads || 0) + 1; await kv.put("module-index", JSON.stringify(index.slice(-200))); }
    } catch (_) {}
    return { ok:true, module };
  }

  // --- Approve a pending module (super admin only) ---
  if (action === "module_approve" || action === "catalog_approve") {
    if (!isSuper(sess.user) || sess.role !== "super")
      return { ok:false, error:"super admin only" };
    const id = String(body.id || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    if (!id) return { ok:false, error:"missing id" };
    const raw = await kv.get("module:" + id);
    if (!raw) return { ok:false, error:"not found" };
    const doc = JSON.parse(raw);
    doc.status = "approved";
    doc.approvedBy = sess.user;
    doc.approvedAt = Date.now();
    await kv.put("module:" + id, JSON.stringify(doc));
    // Update index
    try {
      const index = JSON.parse((await kv.get("module-index")) || "[]");
      const hit = index.find(m => m && m.id === id);
      if (hit) { hit.status = "approved"; await kv.put("module-index", JSON.stringify(index.slice(-200))); }
    } catch (_) {}
    return { ok:true, id, status:"approved" };
  }

  // --- Deny a pending module (super admin only) ---
  if (action === "module_deny" || action === "catalog_deny") {
    if (!isSuper(sess.user) || sess.role !== "super")
      return { ok:false, error:"super admin only" };
    const id = String(body.id || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    if (!id) return { ok:false, error:"missing id" };
    const raw = await kv.get("module:" + id);
    if (!raw) return { ok:false, error:"not found" };
    const doc = JSON.parse(raw);
    doc.status = "denied";
    doc.deniedBy = sess.user;
    doc.deniedAt = Date.now();
    await kv.put("module:" + id, JSON.stringify(doc));
    try {
      const index = JSON.parse((await kv.get("module-index")) || "[]");
      const hit = index.find(m => m && m.id === id);
      if (hit) { hit.status = "denied"; await kv.put("module-index", JSON.stringify(index.slice(-200))); }
    } catch (_) {}
    return { ok:true, id, status:"denied" };
  }

  // --- Tag a module or user (admin/mod only) ---
  if (action === "module_tag" || action === "tag_add") {
    const viewerIsAdmin = isSuper(sess.user) || sess.role === "super" || sess.role === "mod";
    if (!viewerIsAdmin) return { ok:false, error:"admin only" };
    const id = String(body.id || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    const newTags = body.tags ? (Array.isArray(body.tags) ? body.tags : String(body.tags).split(",").map(s => s.trim())).filter(Boolean).map(s => s.slice(0, 24)) : [];
    if (!id || newTags.length === 0) return { ok:false, error:"missing id or tags" };
    const raw = await kv.get("module:" + id);
    if (!raw) return { ok:false, error:"not found" };
    const doc = JSON.parse(raw);
    doc.tags = doc.tags || [];
    for (const t of newTags) if (!doc.tags.includes(t)) doc.tags.push(t);
    doc.tags = doc.tags.slice(0, 12);
    await kv.put("module:" + id, JSON.stringify(doc));
    try {
      const index = JSON.parse((await kv.get("module-index")) || "[]");
      const hit = index.find(m => m && m.id === id);
      if (hit) { hit.tags = doc.tags; await kv.put("module-index", JSON.stringify(index.slice(-200))); }
    } catch (_) {}
    return { ok:true, id, tags: doc.tags };
  }

  // --- Remove a tag from a module (admin/mod only) ---
  if (action === "module_untag" || action === "tag_remove") {
    const viewerIsAdmin = isSuper(sess.user) || sess.role === "super" || sess.role === "mod";
    if (!viewerIsAdmin) return { ok:false, error:"admin only" };
    const id = String(body.id || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    const removeTag = String(body.tag || "").trim();
    if (!id || !removeTag) return { ok:false, error:"missing id or tag" };
    const raw = await kv.get("module:" + id);
    if (!raw) return { ok:false, error:"not found" };
    const doc = JSON.parse(raw);
    doc.tags = (doc.tags || []).filter(t => t !== removeTag);
    await kv.put("module:" + id, JSON.stringify(doc));
    try {
      const index = JSON.parse((await kv.get("module-index")) || "[]");
      const hit = index.find(m => m && m.id === id);
      if (hit) { hit.tags = doc.tags; await kv.put("module-index", JSON.stringify(index.slice(-200))); }
    } catch (_) {}
    return { ok:true, id, tags: doc.tags };
  }

  if (action === "module_delete" || action === "catalog_delete" || action === "module_remove") {
    const id = String(body.id || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 48);
    if (!id) return { ok:false, error:"missing id" };
    const raw = await kv.get("module:" + id);
    if (!raw) return { ok:false, error:"not found" };
    const doc = JSON.parse(raw);
    // Only super admins or the original poster can delete
    const isOwner = doc.author && doc.author.toLowerCase() === String(sess.user || "").toLowerCase();
    if (!isSuper(sess.user) && !isOwner) return { ok:false, error:"not authorized" };
    await kv.delete("module:" + id);
    let index = [];
    try { index = JSON.parse((await kv.get("module-index")) || "[]"); } catch (_) { index = []; }
    index = Array.isArray(index) ? index.filter(m => m && m.id !== id) : [];
    await kv.put("module-index", JSON.stringify(index.slice(-200)));
    try {
      const ci = await kv.get("community-instruments-v1", "json");
      if (Array.isArray(ci)) await kv.put("community-instruments-v1", JSON.stringify(ci.filter(x => x && x.id !== id)));
      await kv.delete("community-instrument:" + id);
    } catch (_) {}
    const ownerKey = "user-modules:" + String(doc.author || "").toLowerCase();
    if (ownerKey !== "user-modules:") {
      const owned = JSON.parse((await kv.get(ownerKey)) || "[]").filter(x => x !== id);
      await kv.put(ownerKey, JSON.stringify(owned.slice(-100)));
    }
    return { ok:true, deleted:id };
  }

  if (action === "module_publish" || action === "community_publish") {
    let mod = body.module;
    if (!mod && body.state) {
      mod = typeof body.state === "string" ? JSON.parse(body.state) : body.state;
      if (mod && typeof mod === "object") {
        mod.name = mod.name || body.name;
        mod.face = mod.face || "kyoto";
        mod.format = mod.format || KYOTO_MODULE_FORMAT;
      }
    }
    if (typeof mod === "string") { try { mod = JSON.parse(mod); } catch (_) { return {ok:false,error:"bad module json"}; } }
    if (!mod || typeof mod !== "object") return {ok:false,error:"module required"};
    const format = mod.format || KYOTO_MODULE_FORMAT;
    const okFormat = format === "kyoteppah-module-1" || format === "kyoteppah-effect-1";
    const face = String(mod.face || body.face || "kyoto").toLowerCase();
    const okFace = ["kyoto","fx","chain","effect"].includes(face);
    if (!okFormat || !okFace) return {ok:false,error:"bad module"};
    const id = Date.now().toString(36) + Math.random().toString(36).slice(2,6);
    const theme = String(mod.theme || body.theme || "trippah").toLowerCase().replace(/[^a-z0-9]/g, "").slice(0,32) || "trippah";
    const tags = Array.isArray(mod.tags) ? mod.tags.slice(0,12).map(t => String(t).slice(0,24)) : [];
    const doc = {
      format, id,
      name:String(mod.name || body.name || "untitled").replace(/[<>]/g,"").trim().slice(0,48)||"untitled",
      face, author:sess.user, theme, grid:Number(mod.grid)||0, free:!!mod.free,
      status: "pending", // auto-uploaded, pending admin approval
      tags,
      steps:Array.isArray(mod.steps)?mod.steps.slice(0,16):[],
      slots:Array.isArray(mod.slots)?mod.slots.slice(0,32):[],
      chainLevels: mod.chainLevels && typeof mod.chainLevels === "object" ? {
        enabled: mod.chainLevels.enabled === true,
        levels: Array.isArray(mod.chainLevels.levels) ? mod.chainLevels.levels.slice(0,33).map(v =>
          typeof v === "number" && Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : 1) : []
      } : null,
      widgets:Array.isArray(mod.widgets)?mod.widgets.slice(0,80):[],
      // Full machine reconstruction payload: playground/body/parts/connections/theme/macro data.
      machineDesign: (mod.machineDesign && typeof mod.machineDesign === "object") ? mod.machineDesign : null,
      instrument:face === "kyoto" ? (mod.instrument || null) : null,
      description:String(body.description||mod.description||"").replace(/[<>]/g,"").trim().slice(0,280),
      at:Date.now()
    };
    if (JSON.stringify(doc).length > 180000) return {ok:false,error:"too large"};
    await kv.put("module:"+id, JSON.stringify(doc));
    let index=[];
    try { index=JSON.parse((await kv.get("module-index"))||"[]"); } catch (_) { index=[]; }
    if (!Array.isArray(index)) index=[];
    index.push({id,name:doc.name,face:doc.face,author:doc.author,at:doc.at,downloads:0,status:"pending",tags});
    await kv.put("module-index", JSON.stringify(index.slice(-200)));
    const ownedKey="user-modules:"+String(sess.user||"").toLowerCase();
    const owned=JSON.parse((await kv.get(ownedKey))||"[]"); owned.push(id);
    await kv.put(ownedKey, JSON.stringify(owned.slice(-100)));
    return {ok:true,id,module:doc,status:"pending"};
  }

  // --- Pending queue for admins ---
  if (action === "module_pending" || action === "pending") {
    const viewerIsAdmin = isSuper(sess.user) || sess.role === "super" || sess.role === "mod";
    if (!viewerIsAdmin) return { ok:false, error:"admin only" };
    let list = [];
    try { list = JSON.parse((await kv.get("module-index")) || "[]"); } catch (_) { list = []; }
    if (!Array.isArray(list)) list = [];
    const pending = [];
    for (const m of list) {
      let status = m.status || "approved";
      if (!m.status) {
        try {
          const doc = JSON.parse((await kv.get("module:" + m.id)) || "null");
          if (doc) status = doc.status || status;
        } catch (_) {}
      }
      if (status === "pending") pending.push(m);
    }
    return { ok:true, modules: pending.sort((a,b) => (b.at||0) - (a.at||0)).slice(0, 50) };
  }

  return null;
}

export { handleKyotoModule };
