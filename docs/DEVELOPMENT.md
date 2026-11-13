# Developer notes

Internal notes for people working on Halo: Co Op Evolved. Players only need [README.md](../README.md).

## Branch

- **main** is the only branch. It is what players download, so only finished features tested in MCC go into it.
  Keep work in progress uncommitted, or in a short-lived branch of your own that is deleted after merging.

## Building and testing

- Build: run `build.cmd` from the repository root (Visual Studio 2022 C++ build tools). It updates
  `CoOpEvolved\CoOpEvolved.exe` and `CoOpEvolved\VERSION.dll`. Close CoOpEvolved.exe first, or the copy fails.
- Test without MCC: `build\CoOpEvolvedTestGame.exe <path to VERSION.dll>` draws four coloured quadrants and loads
  the mod like MCC would. With `COOPEVOLVED_NO_INSTALL=1` the host does not touch the game folder, and with
  `COOPEVOLVED_AUTOCONNECT=1` the client connects with its saved settings. `--host` / `--join` pick the mode.
- Test in MCC: close MCC, open CoOpEvolved.exe as host (it installs the mod into `mcc\binaries\win64`), Open MCC
  → *Anti-Cheat Disabled*. A second CoOpEvolved.exe opens as client while the host runs (IP `127.0.0.1`).
- Logs: `coopevolved-host.log` and `coopevolved-cliente.log` next to CoOpEvolved.exe; `coopevolved-juego.log` in
  `mcc\binaries\win64`. They are git-ignored: never commit logs or `.ini` files (they contain IPs and names).

## Project rules

- Nothing personal in the repository: no IPs, local paths, Steam IDs or e-mails. `build.cmd` trims source paths
  from the binaries (`/d1trimfile`).
- User-facing text: English by default with a Spanish option (`T("en", "es")`). Player documentation lives in
  `README.md` / `README.es.md` and in `CoOpEvolved\` (`README.txt` / `LEEME.txt`).
- Everything found by reverse engineering is for **MCC Steam 1.3528**. Code that patches or hooks the game checks
  the bytes (or the module's PE timestamp) first and turns itself off if they do not match.
- In the old ViGEmBus + AlphaRing fallback, never write AlphaRing's profiles or options and never change how the
  virtual pads are created (that broke movement in v1; the pads must exist before MCC starts).

## Interface look

- Halo-launcher style shared by host and client: `src/common/ui_halo.h` / `src/app/ui_halo.cpp` (background with
  hexagons, panels with cut corners, buttons) and the palette in `src/common/ui_style.h`.
- The Halo font is **not** distributed (its licence is unclear). Fixed texts (titles, labels, buttons) are images:
  `tools/make_ui_images.py` renders them in English and Spanish with the font installed on the developer's PC and
  writes `src/app/ui_text.png`, `src/app/logo.png` (from `docs/logo.png`) and `src/common/ui_text_atlas.h`. To add a
  text, add it to `TEXTS` in the script, run it (needs Pillow and the font) and build. The font has no accented
  letters or ñ: the script draws the accent over the plain letter.
- Text that changes (names, IPs, numbers, statuses) and long text use Segoe UI.
- The mode and language tabs (`ui_mode.h`, `ui_halo_tab`) use Bahnschrift, loaded at run time from the Windows fonts
  folder (falls back to Segoe UI bold). It is a variable font and Dear ImGui only loads its regular weight, so the
  bold look is drawn twice 1 px apart, with extra letter spacing. Buttons keep the Halo-font images (the owner
  preferred them).
- `src/app/app.rc` embeds the icon (`app.ico`, from `icon.png`) and the two PNGs (RCDATA 101 and 102).

## How it works

- **CoOpEvolved.exe (`src/app`)**: one program with two modes. `host_main` (`src/launcher`) and `client_main`
  (`src/client`) are the old host and client; switching mode saves it in `coopevolved.ini` and relaunches the
  exe with `--host` / `--join`, so each mode starts with clean threads and sockets. A named mutex
  (`CoOpEvolvedHost.SingleInstance`) keeps one host per PC: a second host window opens inactive, and a new
  instance opens as client while a host is running.
- **Host mode**: receives each friend's controller over UDP 27960 and forwards it at once (and every 50 ms) to the
  game mod as `proto::Pads` on 127.0.0.1:27961; captures MCC's audio (Opus) and shows the panel. It also sends the
  mod where to send video and the players' names, colours, skins and options (`VideoConfig`), and writes
  `coopevolved-juego.ini` (`mandos=direct|vigem`) next to the installed mod.
- **Game mod (`src/game`, built as `VERSION.dll`)**: sets up 4 local players and the network controllers
  (`local_players.cpp`), hooks `IDXGISwapChain::Present`, encodes each quadrant with NVENC and sends it to its
  player (the whole screen during cutscenes). It also draws name tags, armor colours, player 1 full screen (F6),
  direct mouse aim and skins.
- **Client mode**: decodes the video with NVDEC, plays the audio and sends controller or keyboard/mouse input.
- **Protocol (`src/common/protocol.h`)**: version 6. Bump `k_version` whenever a client↔host message changes size.

## What we know about MCC-Win64-Shipping.exe (Steam 1.3528)

PE timestamp `0x68A0E5D7` (checked before hooking). Offsets are RVAs of the exe. They come from AlphaRing's
1.3528 fork (wouter51, MIT; see THIRD-PARTY-NOTICES.md) and were checked against this build.

- **Input**: MCC reads controllers only through `XINPUT1_3.dll` (imports by ordinal 2 = XInputGetState and
  3 = XInputSetState). Hooking those two in that module is enough to give MCC controllers that do not exist:
  slots that answer `ERROR_SUCCESS` are registered as pads. Our slots 0-2 are players 2-4; slot 3 forwards the
  host's real slot 0.
- **Game manager** (the object MCC hands to each game dll): a static object at `+0x3F7B190` (its vtable pointer
  is at +0; it is in `.bss`, so it is zero until MCC's start-up code builds it). Vtable entries used:
  +0x110 get_player_profile(xuid), +0x120 get_key_state(index, input 0x130 bytes), +0x138 set_vibration(index,
  XINPUT_VIBRATION*), +0x2C0 get_xbox_user_id(id*, name*, size in bytes, index), +0x3A0 get_gamepad_mapping(xuid).
  Also +0x148/+0x150 network_send, +0x158 network_recv (not used).
- **Device manager**: pointer at `+0x4001B78`. +0x00 vtable (+0x38 update_state), +0x08 devices[5] (4 pads +
  keyboard/mouse), +0xC8 last QueryPerformanceCounter. Device: +0x00 vtable (+0x38 set_state(device, dt, input),
  +0x40 check), +0x808 XInput slot, +0x91C XINPUT_STATE.
- `+0x5A16AC` delta time from a QPC difference (prologue `40 53 48 83 EC 20 48 8B D9`);
  `+0x38A09C` get_index_by_xuid (prologue `48 8B C4 55 57 41 54 41 56 41 57`).
- **Local players** (`local_players.cpp`, same idea as AlphaRing with its defaults): get_xbox_user_id answers
  "signed in" for indices 1..count-1 with fake xuids (and the friends' names), where count = J1 + friends up to the
  highest connected player number (4 if the host app is not running). MCC calls get_xbox_user_id **only when a
  mission is started from the menu** (a burst of ~40 calls), not during play nor on restart from the pause menu,
  so the count is fixed per mission and late joiners wait for the next start from the menu; get_key_state fills index 0 from the
  keyboard/mouse device and 1-3 from pad devices 0-2; profile and gamepad mapping are always player 1's;
  get_index_by_xuid maps the fake xuids to 1-3. If `WTSAPI32.dll` (AlphaRing) is loaded from the game folder,
  none of this is installed (only the XInput hook).
- **Subtitles** are drawn by MCC's interface, which is Unreal Engine (`UMCCSubtitlesWidget`,
  `UMCCSubtitlesViewModel`, `SubtitleManager`, `OnQueueSubtitles`). The texts come from
  `Data\UI\Localization\EN_Halo1.bin` (UTF-8, `cor&idCORTANA` + U+E900 + line; a sorted table of 32-bit hashes,
  not of the dialog sound names). MCC converts them to UTF-16 at load and the line on screen lives in UI objects that
  move around the heap, so drawing each line per player would need Unreal reflection work. Not done.
- **Hiding them** works through halo1.dll (`subtitles.cpp`): `halo1+0x47B180` reads the option
  `GameOpt.Subtitles` into `options+0x50` (prologue `40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF`); its caller, when
  the value is 0, calls `+0x47B230`, which sends `HideAll` to the subtitle interface. The mod hooks `+0x47B180` and
  writes 0 when the host ticks *Hide MCC subtitles* (`VideoConfig.hide_subtitles`). It hooks `KernelBase!LoadLibraryExW`
  to catch every load of halo1.dll before MCC uses it (and polls as a fallback).
- `%USERPROFILE%\AppData\LocalLow\MCC\Config\Halo1\UserConfigs\user.cfg` (`GameOpt { Subtitles = 1 }`) is only
  a copy: MCC rewrites it from its own profile settings when it starts and when a mission starts. Never edit it.

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

### 4-player patches (`local_players.cpp`, from AlphaRing)
PE timestamp of halo1.dll 1.3528: `0x68A0E4CF`. Applied on every load of halo1.dll (it reloads when a mission
starts), only if all three match the original bytes:
- `+0x67492` `75 11` → `EB 18`: more than 2 local players.
- `+0x427978` `77` → `EB`: pause with more players.
- `+0x85428` `0F 84 15 01 00 00` → 6 × `90`: fixes the freeze when a level ends (AlphaRing issue #19).

### Split layout (`split_layout.h`)
- `halo1.dll+0x1B7B910` (int16) = local players of the running mission. Seen in MCC: 2 players = full-width top
  and bottom halves; 3 = J1 full-width top half, J2 bottom left, J3 bottom right; 4 = quarters.
- In the wide views Halo keeps the **vertical** FOV of a 4-player quarter and widens the horizontal one, so name
  tags project with tan(vertical) = tan(horizontal FOV / 2) / screen aspect.
- Video per friend = their rectangle (J2 with 2 players is a full-width strip); friends not in the mission get the
  whole screen scaled down (VideoStatus.local_players → Pong.watching → notice in the client).

### halo1.dll reloads
- halo1.dll is unloaded and loaded again every time a mission starts from the MCC menu, often at the same address.
  Code patches and MinHook hooks vanish with it (MinHook still thinks they are on). `local_players.cpp` checks the
  patch bytes every 20 ms; `skins.cpp` re-hooks (MH_RemoveHook + create again) when the tag lookup has its original
  bytes again.

### Cutscenes (`cinematic.cpp`)
- `halo1.dll+0x1B87A20` = 1 while a cutscene plays and 0 in gameplay; `+0x1B87A21` is its opposite. Found by
  comparing memory snapshots with and without the a30 drop-pod cutscene and then watching candidates live.
- During cutscenes Halo draws a single full-screen view, so the mod copies the back buffer, scales it down to a
  quadrant's size (`blit.cpp`) and sends that to every friend; player 1 full screen (F6) is skipped.

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
- Spawn only as many players as are connected (get_xbox_user_id decides how many local players MCC creates).
- One window with host and client tabs instead of relaunching the exe.
