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
- To test in the game: **Halo: The Master Chief Collection on Steam** (the code targets version 1.3528),
  **AlphaRing** and **ViGEmBus**, installed as explained in README.md → *Host installation*.

```bash
git clone https://github.com/koratsolutions-code/Halo-Co-Op-Evolved.git
cd Halo-Co-Op-Evolved
git checkout develop
git pull
# Your own name and e-mail for your commits in this repository:
git config user.name "your-github-user"
git config user.email "your-github-email"
```

Build (from `cmd` or a terminal, in the repository folder):

```bash
cmd /c build.cmd
```

It ends with `DONE:` and updates `host\CoOpEvolvedHost.exe`, `host\VERSION.dll` and
`cliente\CoOpEvolvedClient.exe` (these binaries are committed so players can download them).
If it says `Could not copy`, close CoOpEvolvedHost and CoOpEvolvedClient: the host also loads `host\VERSION.dll`.

Test without MCC (in a terminal, from the repository folder):

```bash
build\CoOpEvolvedTestGame.exe "%CD%\build\VERSION.dll"
```

Then open `host\CoOpEvolvedHost.exe` and `cliente\CoOpEvolvedClient.exe` (IP `127.0.0.1`). The test game draws four
coloured quadrants; the client must show one of them. Environment variables for tests:
`COOPEVOLVED_NO_INSTALL=1` (the host does not touch the game folder), `COOPEVOLVED_AUTOCONNECT=1` (the client
connects with its saved settings).

Test in MCC: **close MCC**, copy `host\VERSION.dll` to `<MCC folder>\mcc\binaries\win64\` (or use the host panel →
*Install mod in the game*), open the host first, then MCC without anti-cheat, AlphaRing (F4) → Players = 4, Halo CE
with classic graphics (Tab).

Logs (git-ignored, overwritten every launch): `host\coopevolved-host.log`, `cliente\coopevolved-cliente.log`,
`<MCC folder>\mcc\binaries\win64\coopevolved-juego.log`.

## 2. Branches and workflow

- **main**: stable, what players download. Do not push to it directly.
- **develop**: work in progress. Work here (or in a branch made from develop) and open a pull request
  develop → main only after the change was tested in MCC.
- Before every commit: build, test, and check that no personal data got in (see rules below).

## 3. What the project is

Four people play the Halo CE campaign in MCC, each on their own PC with their own full screen, over Radmin VPN.
MCC runs only on the host's PC with AlphaRing's 4-player split screen. The mod sends each friend only their quarter
of the screen as low-latency video plus the game sound, and their controller or keyboard/mouse comes back to a
virtual pad. Lowest latency is the top priority.

- `src/launcher` → **CoOpEvolvedHost.exe**: creates the virtual pads for players 2–4 *before* MCC opens, receives
  input over UDP 27960, captures MCC's audio, ImGui panel (players, names, armor colours, options).
- `src/game` → **VERSION.dll** (proxy DLL loaded by MCC): Present hook, NVENC video per quadrant, name tags, armor
  colours, player 1 full screen, direct mouse aim, skins (in progress). Talks to the host over 127.0.0.1:27961.
- `src/client` → **CoOpEvolvedClient.exe**: NVDEC video, audio, controller or keyboard/mouse input.
- `src/common/protocol.h`: UDP protocol, **version 4**. Bump `k_version` whenever a message changes size.
- `tools/test_game.cpp`: the test game.

## 4. What is done (all on main unless noted)

| Feature | Notes |
|---|---|
| Virtual pads before MCC, network input | ViGEmBus; AlphaRing maps Controller 1–3 to players 2–4 |
| Video | NVENC H.264 per quadrant, NVDEC in the client, 30/60/120 FPS |
| Audio | MCC-only capture (WASAPI process loopback), Opus 10 ms |
| Name tags | Drawn by the mod over each player (Halo 1 memory, read-only) |
| Host panel | ImGui, English by default with Español |
| Campaign armor colours | Writes the biped's change colours every 100 ms |
| Security | Host only accepts Radmin (26/8), private networks and loopback; client validates video fragments |
| Player 1 full screen | F6 or a panel option; stretches the top-left quadrant on the host's monitor |
| Direct mouse aim (beta) | Client option; the mouse turns the camera directly (protocol v4). Tested OK |
| Keyboard E = RB | Enter vehicles / pick up weapons |
| **Skins (develop only)** | Steps 1–2 done, step 3 written but **not tested in MCC**; see below |

## 5. Current work: per-player skins

Goal: each player looks like another character (marine, crewman, Elite…) but keeps Master Chief's hitbox, speed,
jump and physics. Each friend chooses in their client; player 1 in the host panel. If the chosen character is not in
the current mission, that player spawns as Master Chief (the owner decided: no modified maps or external tools).

How it works: right before the game creates player N's body (`halo1.dll+0xAD4184`), `src/game/skins.cpp` puts the
chosen character's **model and animations** into Master Chief's definition, and restores them right after. The
full list of skins per mission and all addresses are in docs/DEVELOPMENT.md.

Next steps:
1. **Test step 3 in MCC.** In a mission (e.g. Pillar of Autumn) press **F10** (turns the test on), then kill
   players 2, 3 and 4. Expected: J2 armored marine, J3 marine, J4 crewman, J1 still Master Chief, each only their
   own. Also restart the mission with F10 on: they should start with their skins. Read the `Skins:` lines in
   `coopevolved-juego.log` (detected player and arguments for each spawn).
2. **Step 4, choosing the skin** (protocol v5): a skin field in `PlayerOptions` (client → host), skins for 1–4 in
   `VideoConfig` (host → mod), and the skins available in the current mission reported by the mod in `VideoStatus`
   and passed to the clients (e.g. in `Pong`). UI: a combo next to armor colour in the client menu and F2, and on
   the player 1 card in the host panel; saved in the `.ini` files. Missing skins → Master Chief.
3. **Step 5**: checkpoint revert, cinematics, level change, other missions; then remove the diagnostics
   (`character_probe.*`, `spawn_probe.*`, the F10 test), update README.md, README.es.md and the `host\`/`cliente\`
   txt files, and merge to main.

Later ideas, not started: replacing AlphaRing with an own mod (feasibility study first; AlphaRing has no licence, so
nothing can be copied from it); spawning only as many players as are connected.

## 6. Rules (keep them)

- **Ask the owner before risky or outward-facing steps**: installing into MCC, anything that writes game memory,
  pushing, merging to main. Say what can fail and what they need to do.
- **Never write AlphaRing's profiles or options** and never change how or when the virtual pads are created: that
  broke movement in an earlier version.
- **Reverse engineering in steps**: read-only diagnostic first → small write test → real implementation. Hooks
  and patches check the expected bytes first and turn themselves off on another MCC version.
- **No personal data in the repository**: no IPs, local paths, Steam IDs, e-mails, logs or `.ini` files. Before
  committing, search the diff and the binaries, for example:
  `git diff --cached | grep -iE "users\\\\|@|26\.[0-9]+\.[0-9]+\.[0-9]+"` and look for your user name in the
  `.exe`/`.dll` files. `build.cmd` already strips source paths from the binaries.
- **Code style**: match the surrounding code. Code comments are in Spanish; user-facing text goes through
  `T("English", "Español")` (English is the default).
- **Documentation**: player-facing changes go into README.md (English) and README.es.md (Spanish), plus
  `host\README.txt`/`LEEME.txt` and `cliente\README.txt`/`LEEME.txt`. Do not mention internal phases in the public
  README. Technical findings go into docs/DEVELOPMENT.md.

## 7. Gotchas

- Windows also loads `host\VERSION.dll` into CoOpEvolvedHost.exe (same folder); `dllmain.cpp` only activates
  inside MCC-Win64-Shipping.exe or the test game.
- MCC must be closed to replace `VERSION.dll`; the host and client must be closed to build.
- MCC reads the pads only while its window is active; AlphaRing's menu (F4) blocks all input while open.
- Halo CE shows players 3 and 4 only with classic graphics.
- `halo1.dll` loads when Halo CE is opened from MCC's menu, not when MCC starts.
- A biped's model and animations are read when it spawns: memory changes show up on the next spawn.
- Keys used by the mod in game: F6 (player 1 full screen); in develop also F9 (character list), F10 (skins test),
  F11 (spawn watcher).
