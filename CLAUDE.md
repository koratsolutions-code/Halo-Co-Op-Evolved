# Halo: Co Op Evolved — handoff for Claude and developers

This file is the starting point for anyone (person or Claude) who continues the project. Claude Code reads it
automatically when it is opened in this folder. Technical details found by reverse engineering are in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md); player instructions are in [README.md](README.md).

## 1. Setup

Requirements:
- Windows 10/11 x64 and Git.
- **Visual Studio 2022** (Community or Build Tools) with the **Desktop development with C++** workload. It
  includes CMake and Ninja, which `build.cmd` uses. All other dependencies are already in `deps/`.
- An **NVIDIA GPU** (video uses NVENC/NVDEC).
- To test in the game: **Halo: The Master Chief Collection on Steam** (the code targets version 1.3528). Nothing
  else: the mod sets up the 4 players and the controllers by itself (no extra drivers).

```bash
git clone https://github.com/koratsolutions-code/Halo-Co-Op-Evolved.git
cd Halo-Co-Op-Evolved
# Your own name and e-mail for your commits in this repository:
git config user.name "your-github-user"
git config user.email "your-github-email"
```

Build (from `cmd` or a terminal, in the repository folder):

```bash
cmd /c build.cmd
```

It ends with `DONE:` and updates `CoOpEvolved\CoOpEvolved.exe` and `CoOpEvolved\VERSION.dll` (these binaries are
committed so players can download them). If it says `Could not copy`, close CoOpEvolved.exe: it also loads the
`VERSION.dll` next to it.

Test without MCC (in a terminal, from the repository folder):

```bash
build\CoOpEvolvedTestGame.exe "%CD%\build\VERSION.dll"
```

Then open `CoOpEvolved\CoOpEvolved.exe --host` and `CoOpEvolved\CoOpEvolved.exe --join` (IP `127.0.0.1`). The
test game draws four coloured quadrants; the client must show one of them. Environment variables for tests:
`COOPEVOLVED_NO_INSTALL=1` (the host does not touch the game folder), `COOPEVOLVED_AUTOCONNECT=1` (the client
connects with its saved settings).

Test in MCC: **close MCC**, open `CoOpEvolved.exe` as host (it installs `VERSION.dll` into
`<MCC folder>\mcc\binaries\win64\`), then Open MCC → *Anti-Cheat Disabled*, Halo CE with classic graphics (Tab).
A second `CoOpEvolved.exe` opens as client by itself while a host is running.

Logs (git-ignored, overwritten every launch): `CoOpEvolved\coopevolved-host.log`,
`CoOpEvolved\coopevolved-cliente.log`, `<MCC folder>\mcc\binaries\win64\coopevolved-juego.log`.

## 2. Branch and workflow

- **main** is the only branch, and it is what players download (the `CoOpEvolved\` binaries included).
- Commit to main only finished features that were tested in MCC; keep work in progress uncommitted, or in a
  short-lived branch of your own that is deleted after merging.
- Before every commit: build, test, and check that no personal data got in (see rules below).

## 3. What the project is

Four people play the Halo CE campaign in MCC, each on their own PC with their own full screen, over Radmin VPN.
MCC runs only on the host's PC with a 4-player split screen. The mod sends each friend only their quarter of the
screen as low-latency video plus the game sound, and their controller or keyboard/mouse comes back to the game.
Lowest latency is the top priority.

- `src/app/main.cpp` → **CoOpEvolved.exe**: one program. Opens in the last mode used (`coopevolved.ini`); the
  "Host a game / Join a game" selector (`src/common/ui_mode.h`) relaunches it in the other mode. `--host` /
  `--join` force a mode. If a host is already running on the PC, a new instance opens as client; a second host
  window opens inactive (greyed out).
- `src/launcher` (host mode, `host_main`): receives input over UDP 27960 and forwards it to the game mod, captures
  MCC's audio, ImGui panel (players, names, armor colours, skins, options), installs `VERSION.dll`.
- `src/client` (join mode, `client_main`): NVDEC video, audio, controller or keyboard/mouse input.
- `src/game` → **VERSION.dll** (proxy DLL loaded by MCC): 4 local players and network controllers
  (`local_players.cpp`), Present hook, NVENC video per quadrant, full-screen cutscenes (`cinematic.cpp`), name
  tags, armor colours, player 1 full screen, direct mouse aim, skins, subtitle copy. Talks to the host over
  127.0.0.1:27961.
- `src/common/protocol.h`: UDP protocol, **version 7**. Bump `k_version` whenever a message between client and
  host changes size (local host↔mod messages ship together and do not need it).
- `tools/test_game.cpp`: the test game.

## 4. What is done

| Feature | Notes |
|---|---|
| 4 local players | `local_players.cpp`: MCC game-manager hooks + 3 halo1.dll patches. Tested |
| Only connected players | Mission gets J1 + friends up to the highest connected number (2-4); video, name tags, J1 full screen and mouse aim follow the 2/3/4 split (`split_layout.h`). Late joiners watch the whole screen (Pong.watching, protocol v6) until the host presses F2 → Reload Mission. Tested |
| Join a started mission | Host F2 → *Reload Mission* (enabled only while a friend is waiting; notice at the top of the host's screen): repeats halo1's `LoadLevelSolo` (`+0x66AF0`, args saved from the last load) from the game thread (player 1's get_key_state hook), which recounts the players; MCC then does its own load and clears its loading screen. With the same count MCC leaves its loading screen up, so it is not offered. Tested 1→2 |
| Network controllers | Launcher sends J2-J4 pads over UDP 127.0.0.1; XInputGetState/SetState hook; slots 0-2 = players 2-4 (`proto::Pads`). Tested |
| Video | NVENC H.264 per quadrant, NVDEC in the client, 30/60/120 FPS |
| Full-screen cutscenes | While `halo1+0x1B87A20` is 1 every friend gets the whole screen scaled down. Tested |
| Hide MCC subtitles | Host option; hooks `halo1+0x47B180` so Halo sends `HideAll` (MCC settings untouched). Tested |
| Subtitles for each player | Host option + client *Subtitles* (F2). Hooks halo1 ShowText/HideText to know when a line is up and copies the subtitle band of the back buffer into every player's part (`subtitle_copy.cpp`), protocol v7. Tested 2-4 players |
| Halo-launcher look | Logo, icon, Halo-font images for fixed texts, Bahnschrift tabs, admin panel (`ui_halo.*`) |
| Audio | MCC-only capture (WASAPI process loopback), Opus 10 ms |
| Name tags | Drawn by the mod over each player (Halo 1 memory, read-only) |
| Single program | CoOpEvolved.exe with host/join selector, English by default with Español |
| Campaign armor colours | Writes the biped's change colours every 100 ms. Any RGB (`ArmorRgb`, protocol v7) + animated *Rainbow (Gaylo)*. Stripes are impossible: Halo CE uses one change colour for the whole armor. Only Master Chief bodies are painted: skins keep their own colours. Tested |
| Security | Host only accepts Radmin (26/8), private networks and loopback; client validates video fragments |
| Player 1 full screen | F6 or a panel option; stretches the top-left quadrant on the host's monitor |
| Host F2 menu in game | `host_menu.cpp`: ImGui over MCC after the video capture (only the host sees it); F2 or View held 1 s, B closes. Color, skin, F6, subtitles, host options, Kick, Reload Mission. Local messages `host_menu`/`host_kick` to the panel, which saves and resends `VideoConfig`. Player 1 input blocked while open. Tested |
| Direct Mouse Aim (Mouse Support) | Client option; the mouse turns the camera directly (protocol v4). Cameras from fixed halo1 pointers (`aim.cpp`), active as soon as the players appear; full scan only as a fallback. The client shows *Mouse enabled* when it starts working. Tested OK |
| Keyboard E = RB | Enter vehicles / pick up weapons |
| Players 3 and 4 in original missions | `coop_follow.cpp`: first spawn on J1/J2's recent footsteps (waits suspended until they move), script teleport/suspend/enter/exit repeated for J3/J4, Pelican middle seats (copied Chief animations) or hidden ride, nothing on maps with 4+ starts. Tested a30, b30 |
| Respawn in combat | halo1 patch `+0xAD0402`: ~3 s respawn even near enemies. Tested |
| Extra vehicles | `extra_vehicles.cpp`: c40 one Banshee per player beyond 2 (tested); a30 second Warthog with 4 players (not seen in game yet) |
| Friends' buttons | `hooked_mapping`: J2-J4 get a fixed layout matching the client's keys (R reload, E action, 1 flashlight). Tested |
| halo1 load notice | `halo1_load.cpp`: every halo1 patch/hook is applied from the LoadLibraryExW hook (no more freeze when leaving to the menu). Tested |
| MCC close crash | Hook on MCC `+0x8AD5E0` ignores events sent to the destroyed event manager. Tested |
| Per-player skins | Per-player skin selector (client and host panel), protocol v5. Tested in MCC (a30); Flood marine tested. Sergeant Johnson = armored marine with forced model variants (`Skin::variant`), tested in a10 |

## 5. Open work

- Skins: test checkpoint revert and level change. In c10 Keyes (fatigues) and Jenkins are not in
  the mission and Keyes (dress) has a skeleton that is not the marine's: the per-mission table in
  docs/DEVELOPMENT.md is not reliable for them (left as is by the owner).
- Subtitle copy: a few white specks of the scene can pass through the text filter (`ps_subtitle` threshold in
  `blit.cpp`); raise it if they bother.
- Level-end freeze fix (halo1 patch 3): not yet confirmed in a full play-through.
- Pause: only player 1 can open MCC's pause menu (MCC's menu only listens to the main user). The owner chose to
  keep it that way; friends' Start/Enter still reaches the game but does not pause.
- Reload Mission with the same player count (e.g. after a kick, or with nobody new) leaves MCC's loading screen
  up: MCC only does its own load when the count changes. Not offered for now; finding what halo1 tells MCC when the
  count changes would allow it.
- Ideas: opening-cutscene bodies born with their skin. The host/join selector that relaunches the exe stays as it is (owner's decision).

## 6. Rules (keep them)

- **Ask the owner before risky or outward-facing steps**: installing into MCC, anything that writes game memory,
  pushing, merging to main. Say what can fail and what they need to do. The owner runs commits and pushes
  themselves: give them the commands.
- **Reverse engineering in steps**: read-only diagnostic first → small write test → real implementation. Hooks
  and patches check the expected bytes (or module timestamp) first and turn themselves off on another version.
- **No personal data in the repository**: no IPs, local paths, Steam IDs, e-mails, logs or `.ini` files. Before
  committing, search the diff and the binaries, for example:
  `git diff --cached | grep -iE "users\\\\|@|26\.[0-9]+\.[0-9]+\.[0-9]+"` and look for your user name in the
  `.exe`/`.dll` files. `build.cmd` already strips source paths from the binaries.
- **Code style**: match the surrounding code. Code comments are in Spanish; user-facing text goes through
  `T("English", "Español")` (English is the default).
- **Documentation**: player-facing changes go into README.md (English) and README.es.md (Spanish), plus
  `CoOpEvolved\README.txt` / `LEEME.txt`. Do not mention internal phases in the public README. Technical findings
  go into docs/DEVELOPMENT.md. Keep THIRD-PARTY-NOTICES.md (licences of the libraries in `deps/`).

## 7. Gotchas

- Windows also loads `CoOpEvolved\VERSION.dll` into CoOpEvolved.exe (same folder); `dllmain.cpp` only activates
  inside MCC-Win64-Shipping.exe or the test game.
- MCC must be closed to replace `VERSION.dll`; CoOpEvolved.exe must be closed to build.
- MCC reads the controllers only while its window is active.
- Halo CE shows players 3 and 4 only with classic graphics.
- `halo1.dll` loads when Halo CE is opened from MCC's menu, not when MCC starts, and **reloads every time a
  mission is started from the menu** (often at the same address): its code is back to the original bytes, so
  every patch and hook on halo1.dll must be re-applied on each load. Do it only from `halo1_load_subscribe`
  (called right after LoadLibraryExW returns halo1.dll), never from a polling thread: hooking while the DLL is
  being unloaded froze MCC when leaving to the menu.
- A body's node buffers are sized for its model when it is created: skins must be in place at spawn, never later.
- The only key used by the mod in game is F6 (player 1 full screen).
- The host and client windows open maximized.
- Fixed UI texts are images made with the Halo font (`tools/make_ui_images.py`); the font itself must never be
  committed. New labels or buttons need a run of that script (see docs/DEVELOPMENT.md → Interface look).
