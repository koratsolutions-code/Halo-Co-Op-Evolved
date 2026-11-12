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
  else: the mod sets up the 4 players and the controllers by itself (no AlphaRing, no ViGEmBus).

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
  tags, armor colours, player 1 full screen, direct mouse aim, skins. Talks to the host over 127.0.0.1:27961.
- `src/common/protocol.h`: UDP protocol, **version 6**. Bump `k_version` whenever a message between client and
  host changes size (local host↔mod messages ship together and do not need it).
- `tools/test_game.cpp`: the test game.

## 4. What is done

| Feature | Notes |
|---|---|
| Local players without AlphaRing | `local_players.cpp`: MCC game-manager hooks + 3 halo1.dll patches (from AlphaRing, MIT). Tested |
| Only connected players | Mission gets J1 + friends up to the highest connected number (2-4); video, name tags, J1 full screen and mouse aim follow the 2/3/4 split (`split_layout.h`). Late joiners watch the whole screen (Pong.watching, protocol v6). Tested |
| Network controllers without ViGEmBus | XInputGetState/SetState hook; slots 0-2 = players 2-4 (`proto::Pads`). Tested |
| Video | NVENC H.264 per quadrant, NVDEC in the client, 30/60/120 FPS |
| Full-screen cutscenes | While `halo1+0x1B87A20` is 1 every friend gets the whole screen scaled down. Tested |
| Audio | MCC-only capture (WASAPI process loopback), Opus 10 ms |
| Name tags | Drawn by the mod over each player (Halo 1 memory, read-only) |
| Single program | CoOpEvolved.exe with host/join selector, English by default with Español |
| Campaign armor colours | Writes the biped's change colours every 100 ms |
| Security | Host only accepts Radmin (26/8), private networks and loopback; client validates video fragments |
| Player 1 full screen | F6 or a panel option; stretches the top-left quadrant on the host's monitor |
| Direct mouse aim (beta) | Client option; the mouse turns the camera directly (protocol v4). Tested OK |
| Keyboard E = RB | Enter vehicles / pick up weapons |
| Per-player skins | Per-player skin selector (client and host panel), protocol v5. Tested in MCC (a30) |

## 5. Open work

- Skins: test Flood marine, Keyes, Jenkins, checkpoint revert and level change.
- Subtitles during gameplay: MCC (not halo1.dll) draws them over the whole split screen, so friends see them cut.
  The text is in MCC's UI objects (`<color="#…">CORTANA: …</>`), which move around the heap; finding the current
  one needs a write watch on the text from inside the mod. Not started.
- Level-end freeze fix (halo1 patch 3) and pause with 4 players: not yet confirmed in a full play-through.
- Late joiners only get in when the mission is started again from the MCC menu: MCC asks for the users only then
  (restart from the pause menu reuses them). Forcing it on restart would mean digging into MCC's `game_restart`
  (game-manager vtable +0x20). Not started.
- Ideas: one window with host/client tabs instead of relaunching; spawning only as many players as are connected.
- The old ViGEmBus + AlphaRing path is still there as a fallback: `controllers=vigem` in `coopevolved-host.ini`
  (the host then writes `mandos=vigem` to `coopevolved-juego.ini` in the game folder).

## 6. Rules (keep them)

- **Ask the owner before risky or outward-facing steps**: installing into MCC, anything that writes game memory,
  pushing, merging to main. Say what can fail and what they need to do. The owner runs commits and pushes
  themselves: give them the commands.
- In the ViGEmBus fallback, **never write AlphaRing's profiles or options** and never change how or when the
  virtual pads are created: that broke movement in an earlier version.
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
  go into docs/DEVELOPMENT.md. Keep THIRD-PARTY-NOTICES.md (AlphaRing's MIT notice).

## 7. Gotchas

- Windows also loads `CoOpEvolved\VERSION.dll` into CoOpEvolved.exe (same folder); `dllmain.cpp` only activates
  inside MCC-Win64-Shipping.exe or the test game.
- MCC must be closed to replace `VERSION.dll`; CoOpEvolved.exe must be closed to build.
- MCC reads the controllers only while its window is active.
- Halo CE shows players 3 and 4 only with classic graphics.
- `halo1.dll` loads when Halo CE is opened from MCC's menu, not when MCC starts, and **reloads every time a
  mission is started from the menu** (often at the same address): its code is back to the original bytes, so
  every patch and hook on halo1.dll must be re-applied when the original bytes reappear (local_players and skins
  check the bytes, not the module address).
- If AlphaRing (`WTSAPI32.dll` in the game folder) is present, the mod leaves the 4 players to it and only
  provides the network controllers.
- A body's node buffers are sized for its model when it is created: skins must be in place at spawn, never later.
- The only key used by the mod in game is F6 (player 1 full screen).
- The host and client windows open maximized.
