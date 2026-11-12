HALO: CO OP EVOLVED
===================

Halo: Combat Evolved campaign (MCC, Steam) for 4 players, each on their own PC with their
own full screen, through Radmin VPN. One program for everyone:

  CoOpEvolved.exe -> the program. At the top right choose "Host a game" (player 1, the PC
                     with Halo) or "Join a game" (players 2, 3 and 4). It opens in the last
                     mode you used.
  VERSION.dll     -> the mod inside the game. Only the host needs it: CoOpEvolved.exe
                     copies it into the game folder by itself.

The program is in English. To use it in Spanish pick "Español" at the top right (it is saved).
Nothing else needs installing (no AlphaRing, no virtual-controller drivers).

HOST (player 1)
---------------
You need: MCC 1.3528 on Steam, an NVIDIA GPU, Radmin VPN and about 10 Mbps of upload per friend.

  1. Open CoOpEvolved.exe and choose "Host a game". With MCC closed it installs or updates
     the mod in the game by itself. Do not close it while you play.
     The first time, if Windows asks about the firewall, tick private and public networks.
  2. Give your friends the Radmin IP shown at the top ("Copy" copies it).
  3. In the player 1 card type your name in "Nickname" and pick your "Armor" color and "Skin".
  4. Press "Open MCC" and choose "Play Halo: MCC (Anti-Cheat Disabled)".
  5. Start a Halo CE campaign mission: it already has 4 players.
  6. Use classic graphics (Tab) and keep the game as the active window.
  You play with keyboard and mouse.

  In the panel:
  - Dots at the top: green = ready (game mod, 4 players, MCC, audio, video).
  - "Video at 30/60/120 FPS": more FPS = less delay but more upload bandwidth.
  - "Show names over players": if they do not sit right, set "Game FOV" to the game's FOV.
  - "Player 1 full screen (F6 in game)": you see only your part, stretched to your monitor.
  - "Full-screen cutscenes for friends": during cutscenes each friend sees the whole screen.
  - One card per player: name, IP, inputs per second and video of each friend.
    "Move": tests that player's controls.  "Kick": disconnects them.
  - "Skin": look like another character (Marine, Elite...) while still being Master Chief.
    Only the skins of the loaded mission work. A new skin shows the next time you respawn.

  To test on one PC: open CoOpEvolved.exe a second time; it opens as "Join a game" by itself
  (IP 127.0.0.1). Only one host can run per PC.

FRIENDS (players 2, 3 and 4)
----------------------------
You need: Windows 10/11 with an NVIDIA GPU and up-to-date drivers, Radmin VPN on the host's
network, and an Xbox controller or keyboard and mouse. You do NOT need Halo.

  1. Open CoOpEvolved.exe (if Windows shows "Windows protected your PC": More info -> Run
     anyway). If it shows the host panel, choose "Join a game" at the top right.
  2. Type the host's Radmin IP and your name; pick your armor color and skin.
  3. Choose your player number (each friend a different one, always the same):
       Player 2 = top right    Player 3 = bottom left    Player 4 = bottom right
  4. Choose "Controller" or "Keyboard and mouse" and press "Connect".
  5. When the host opens the game, the window switches to full screen with your video.

  F2 ............. menu (armor color, skin, volume, sensitivity, controls, disconnect)
  F1 ............. stats (ping, video FPS, losses)
  F3 ............. mute / unmute the game sound
  F11 / Alt+Enter  full screen or window
  Esc ............ release the mouse (keyboard and mouse)
  + and - ........ mouse sensitivity (0.1x to 99x)

With KEYBOARD AND MOUSE: click the video to capture the mouse.
"Direct mouse aim (beta)" (menu or F2): the mouse turns the camera directly, like Halo on PC,
instead of moving the controller's stick. If the host cannot use it yet, the stick is used.

  WASD ............ move               Mouse ............ aim
  Left click ...... shoot              Right click ...... grenade
  Space ........... jump               E ................ action (RB): vehicles, pick up
  R ............... reload             F or V ........... melee
  Q or wheel ...... switch weapon
  G ............... switch grenade     Ctrl or C ........ crouch
  Z or middle click zoom               1-4 or arrows .... d-pad (flashlight, etc.)
  Enter ........... pause              Tab .............. scoreboard

Problems
--------
  "The host is not responding": check the IP, that Radmin VPN is connected (ping the host
     from Radmin) and that the host has Co Op Evolved open in "Host a game".
  "Waiting for video...": the host has not opened MCC yet, or the mod is not installed in
     their game (their panel says so).
  "That player is already taken": another friend picked your number; pick another one.
  "Lost the connection to the host": the host closed or Radmin dropped; press Connect again.
  Laptop with no video / NVIDIA error: in Windows Settings > Display > Graphics set
     CoOpEvolved.exe to "High performance".
  A character does not move: the host's game must be in the foreground on their PC.

Settings are saved in coopevolved-host.ini / coopevolved-cliente.ini and what happens in
coopevolved-host.log / coopevolved-cliente.log, all in this folder. The logs contain names
and Radmin IPs: only send them privately.

Full instructions: README.md in the repository (Español: LEEME.txt).
