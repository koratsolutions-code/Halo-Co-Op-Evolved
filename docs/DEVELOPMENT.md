# Developer notes

Internal notes for people working on Halo: Co Op Evolved. Players only need [README.md](../README.md).

## Branches

- **main**: stable. This is what players download. Changes reach it only from `develop`, once tested in MCC.
- **develop**: work in progress. It may contain diagnostics and features that are not tested yet.

## Building and testing

- Build: run `build.cmd` from the repository root (Visual Studio 2022 C++ build tools). It updates
  `host\CoOpEvolvedHost.exe`, `host\VERSION.dll` and `cliente\CoOpEvolvedClient.exe`. Close the host and the
  client first, or the copy fails.
- Test without MCC: `build\CoOpEvolvedTestGame.exe <path to VERSION.dll>` draws four coloured quadrants and loads
  the mod like MCC would. With `COOPEVOLVED_NO_INSTALL=1` the host does not touch the game folder, and with
  `COOPEVOLVED_AUTOCONNECT=1` the client connects with its saved settings.
- Test in MCC: close MCC, install the mod (host panel → *Install mod in the game*, or copy `host\VERSION.dll` to
  `mcc\binaries\win64` in the game folder), open MCC without anti-cheat, AlphaRing → Players = 4.
- Logs: `coopevolved-host.log` and `coopevolved-cliente.log` next to each exe; `coopevolved-juego.log` in
  `mcc\binaries\win64`. They are git-ignored: never commit logs or `.ini` files (they contain IPs and names).

## Project rules

- **Never write AlphaRing's profiles or options** and never change how the virtual pads are created. Doing that in
  v1 broke player movement. The virtual pads must exist **before** MCC starts.
- Nothing personal in the repository: no IPs, local paths, Steam IDs or e-mails. `build.cmd` trims source paths
  from the binaries (`/d1trimfile`).
- User-facing text: English by default with a Spanish option (`T("en", "es")`). Player documentation lives in
  `README.md` / `README.es.md` and in `host\` and `cliente\` (`README.txt` / `LEEME.txt`).
- Everything found by reverse engineering is for **MCC Steam 1.3528**. Code that patches or hooks the game checks
  the bytes first and turns itself off if they do not match.

## How it works

- **Host (`src/launcher`)**: creates the virtual pads for players 2–4 (ViGEmBus) before opening MCC, receives
  their input over UDP, captures MCC's audio (Opus) and shows the panel. It tells the game mod over
  127.0.0.1:27961 where to send video and the players' names, colours and options.
- **Game mod (`src/game`, built as `VERSION.dll`)**: hooks `IDXGISwapChain::Present`, encodes each quadrant with
  NVENC and sends it to its player. It also draws name tags, armor colours, player 1 full screen (F6) and direct
  mouse aim.
- **Client (`src/client`)**: decodes the video with NVDEC, plays the audio and sends controller or keyboard/mouse
  input.
- **Protocol (`src/common/protocol.h`)**: version 4. Bump `k_version` whenever a message changes size.

## What we know about halo1.dll (MCC Steam 1.3528)

RVAs are relative to `halo1.dll`, which loads when Halo CE is opened from the MCC menu.

### Players and objects (`halo1_players.cpp`)
- The `players` and `object` data arrays are found by their signature `d@t@` (header +0x28). There are several
  copies; the live one is the one whose counter at player record +0x74 keeps changing.
- Player record: 0xC20 bytes. +0x00 salt (16 bits), +0x64 handle of the unit (biped) it controls.
- Object entry: 12 bytes, +8 offset into the object pool (object header + 0x38 + max × 12 + 0x34).
- Object data: +0x00 tag ID of its definition. Biped: +0x18 position, +0x5C body centre, +0x210 look vector,
  +0x174 change colours (4 RGB floats; what armor colours write).
- Local player count: `halo1.dll+0x1B7B910`.

### Cameras (`aim.cpp`)
- Real per-player camera: one structure per player, **0x58 bytes apart**: float yaw (radians, [0, 2π), grows to
  the left) then float pitch (up is +). Found by matching each player's look vector. Writing there turns the
  camera.
- A similar block with players **0x30 apart** is only a copy that the game rewrites every frame.

### Tags (`skins.cpp`)
- `*(halo1.dll+0x1C34FB0)` = tag array; `*(halo1.dll+0x2B22F50)` = tag data block (array = block + 0x28).
- Tag entry: 0x20 bytes. +0x00 class (`'bipd'` read as an integer), +0x0C tag ID (low 16 bits = index),
  +0x10 name pointer, +0x14 data pointer. 32-bit tag pointers resolve as `block + (pointer - 0x50000000)`.
- Tag reference inside tag data: class, name pointer, 0, tag ID (16 bytes).
- Biped tag data: +0x28 model reference (`'mod2'`); the animation reference (`'antr'`) is found by scanning.

### Spawning
- `halo1.dll+0xAD2404` sets or clears a player's unit (called on death and on spawn).
- `halo1.dll+0xAD4184` creates a player's unit (it calls the one above when spawning); `+0xAD03C0` is the
  respawn loop that calls it.
- The game reads a biped's model and animations **when the object is created**, not afterwards.

## Skins (in progress)

Goal: each player looks like another character but keeps Master Chief's hitbox, speed, jump and physics.

- How: just before `+0xAD4184` creates player N, put the chosen character's model **and** animation references
  into Master Chief's biped tag, and restore them right after. Model and animations must change together: Chief's
  animations on another skeleton leave the model in T-pose.
- Swapping the whole biped (the globals player unit) was tried and rejected: the player gets the other
  character's size, speed and limits.
- A skin only exists in missions that load that character. No modified maps or external tools: if a skin is not in
  the mission, the player spawns as Master Chief.

| Skin | Biped tag | Missions |
|---|---|---|
| Master Chief | `characters\cyborg\cyborg` | all |
| Marine | `characters\marine\marine` | a10 a30 a50 b30 b40 c10 |
| Armored marine (tested) | `characters\marine_armored\marine_armored` | a10 a30 a50 b30 b40 c10 |
| Sergeant Johnson | marine_armored with Johnson's face permutation (extra work) | a10 a30 a50 b30 b40 c10 |
| Crewman | `characters\crewman\crewman` | a10 a30 |
| Pilot | `characters\pilot\pilot` | a10 a30 |
| Captain Keyes (fatigues) | `characters\captain_ingame\captain_ingame` | a50 c10 |
| Captain Keyes (dress) | `characters\captain\captain` | a10 a50 c10 d40 |
| Jenkins | `characters\marine_cinematic\jenkins\jenkins` | c10 |
| Flood marine | `characters\floodcombat_human\floodcombat_human` | c10 c20 c40 d20 d40 |
| Elite | `characters\elite\elite` | a10 a30 a50 b30 b40 c40 d20 d40 |

### Roadmap
1. ✅ Find the tag array instantly (pointers above).
2. ✅ Find the spawn function (hardware write watch on player record +0x64).
3. 🧪 Per-player skins at spawn: `skins.cpp` hooks `+0xAD4184`. **F10** toggles a test with fixed skins
   (J2 armored marine, J3 marine, J4 crewman). **Not yet tested in MCC.** It identifies the player from the
   arguments (low 16 bits = player index, high 16 bits = the salt at the start of the record) and logs them.
4. Choosing the skin: the client (menu and F2, next to armor colour) and the host panel for player 1. The host
   reports which skins the current mission has; missing ones fall back to Master Chief. Protocol v5.
5. Edge cases (checkpoint revert, cinematics, level change, other missions), remove the diagnostics, update the
   READMEs.

### Diagnostics in develop (remove before merging to main)
- **F9** (`character_probe.cpp`, read-only): lists the mission's biped tags and the halo1.dll pointers to the tag
  array.
- **F11** (`spawn_probe.cpp`): hardware write watch on player 2's unit handle; logs the writing instruction and
  call chain.

## Other ideas (not started)
- Own mod instead of AlphaRing (feasibility study first; AlphaRing has no licence, so nothing can be copied).
- Spawn only as many players as are connected (needs control of the local player count; easier without
  AlphaRing).
