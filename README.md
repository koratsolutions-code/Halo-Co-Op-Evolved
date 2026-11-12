# Halo: Co Op Evolved

[Español](README.es.md) · **English**

**Halo: Combat Evolved** campaign (Halo: The Master Chief Collection, Steam) for **4 players**,
each on their own PC with their own full screen, connected through **Radmin VPN** to the host's PC.

The game runs only once, on the host's PC, with the screen split between the players (2, 3 or 4). Co Op Evolved
sends each friend only their part of the screen plus the game sound, and receives their controls back. During
cutscenes everyone sees the whole screen.

It is a single program, `CoOpEvolved.exe`, for everyone: the host chooses **Host a game** and the friends
**Join a game**. It is in **English** by default; switch to **Español** with the selector at the top right.

## Download

You do **not** need to build or install any programming tools: the ready-to-use files are already in
this repository.

1. At the top of this page press the green **Code** button → **Download ZIP**.
2. Unzip it (right-click → *Extract All...*).
3. Everyone (host and friends) uses the `CoOpEvolved/` folder. Copy it anywhere you like.

| File in `CoOpEvolved/` | What it is |
|------------------------|------------|
| `CoOpEvolved.exe` | The program: host panel or client, chosen at the top right |
| `VERSION.dll` | The mod inside the game. Only the host needs it: the program installs it by itself |
| `README.txt` / `LEEME.txt` | Short instructions |

---

## Requirements

**Host**
- Halo: The Master Chief Collection on **Steam** (version 1.3528.0.0).
- NVIDIA GPU (GTX 900 or newer) with up-to-date drivers: video is encoded with NVENC.
- About **10 Mbps of upload per friend at 60 FPS** (30 Mbps for 3 friends).
- [Radmin VPN](https://www.radmin-vpn.com/) installed and joined to the same network as your friends.
- Windows 10 version 2004 or newer (to capture only the game's sound).

**Each friend**
- Windows 10/11 with an NVIDIA GPU and up-to-date drivers (video is decoded with NVDEC).
- Radmin VPN joined to the same network as the host.
- Xbox controller, or keyboard and mouse.
- They do **not** need Halo, Steam or MCC.

Nothing else needs installing: no AlphaRing and no virtual-controller drivers.

---

## How to play (host)

1. Open `CoOpEvolved.exe` and choose **Host a game** at the top right (it opens in that mode from then on).
   With MCC closed, it finds the game in your Steam libraries and **installs or updates `VERSION.dll` by
   itself** in the game's `mcc\binaries\win64` folder. **Do not close it while you play.**
2. At the top it shows your **Radmin IP**: give it to your friends (button *Copy*).
3. Type your name in **Nickname** and pick your **Armor** color and **Skin** (player 1 card).
4. Press **Open MCC** and choose **"Play Halo: MCC (Anti-Cheat Disabled)"** in the Steam window.
5. Start a Halo CE campaign mission: it has **as many players as are connected**, no extra menu needed (see
   *Players and late arrivals*).
6. Turn on **classic graphics** (**Tab** key in game): with the new graphics only 2 players are shown.
7. Keep the game window **active** (in the foreground) while you play: if you switch to another
   window, MCC stops reading the controllers.

Player 1 (you) plays with **keyboard and mouse**.

### Players and late arrivals
- When a mission starts **from the MCC menu**, it gets player 1 plus every friend up to the highest connected
  player number: with player 2 connected there are 2 players, with players 2 and 3 there are 3. Each friend keeps
  their number, so if players 2 and 4 are connected there are 4 and player 3 stands still. Ask your friends to use
  2, 3 and 4 in that order.
- The screen is split accordingly: **2 players** = top and bottom halves; **3 players** = player 1 on the top
  half, player 2 bottom left, player 3 bottom right; **4 players** = quarters.
- A friend who connects **after the mission started** watches the whole screen with a notice. To let them in,
  go back to the MCC menu and start the mission again: restarting from the pause menu keeps the same players.

### Game folder
To find the game's `mcc\binaries\win64` folder: in Steam, right-click
**Halo: The Master Chief Collection → Manage → Browse local files**, then open `mcc` → `binaries` → `win64`.

### Video
The panel shows the **Video** line: a few seconds after MCC opens it says *Video ready* with the game
resolution, and each player card shows the FPS and Mbps that friend receives. The **Video at 30/60/120 FPS**
selector sets how much to send: more FPS = less delay, but more upload bandwidth. Each friend receives their part
of the game resolution (with the game at 2560x1440 and 4 players, each sees 1280x720; with 2 players, player 2
gets a wide 2560x720 strip).

### Cutscenes
Halo draws cutscenes once over the whole screen. While one plays, each friend receives **the whole screen**
(scaled to their size) instead of their quarter, and gets their quarter back when it ends. **Full-screen
cutscenes for friends** turns it off. Subtitles during gameplay are still drawn by MCC across the whole split
screen, so friends may see them cut.

### Audio
Co Op Evolved captures **only MCC's sound** (not Discord, music or other apps) and sends the same mix
to every friend (~0.1 Mbps each). The panel shows *Audio active* while MCC is open.

### Names over players
The game mod draws each player's name over their character, on your screen and in every friend's video.
Names stay visible through walls and get smaller and fainter with distance. Player 1's name is the
**Nickname** in the panel; players 2-4 use the name they typed in their client.
- **Show names over players** turns them on or off.
- If the names do not sit right over the characters, set **Game FOV** to the game's field of view.
- The names show up a few seconds after each level loads.

### Player 1 full screen
By default you see the whole split screen. With **Player 1 full screen (F6 in game)** ticked, you see only
your part (top left) stretched to your whole monitor; your friends still get their own part.
- **F6** switches between full screen and split screen while you play (the game must be the active window).
  The panel box only chooses how it starts.
- MCC's pause menus are drawn for the whole screen, so in full screen they look cut off: press **F6** to
  use them in split screen. Cutscenes are shown whole.
- Your image is your part of the game resolution stretched, so a higher game resolution looks sharper
  (at 2560x1440 with 4 players your part is 1280x720). With 2 or 3 players your part is a wide strip: it is
  shown without stretching, with black bars above and below.

### Armor colors
MCC's armor customization only applies to multiplayer, so the game mod paints each player's armor in the
campaign. Player 1 picks it in **Armor** (player 1 card); players 2-4 in **Armor color** in their client
(also in F2 while playing). There are the 18 Halo CE colors plus **Default** (the usual olive green). The
color comes back by itself when a player respawns or a new level loads, and every player card shows it.

### Skins
Each player can look like another Halo CE character and still be Master Chief in everything else (hitbox,
speed, jump). Player 1 picks it in **Skin** (player 1 card); players 2-4 in **Skin** in their client (also in
F2 while playing). Every player card shows each friend's skin.

Skins: Master Chief, Marine, Armored marine, Crewman, Captain Keyes (fatigues), Captain Keyes (dress), Jenkins,
Flood marine and Elite.
- A skin only exists in the missions that have that character. Once a mission is loaded, the selector greys
  out the ones it does not have; if yours is not there, you are Master Chief in that mission.
- A new skin shows **the next time you respawn**: the body you have keeps its look until it dies.
- Missions that start with a cutscene that places the players (for example *Halo*, with the drop pod) start
  everyone as Master Chief; each player gets their skin on their first respawn.
- Only the look changes: shots still hit where Master Chief's body would be.

### Testing the controllers
Each player's **Move** button makes their character walk forward, back, sideways, turn and jump (about
10 seconds). Afterwards the game is brought back to the foreground.

### Playing as a client on the same PC
To test, you can open `CoOpEvolved.exe` a second time while the host is open: that window opens as
**Join a game** by itself (connect to `127.0.0.1`). Only one host can run on a PC: a second host window opens
greyed out and does nothing.

### First time: firewall
The first time you host, Windows asks whether to allow network access. Tick **private and public networks**
(Radmin VPN is usually a public network) and accept. If it does not appear and your friends cannot connect,
allow `CoOpEvolved.exe` in *Windows Firewall → Allow an app*.

### Security
- The host only answers **Radmin VPN** (26.x.x.x), your **local network** and your own PC. Packets from
  the internet are ignored, even if the port is open in the firewall or forwarded on your router. You do not
  need to (and should not) forward any port.
- Anyone in your Radmin network can take a free player slot: protect the Radmin network with a password and
  only share it with your friends. **Kick** removes someone at any time.
- The game mod changes the game only while it runs: the 4 local players, the controls of players 2-4, armor
  colors, skins and cutscenes. It does not touch your files, your Steam account or your MCC profile.
- `coopevolved-host.log` and `coopevolved-cliente.log` record your friends' names and Radmin IPs. They stay on
  your PC: do not post them publicly.

### Host panel
It is laid out like the split screen: one card per player. Each connected friend shows their name, IP,
how many control updates per second arrive (about 250 is normal) and the video they receive.
**Kick** disconnects them (they can reconnect). The panel remembers your nickname, armor color, skin, language,
FPS and name settings in `coopevolved-host.ini`, next to `CoOpEvolved.exe`.

### Common problems
- **The game does not have the players it should:** the number is set when the mission starts from the MCC
  menu (see *Players and late arrivals*). Check that the panel shows a green *4 players: built in* dot and that
  `VERSION.dll` is installed (*Game mod* dot). It only works with MCC 1.3528 on Steam.
- **AlphaRing is still installed** (from an older version of Co Op Evolved): it still works, but then
  AlphaRing sets the players (F4 → Players = 4 every time). To use the built-in 4 players, delete
  `WTSAPI32.dll` from the game's `mcc\binaries\win64` folder.
- **Video: game not responding:** wait about 10 seconds after opening MCC. If it persists, check that
  the panel shows a green *Game mod* dot and look at `coopevolved-juego.log` in the `mcc\binaries\win64`
  folder.
- **No names over the players:** check that *Show names over players* is on and wait a few seconds after
  the level loads. The names only work with MCC version 1.3528.

### Known issues with 4 players
- With remastered graphics only players 1 and 2 are shown → use classic graphics (Tab).
- In some levels (*Assault on the Control Room*, *Keyes*) players 3 and 4 may spawn outside the map.
- Finishing a level with 4 players used to freeze the game; the mod includes a fix, but if it still happens,
  save and restart the mission.

---

## How to play (friends)

For players 2, 3 and 4. You do **not** need Halo, Steam or MCC: the game runs on the host's PC.

### Before the first time
1. **Radmin VPN:** install [Radmin VPN](https://www.radmin-vpn.com/) and join the host's network
   (*Network → Join an existing network*, with the network name and password the host gives you).
2. **NVIDIA drivers:** update your NVIDIA GPU drivers (GeForce Experience / NVIDIA App, or nvidia.com).
3. **The program:** download the repository (*Download*) and copy the `CoOpEvolved/` folder anywhere you
   like. Nothing else needs installing.

### Every time you play
1. Check that Radmin VPN is **on** and that the host shows as connected in your Radmin network.
2. Open `CoOpEvolved.exe`. The first time Windows may show "Windows protected your PC":
   *More info → Run anyway*. If it opens the host panel, choose **Join a game** at the top right.
3. Fill in the menu (it is all remembered for next time):
   - **Host's Radmin IP:** the host sees it at the top of their Co Op Evolved (button *Copy*).
   - **Your name:** shown over your character to everyone.
   - **Armor color:** your color in the campaign.
   - **Skin:** the character you look like (see *Skins* in the host section).
   - **Player:** **2** (top right), **3** (bottom left) or **4** (bottom right). Each friend a different
     one; keep the same one every time.
   - **Controller** or **Keyboard and mouse**.
4. Press **Connect**. When the host opens the game, the client switches to **full screen** with your part
   of the screen and the game sound. Connect **before the host starts the mission**: if you join later you watch
   the whole screen until the host starts the mission again from the MCC menu.
5. To leave: **F2 → Disconnect**, or close the window.

### Keys
| Key | What it does |
|-----|--------------|
| **F2** | Menu: armor color, skin, volume, sensitivity, controls, disconnect |
| **F1** | Stats: ping, video FPS, Mbps, lost frames |
| **F3** | Mute / unmute the game sound |
| **F11** or **Alt+Enter** | Full screen / window |
| **Esc** | Release the mouse (keyboard and mouse) |
| **+ / -** | Mouse sensitivity (0.1x to 99x) |

- With a **controller** nothing else is needed (Xbox controller recommended).
- With **keyboard and mouse** click the video to capture the mouse. The full key list is in
  `CoOpEvolved/README.txt` and in F2 → *Keyboard controls*.
- **Direct mouse aim (beta)** (keyboard and mouse, in the menu or F2): the mouse turns your camera directly,
  like Halo on PC, instead of moving the controller's right stick (no dead zone or stick acceleration). Next to
  it, *active* means it is working; if the host's game cannot use it yet, the stick is used as before. The host's
  panel shows *mouse aim* on your card. Adjust it with **+ / -**.
- If the video freezes for a moment after packet loss, it recovers by itself in under half a second.
- **Game volume** is set with the slider in the main menu or in F2 (it is saved); **F3** mutes.

### Common problems (friends)
- **"The host is not responding":** check the IP, that Radmin VPN is connected (ping the host from Radmin)
  and that the host has Co Op Evolved open in **Host a game**.
- **"Waiting for video...":** the host has not opened MCC yet, or their panel shows the game mod missing.
- **"That player is already taken":** another friend picked your number; pick another one.
- **"You joined with the mission already started":** you are watching; you will play when the host starts the
  mission again from the MCC menu.
- **"Your client and the host's are different versions":** you and the host must use the same version of Co Op Evolved; download it again.
- **Black screen or NVIDIA error on a laptop:** *Windows Settings → Display → Graphics* → set
  `CoOpEvolved.exe` to **High performance**.
- **Your character does not move:** the game must be the active window on the host's PC; tell the host.
- If something fails, send the host your `coopevolved-cliente.log` (next to `CoOpEvolved.exe`) privately.

---

## Building from source (optional)

**Only if you want to change the code.** To play, use the files that already come in `CoOpEvolved/`
(see *Download*).

1. **Install Visual Studio** (free):
   1. Go to <https://visualstudio.microsoft.com/downloads/> and download **Visual Studio Community**.
   2. Open the installer. On the **Workloads** tab tick **Desktop development with C++** and press
      **Install**. It downloads several GB and takes a while; you do not need to change anything else.
   3. When it finishes you can close Visual Studio: it does not need to stay open.
2. **Download the code**: green **Code** button → **Download ZIP**, then unzip it.
3. **Build**: in the unzipped folder, double-click **`build.cmd`**.
   - If Windows shows "Windows protected your PC": *More info → Run anyway*.
   - A black window opens. The first time it takes a few minutes; then it is much faster.
   - It ends with **`DONE: ...`** and *Press any key*: the new files are already in `CoOpEvolved/`.
   - If it ends with **`BUILD FAILED`**, read the messages above it. The most common causes: Visual Studio
     was installed without *Desktop development with C++* (open *Visual Studio Installer* →
     **Modify** and tick it), or `CoOpEvolved.exe` was open while building.

The build also creates a `build/` folder with temporary files; you can delete it.

---

## Uninstall

Delete `VERSION.dll` (this mod), `coopevolved-juego.ini` and `coopevolved-juego.log` from the game's
`mcc\binaries\win64` folder (and `WTSAPI32.dll` if you installed AlphaRing for an older version).
Then in Steam: right-click MCC → Properties → Installed Files → **Verify integrity**.

## Credits

The 4-player split screen follows [AlphaRing](https://github.com/WinterSquire/AlphaRing) by WinterSquire
(MIT licence); see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
