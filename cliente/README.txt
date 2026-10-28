HALO1 ONLINE CO-OP - CLIENT
===========================

HaloCoopClient.exe is the app for players 2, 3 and 4: it shows your part of the game full
screen with the game sound, and sends your controller (or keyboard and mouse) to the host's
PC through Radmin VPN. Your name shows over your character in everyone's screen, with the
armor color you pick in "Armor color" (you can also change it in F2 while playing).

The app is in English. To use it in Spanish pick "Español" at the top right (it is saved).

You need
--------
  - Windows 10/11 with an NVIDIA GPU and up-to-date drivers.
  - Radmin VPN joined to the same network as the host.
  - The host's Radmin IP: the host gives it to you (shown at the top of their HaloCoop Host).
  - Xbox controller (recommended) or keyboard and mouse.

How to play
-----------
  1. Copy this folder anywhere you like and open HaloCoopClient.exe.
     (If Windows shows "Windows protected your PC": More info -> Run anyway.)
  2. Type the host's Radmin IP and your name.
  3. Choose your player number (each friend a different one, always the same):
       Player 2 = top right quadrant
       Player 3 = bottom left quadrant
       Player 4 = bottom right quadrant
  4. Choose "Controller" or "Keyboard and mouse" and press "Connect".
  5. When the host opens the game, the client switches to full screen with your video.

Client keys
-----------
  F2 ............. menu (volume, sensitivity, controls, disconnect)
  F1 ............. stats (ping, video FPS, losses)
  F3 ............. mute / unmute the game sound
  F11 / Alt+Enter  full screen or window
  Esc ............ release the mouse (keyboard and mouse)
  + and - ........ mouse sensitivity (0.1x to 99x)

With KEYBOARD AND MOUSE: click the video to capture the mouse.

  WASD ............ move               Mouse ............ aim
  Left click ...... shoot              Right click ...... grenade
  Space ........... jump               E or R ........... action / reload
  F or V .......... melee              Q or wheel ....... switch weapon
  G ............... switch grenade     Ctrl or C ........ crouch
  Z or middle click zoom               1-4 or arrows .... d-pad (flashlight, etc.)
  Enter ........... pause              Tab .............. scoreboard

Problems
--------
  "The host is not responding": check the IP, that Radmin VPN is connected (ping the host
     from Radmin) and that the host has HaloCoop Host open.
  "Waiting for video...": the host has not opened MCC yet, or the mod is not installed in
     their game (their panel says so).
  "That player is already taken": another friend picked your number; pick another one.
  "Lost the connection to the host": the host closed or Radmin dropped; press Connect again.
  Laptop with no video / NVIDIA error: in Windows Settings > Display > Graphics set
     HaloCoopClient.exe to "High performance".
  My character does not move: the host's game must be in the foreground on their PC.

Your data (host IP, name, etc.) is saved in halocoop-cliente.ini and what happens in
halocoop-cliente.log (send it to the host if something fails), both in this folder.

Español: LEEME.txt
