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
Nothing else needs installing (the 4 players are built in, no extra drivers).

HOST (player 1)
---------------
You need: MCC 1.3528 on Steam, an NVIDIA GPU, Radmin VPN and about 10 Mbps of upload per friend.

  1. Open CoOpEvolved.exe and choose "Host a game". With MCC closed it installs or updates
     the mod in the game by itself. Do not close it while you play.
     The first time, if Windows asks about the firewall, tick private and public networks.
  2. Give your friends the Radmin IP shown at the top ("Copy" copies it).
  3. In the player 1 card type your name in "Nickname" and pick your "Armor" color and "Skin".
  4. Press "Open MCC" and choose "Play Halo: MCC (Anti-Cheat Disabled)".
  5. Start a Halo CE campaign mission from the MCC menu: it gets player 1 plus every
     friend up to the highest connected number (2, 3 or 4 players; ask friends to use
     2, 3 and 4 in that order). The screen is split for that many players.
  6. Use classic graphics (Tab) and keep the game as the active window.
  You play with keyboard and mouse.

  Normal MCC (with the anti-cheat) does not start while the mod is in the game (error
  0xc000007b). Close MCC and press "Remove mod" in the panel. "Install mod" puts it back
  (opening Co Op Evolved as host also installs it by itself).

  F2 in game (or hold View on the controller 1 s): menu over the game with your armor color,
  skin, F6, subtitles, host options, Kick and Reload Mission. Only you see it. F2 or B closes it.
  Armor colors only paint Master Chief: other skins keep their own colors.

  In the panel:
  - Dots at the top: green = ready (game mod, 4 players, MCC, audio, video).
  - "Video at 30/60/120 FPS": more FPS = less delay but more upload bandwidth.
  - "Show names over players": if they do not sit right, set "Game FOV" to the game's FOV.
  - "Player 1 full screen (F6 in game)": you see only your part, stretched to your monitor.
  - "Full-screen cutscenes for friends": during cutscenes each friend sees the whole screen.
  - "Subtitles for each player": copies MCC's subtitle line into every player's part.
  - "Hide MCC subtitles": hides them for everyone (friends cannot turn them on).
    Takes effect when a mission starts; your MCC settings are not changed.
  - Armor color: the Halo CE colors, "More colors..." (any color) and "Rainbow (Gaylo)".
  - Admin panel, one card per player: name, IP, inputs per second and video of each friend.
    "Move": tests that player's controls.  "Kick": disconnects them.
  - "Skin": look like another character (Marine, Elite...) while still being Master Chief.
    Only the skins of the loaded mission work. A new skin shows the next time you respawn.

  A friend who connects after the mission started watches the whole screen, and you see
  "friend(s) waiting to join" at the top. To let them in: F2 > "Reload Mission" (the
  mission starts again from the beginning with everyone). Restarting from MCC's pause
  menu keeps the same players.

  Players 3 and 4: the missions only have places for 2 players, so the mod puts player 3
  next to player 1 and player 4 next to player 2 when the mission starts, takes them along
  in Pelicans and other vehicles, and makes them follow the mission's scripted moves.
  Dead players come back in about 3 seconds, even during combat. Extra vehicles: a second
  Warthog in "Halo" with 4 players, and one more Banshee per extra player in "Two Betrayals".

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
     If you connect after the host started the mission, you watch the whole screen
     until the host reloads it (F2 > Reload Mission).

  F2 ............. menu (armor color, skin, volume, subtitles, sensitivity, controls, disconnect)
  F1 ............. stats (ping, video FPS, losses)
  F3 ............. mute / unmute the game sound
  F11 / Alt+Enter  full screen or window
  Esc ............ release the mouse (keyboard and mouse)
  + and - ........ mouse sensitivity (0.1x to 99x)

With KEYBOARD AND MOUSE: click the video to capture the mouse.
"Direct Mouse Aim (Mouse Support)" (menu or F2): the mouse turns the camera directly, like Halo on PC,
instead of moving the controller's stick. If the host cannot use it yet, the stick is used.
It turns on when your character appears ("Mouse enabled: you can aim with the mouse").
"Subtitles" (F2): the subtitle line in your part of the screen; untick it to hide it.

  WASD ............ move               Mouse ............ aim
  Left click ...... shoot              Right click ...... grenade
  Space ........... jump               E ................ action (RB): vehicles, pick up
  R ............... reload             F or V ........... melee
  Q or wheel ...... switch weapon
  G ............... switch grenade     Ctrl or C ........ crouch
  Z or middle click zoom               1 or Up .......... flashlight
  Tab ............. scoreboard         2-4 or other arrows d-pad

The keys work the same whatever controller layout the host picked in MCC.
Only the host (player 1) can pause: MCC's pause menu only answers to them.

Problems
--------
  "The host is not responding": check the IP, that Radmin VPN is connected (ping the host
     from Radmin) and that the host has Co Op Evolved open in "Host a game".
  "Waiting for video...": the host has not opened MCC yet, or the mod is not installed in
     their game (their panel says so).
  "That player is already taken": another friend picked your number; pick another one.
  "Different versions": you and the host must use the same version; download it again.
  "Lost the connection to the host": the host closed or Radmin dropped; press Connect again.
  Laptop with no video / NVIDIA error: in Windows Settings > Display > Graphics set
     CoOpEvolved.exe to "High performance".
  A character does not move: the host's game must be in the foreground on their PC.

Settings are saved in coopevolved-host.ini / coopevolved-cliente.ini and what happens in
coopevolved-host.log / coopevolved-cliente.log, all in this folder. The logs contain names
and Radmin IPs: only send them privately.

Full instructions: README.md in the repository (Español: LEEME.txt).

Privacy and legal
-----------------
No accounts, ads, analytics or servers of our own: everything travels only between the host and
the friends through your Radmin VPN network. Settings and logs (with the players' names and Radmin
IPs) stay in this folder; delete it to remove them.
Free fan project, not made, affiliated with or endorsed by Microsoft, Xbox Game Studios or Halo
Studios. Halo is a trademark of Microsoft. The host needs their own copy of MCC on Steam. Campaign
only, with MCC's anti-cheat disabled. Provided "as is", without warranty: use at your own risk.
