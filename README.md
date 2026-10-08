# KyotoVST

Modular FX builder in VST3 format (JUCE 8, C++17). You get 200 DSP units and 125 multi-stage machines (studio racks, stompboxes, tape & echo, space, modular, broken tech, alien tech). You wire them together into any series or parallel graph, then play the result in the full-screen **Viewer**. **DreamShare** (DreamAPI) and the **Discord bridge** are built in.

## Repository layout
```
CMakeLists.txt            one plugin target: KyotoVST (VST3, Fx)
.github/workflows/        CI build for Windows + macOS -> downloadable VST3 zips
Source/
  FxCatalog.h             the 200 FX (id, name, family)
  KtDspEngine.h/.cpp      DSP recipe for every FX
  Rack.h                  signal graph: nodes, wires, cycle-safe routing, compile + lock-free swap, JSON
  Machines.h              multi-stage machine recipes (one line each)
  PluginProcessor.*       host I/O, global params (in/out/mix/4 macros), state save
  Ui.h                    sci-fi console look & feel (fully theme driven)
  RackCanvas.h            patch bay + right-click designer
  Viewer.h                plugin-only machine view
  DreamPanel.h            DreamShare / Discord console
  PluginEditor.*          library, inspector, layout
  DreamApi.*, Attach.h    DreamAPI client (worker endpoint)
  Themes.h                theme palettes shared with the site
worker/                   DreamShare Cloudflare Worker (worker.js + module-rules.js)
discord/                  Discord bot / bridge worker
```

## Building
Push to GitHub and the Actions workflow builds `KyotoVST-Windows-VST3.zip` and `KyotoVST-macOS-VST3.zip`.

To build locally:
```
cmake -S . -B build && cmake --build build --config Release --target KyotoVST_VST3
```
To use a local JUCE checkout, add `-DKYOTO_JUCE_DIR=/path/to/JUCE`.

## Using it
- **Library** (left): search, or filter by family / machines. Double-click a unit, or drag it onto the patch bay. New units join the end of the main chain automatically.
- **Wiring**: drag from a right-hand jack to a module (or to OUT). A module can feed several others, and several modules can feed one (they are summed), so parallel branches work. Cycles are rejected.
  - Right-click a cable to delete it.
  - Press DEL to remove the selected module or cable.
- **Right-click designer**:
  - On a module: rename, faceplate colour, knob face (machined / LED ring / chicken-head), show in Viewer, bypass, duplicate, re-insert into the chain, disconnect, remove.
  - On empty space: add any unit or machine at that spot, auto-chain, show all in Viewer, reset faceplates, clear.
- **Inspector** (right): Amount / Tone / Motion / Shape / Mix / Level for the selected module. You can also assign its Amount to Macro 1-4 and set a depth.
- **Viewer**: hides the builder and shows only your machine, one faceplate per stage, with live controls.
- **Console** (bottom): Input, Mix, Output, and Macro 1-4. All of these can be automated from the DAW.
- **Export / Import**: `.kyoto` patch files.

## DreamShare (DreamAPI)
Sign in with your DreamShare account. Each channel uses the DreamAPI actions listed here:

| Channel | Actions |
|---|---|
| Lounge | chat_list, chat_send |
| Threads | list_threads, create_thread, comment, react, delete_thread |
| Catalog | module_list, module_get (load into rack), module_publish, module_tag, module_untag |
| My Modules | module_my, module_delete |
| Pending (mods) | module_pending, module_approve, module_deny |
| Cloud Presets | preset_list, preset_save, preset_delete (machine `kyotovst`) |
| Friends / DM | friends_list, friend_request, friend_accept, friend_remove, dm_list, dm_send |
| Discord | discord_channels, discord_messages, discord_send |
| Background | heartbeat (presence) |

The endpoint is set in `Source/DreamApi.cpp`. The Discord bot token stays on the worker and never ships in the plugin.

## Extending
- **New machine**: add one line to `Source/Machines.h`. Example: `"drive@0.6 > delay | plate"`.
- **New FX**: add an entry to `FxCatalog.h` and a recipe to `KtDspEngine.cpp`.
- **Patch format**: `kyotovst-patch-1` JSON (nodes + wires). Published modules carry it in `machineDesign`.
