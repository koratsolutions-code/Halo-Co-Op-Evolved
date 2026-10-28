# halo1-coop

[Español](README.es.md) · **English**

**Halo: Combat Evolved** campaign (Halo: The Master Chief Collection, Steam) for **4 players**,
each on their own PC with their own full screen, connected through **Radmin VPN** to the host's PC.

The game runs only once, on the host's PC, with the screen split in 4 (AlphaRing). The host mod sends
each friend only their quarter of the screen plus the game sound, and receives their controls back.

> **Current status (phase 5):** the host (`HaloCoopHost.exe` + `VERSION.dll`) creates the controllers
> for players 2, 3 and 4, receives their input and sends each one their quadrant as video (NVENC) plus
> the game sound (Opus). Each friend sees and hears it full screen in `HaloCoopClient.exe`, with every
> player's name over their character.
>
> Both apps are in **English** by default; switch to **Español** with the selector at the top right.

## Building from source

Requires Visual Studio 2022 or Build Tools with *Desktop development with C++* (includes CMake and
Ninja). Run `build.cmd`: it builds and places `HaloCoopHost.exe` and `VERSION.dll` in `host/` and
`HaloCoopClient.exe` in `cliente/`.

---

## Repository layout

| Folder     | For whom          | Contents |
|------------|-------------------|----------|
| `host/`    | The host          | `HaloCoopHost.exe` (launcher and panel), `VERSION.dll` (game mod, installs itself) and instructions (`README.txt` / `LEEME.txt`) |
| `cliente/` | The other 3 players | `HaloCoopClient.exe` and instructions (`README.txt` / `LEEME.txt`) |

---

## Requirements

**Host**
- Halo: The Master Chief Collection on **Steam** (version 1.3528.0.0).
- NVIDIA GPU (GTX 900 or newer) with up-to-date drivers: video is encoded with NVENC.
- About **10 Mbps of upload per friend at 60 FPS** (30 Mbps for 3 friends).
- [Radmin VPN](https://www.radmin-vpn.com/) installed and joined to the same network as your friends.
- Windows 10 version 2004 or newer (to capture only the game's sound).

**Each client**
- Windows 10/11 with an NVIDIA GPU and up-to-date drivers (video is decoded with NVDEC).
- Radmin VPN joined to the same network as the host.
- Xbox controller, or keyboard and mouse.

---

## Host installation

### Game folder
Several steps use the game's `mcc\binaries\win64` folder. To find it: in Steam, right-click
**Halo: The Master Chief Collection → Manage → Browse local files**, then open
`mcc` → `binaries` → `win64`.

### 1. AlphaRing (4-player split screen)

AlphaRing is not included in this repository: download it from its official page.

1. Download `Release1.0.zip` from release **1.3528.0.0** at
   <https://github.com/wouter51/AlphaRing/releases>.
2. Unzip it and copy `WTSAPI32.dll` into the game's `mcc\binaries\win64` folder.
3. (Optional) Check it is the right file: in that folder, Shift + right-click → *Open in Terminal*
   and run:
   ```powershell
   Get-FileHash .\WTSAPI32.dll
   ```
   It must start with `091AA64DCC040BB6`.

### 2. Virtual controller driver (ViGEmBus)

Install **ViGEmBus** from <https://github.com/nefarius/ViGEmBus/releases/latest>
(file `ViGEmBus_..._x64_x86_arm64.exe`) and restart the PC. If it is missing, `HaloCoopHost.exe`
tells you and opens that page.

### 3. HaloCoop Host

Copy the `host/` folder anywhere you like (it does not need to go in the game folder).
It contains `HaloCoopHost.exe`, which creates the controllers for players 2, 3 and 4, and
`VERSION.dll`, the mod that captures the video inside the game.

When you open `HaloCoopHost.exe` (with MCC closed) it finds MCC in your Steam libraries and **copies
`VERSION.dll` by itself** into `mcc\binaries\win64`, or updates it if it changed. The panel shows
whether the mod and AlphaRing are installed; if it could not copy it (permissions), copy it there
yourself.

---

## How to play (host)

> **Important:** always open **HaloCoop Host first** and MCC afterwards. The controllers for players
> 2, 3 and 4 must exist before the game starts; otherwise those players cannot move.
> If MCC is already open, the launcher asks you to close it.

1. Unplug any physical controller (it changes the controller order). To play as player 1 with a
   controller, see *Common problems*.
2. Open `HaloCoopHost.exe`. At the top it shows your Radmin IP (give it to your friends) and which
   Controller each player got (usually P2 = Controller 1, P3 = Controller 2, P4 = Controller 3).
   **Do not close it while you play.**
3. Type your name in **Nickname** (player 1 card): your friends see it over your character.
4. Press **Open MCC** and choose **without anti-cheat (EAC disabled)**.
5. Press **F4** to open the AlphaRing menu → **Splitscreen**:
   - **Players = 4**.
   - Do not change the other options. In particular, keep *"Use player1's profile"* enabled.
   - AlphaRing **does not save** its settings: do this every time the game is opened.
6. Close the menu with **F4**. While it is open, **no player can move**.
7. In Halo CE turn on **classic graphics** (**Tab** key in game): with the new graphics only
   2 players are shown.
8. Keep the game window **active** (in the foreground) while you play: if you switch to another
   window, MCC stops reading the controllers.

### Video
The panel shows the **Video** line: a few seconds after MCC opens it says *Video ready* with the game
resolution, and each player card shows the FPS and Mbps that friend receives. The **Video at 30/60/120 FPS**
selector sets how much to send: more FPS = less delay, but more upload bandwidth. Each friend receives a quarter of the
game resolution (with the game at 2560x1440, each sees 1280x720).

### Audio
HaloCoop Host captures **only MCC's sound** (not Discord, music or other apps) and sends the same mix
to every friend (~0.1 Mbps each). The panel shows *Audio active* while MCC is open.

### Names over players
The game mod draws each player's name over their character, on your screen and in every friend's video.
Names stay visible through walls and get smaller and fainter with distance. Player 1's name is the
**Nickname** in the panel; players 2-4 use the name they typed in their client.
- **Show names over players** turns them on or off.
- If the names do not sit right over the characters, set **Game FOV** to the game's field of view.
- The names show up a few seconds after each level loads.

### Testing the controllers
In HaloCoop Host, each player's **Move** button makes their character walk forward, back,
sideways, turn and jump (about 10 seconds). Afterwards the game is brought back to the foreground.

### First time: firewall
The first time you open HaloCoop Host, Windows asks whether to allow network access. Tick
**private and public networks** (Radmin VPN is usually a public network) and accept. If it does not
appear and your friends cannot connect, allow `HaloCoopHost.exe` in *Windows Firewall → Allow an app*.

### Host panel
It is laid out like the split screen: one card per player. Each connected friend shows their name, IP,
how many control updates per second arrive (about 250 is normal) and the video they receive.
**Kick** disconnects them (they can reconnect). The panel remembers your nickname, language, FPS and
name settings in `halocoop-host.ini`, next to `HaloCoopHost.exe`.

### Common problems
- **A player does not move:** close MCC, close HaloCoop Host, then open them again in that order.
- **I have a physical controller plugged in:** the virtual ones end up on other Controllers.
  HaloCoop Host warns you and shows which one each player got; assign those Controllers in AlphaRing.
- **HaloCoop Host says ViGEmBus is missing:** install it (step 2) and restart the PC.
- **Video: game not responding:** wait about 10 seconds after opening MCC. If it persists, check that
  the panel shows a green *Game mod* dot and look at `halocoop-juego.log` in the `mcc\binaries\win64`
  folder.
- **No names over the players:** check that *Show names over players* is on and wait a few seconds after
  the level loads. The names only work with MCC version 1.3528.

### Known AlphaRing issues with 4 players
- With remastered graphics only players 1 and 2 are shown → use classic graphics (Tab).
- In some levels (*Assault on the Control Room*, *Keyes*) players 3 and 4 may spawn outside the map.
- Finishing a level with 4 players may freeze the game: save and restart the mission if it happens.

---

## Client installation and use

Each friend downloads the `cliente/` folder and opens `HaloCoopClient.exe`
(if Windows shows "Windows protected your PC": *More info → Run anyway*).

1. Type the **host's Radmin IP** (shown at the top of their HaloCoop Host) and your name.
2. Choose your player number: **2** (top right), **3** (bottom left) or **4** (bottom right).
   Each friend a different one.
3. Choose **Controller** or **Keyboard and mouse** and press **Connect**.

When the video arrives, the client switches to **full screen** with your quadrant of the game.

| Key | What it does |
|-----|--------------|
| **F2** | Menu: volume, sensitivity, controls, disconnect |
| **F1** | Stats: ping, video FPS, Mbps, lost frames |
| **F3** | Mute / unmute the game sound |
| **F11** or **Alt+Enter** | Full screen / window |
| **Esc** | Release the mouse (keyboard and mouse) |
| **+ / -** | Mouse sensitivity (0.1x to 99x) |

- With a **controller** nothing else is needed.
- With **keyboard and mouse** click the video to capture the mouse. The key list is in
  `cliente/README.txt`.
- If the video freezes for a moment after packet loss, it recovers by itself in under half a second.
- **Game volume** is set with the slider in the main menu or in F2 (it is saved); **F3** mutes.

---

## Uninstall

Delete `WTSAPI32.dll` (AlphaRing), `VERSION.dll` (this mod) and `halocoop-juego.log` from the game's
`mcc\binaries\win64` folder.
Then in Steam: right-click MCC → Properties → Installed Files → **Verify integrity**.
