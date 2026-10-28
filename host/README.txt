HALO1-COOP - HOST
=================

Contents:
  HaloCoopHost.exe  -> creates the controllers for players 2, 3 and 4, receives their input
                       and shows the host panel.
  VERSION.dll       -> the mod inside the game (captures each friend's video).
                       HaloCoopHost.exe copies it into the game folder by itself.

Before the first time:
  1. AlphaRing: download Release1.0.zip (1.3528.0.0) from
       https://github.com/wouter51/AlphaRing/releases
     and copy WTSAPI32.dll into the game's mcc\binaries\win64 folder.
     To find it: in Steam, right-click Halo: The Master Chief Collection ->
     Manage -> Browse local files, then open mcc -> binaries -> win64.
  2. ViGEmBus: install the driver from
       https://github.com/nefarius/ViGEmBus/releases/latest
     and restart the PC.

Every time you play:
  1. Unplug physical controllers.
  2. Open HaloCoopHost.exe FIRST, with MCC closed (do not close it while you play).
     The first time, if Windows asks about the firewall, tick private and public networks.
  3. Give your friends the Radmin IP shown at the top of the panel.
  4. Press "Abrir MCC" (Open MCC) and choose without anti-cheat.
  5. F4 -> Splitscreen -> Players = 4. Close the menu with F4.
  6. In the mission use classic graphics (Tab) and keep the game as the active window.

In the panel (the interface is in Spanish):
  - "Video": ~10 s after MCC opens it says "listo" (ready) and the FPS/Mbps each friend gets.
  - "Video a 30/60/120 FPS": more FPS = less delay but more upload bandwidth
    (about 10 Mbps per friend at 60 FPS).
  - "Audio: activo" (active): MCC's sound (only the game's) is being sent to everyone.
  - "Mover" (Move): tests that player's controller.  "Expulsar" (Kick): disconnects them.

Full instructions are in the repository's README.md (Español: LEEME.txt).
