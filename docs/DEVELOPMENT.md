# Developer notes

Internal notes for people working on Halo: Co Op Evolved. Players only need [README.md](../README.md).

## Branch

- **main** is the only branch. It is what players download, so only finished features tested in MCC go into it.
  Keep work in progress uncommitted, or in a short-lived branch of your own that is deleted after merging.

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
  NVENC and sends it to its player. It also draws name tags, armor colours, player 1 full screen (F6), direct
  mouse aim and skins.
- **Client (`src/client`)**: decodes the video with NVDEC, plays the audio and sends controller or keyboard/mouse
  input.
- **Protocol (`src/common/protocol.h`)**: version 5. Bump `k_version` whenever a message changes size.

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
- Model tag (`mod2`): +0x04 node-list checksum, nodes block at +0xB8. Chief has 19 nodes, marine / armored marine /
  crewman 20 (checksum `9F34D329`, ponytail at node 16), pilot 21 (adds a frame visor), Elite 26.
- **Tag lookup** `halo1.dll+0xA9B648` (`tag_get`): index = (int16)ecx; returns the tag's data address. Everything
  that needs an object's definition does `ecx = object+0; call tag_get` and then reads the model at data+0x34 —
  the render thread about 4000 times per second. So the model is **not** cached: it is looked up every frame.

### Spawning
- `halo1.dll+0xAD2404` sets or clears a player's unit (called on death and on spawn).
- `halo1.dll+0xAD4184` creates a player's unit (it calls the one above when spawning); `+0xAD03C0` is the
  respawn loop that calls it. One of its first four arguments identifies the player: low 16 bits = player index,
  high 16 bits = the salt at the start of the player record. In a30 it is retried every ~33 ms for player 4 for
  about 9 s at mission start (no room in the drop pod).
- Bodies placed by a mission's opening cutscene (a30, the drop pod) are created by another path, not `+0xAD4184`.
- When an object is created the game sizes its per-node buffers for the model it has **at that moment** (offsets
  at object +0x1A4…+0x1AC: 0x260 = 19 nodes × 0x20 for Chief, 0x280 for a 20-node marine). They are never resized.

## Skins

Each player looks like another character but keeps Master Chief's hitbox, speed, jump and physics. The player picks
it in the client (menu and F2) or, for player 1, in the host panel. Protocol v5: `PlayerOptions.skin` (client →
host), `VideoConfig.skins[4]` (host → mod), `VideoStatus.skins_available` (mod → host, bit i = skin i is in the
mission) and `Pong.skins_available` (host → clients, to grey out missing ones). The list is `src/common/skins.h`.

How it works (`src/game/skins.cpp`):
1. When a mission loads, find Chief's biped and every skin's biped by name; read their model and animation
   references. Skins marked "marine animations" (crewman, Keyes, Jenkins: their own animation graphs lack weapon
   animations) use the marine's graph, but only if their model's node checksum equals the marine's.
2. The first time someone uses a skin: copy the tag array into a bigger buffer of our own, with 16 extra entries
   (index `count + 1 + skin`) that copy Chief's entry; make one **clone of Chief's biped data per skin** (0x1000
   bytes, in our own `VirtualAlloc` memory) with that skin's model and animation references; swap the pointer at
   `+0x1C34FB0` to our array.
3. A hand-written machine-code hook at the start of `tag_get` (touches only rax, rdx and flags, like the original)
   returns the clone for those 16 indices; the extra entries still point to Chief's data, as a fallback for anything
   that reads the array directly.
4. A MinHook on `+0xAD4184`: while it creates the body of a player with a skin, `tag_get` for **Chief's** index
   returns that skin's clone, so the body is **born** with the skin (right node buffers). Right after, the body's
   object+0 is set to the clone's tag ID so it keeps the skin every frame for the rest of its life.
5. Picking another skin only changes what the next body is born with. Nothing is freed: other game threads may be
   reading it.

Findings that shaped this:
- Swapping Chief's tag model around the spawn call does not work (the model is read every frame), and swapping the
  whole biped brings the other character's size, speed and limits (rejected).
- Clones placed near the game's tag data crashed: the game frees that range. Keep them in our own memory.
- **A body must be born with its skin.** Retagging a body born as Chief gives T-pose on its first appearance and,
  after standing still, the hand holding the weapon drifts (the 20th node has no buffer). Hence: a skin picked
  mid-game shows on the next respawn, and players placed by an opening cutscene start as Master Chief.
- Putting another mesh on Chief's skeleton is impossible: vertex bone indices are already in GPU buffers.
- Pilot was dropped: its own animations crash the game (`+0xAFE7B4`, no weapon animations) and its 21-node
  skeleton does not fit the marine's.
- Tested in MCC in a30 (respawn and mid-game change). Not tested yet: Flood marine, Keyes, Jenkins, checkpoint
  revert, level change.

| Skin | Biped tag | Animations | Missions |
|---|---|---|---|
| Master Chief | `characters\cyborg\cyborg` | own | all |
| Marine | `characters\marine\marine` | own | a10 a30 a50 b30 b40 c10 |
| Armored marine | `characters\marine_armored\marine_armored` | own | a10 a30 a50 b30 b40 c10 |
| Crewman | `characters\crewman\crewman` | marine | a10 a30 |
| Captain Keyes (fatigues) | `characters\captain_ingame\captain_ingame` | marine | a50 c10 |
| Captain Keyes (dress) | `characters\captain\captain` | marine | a10 a50 c10 d40 |
| Jenkins | `characters\marine_cinematic\jenkins\jenkins` | marine | c10 |
| Flood marine | `characters\floodcombat_human\floodcombat_human` | own | c10 c20 c40 d20 d40 |
| Elite | `characters\elite\elite` | own | a10 a30 a50 b30 b40 c40 d20 d40 |

Ideas not done: Sergeant Johnson (marine_armored with Johnson's face permutation); bodies placed by opening
cutscenes born with the skin (needs that creation path).

## Other ideas (not started)
- Own mod instead of AlphaRing (feasibility study first; AlphaRing has no licence, so nothing can be copied).
- Spawn only as many players as are connected (needs control of the local player count; easier without
  AlphaRing).
