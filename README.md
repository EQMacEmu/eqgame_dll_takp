# TAKP eqgame.dll that works with [eqw_takp](https://github.com/CoastalRedwood/eqw_takp)

This is meant for playing the old TAKP client on Windows 10/11.  This client is used for TAKP and other servers derived from it.
To use this mod, download the TAKP client and replace `eqw.dll` with the version from [eqw_takp](https://github.com/CoastalRedwood/eqw_takp) and replace `eqgame.dll` with the version from this repository.  They are intended to be used together.

If you have a high DPI display, the first time you run a program windows will apply scaling to it to make it bigger and it will save this setting for that file path.
The recommended `eqgame.exe` and `eqgfx_dx8.dll` files are the ones from the TAKP 2.1 client zip as they have been modified to address this issue.
If you have an already existing install, make sure you have the scaling set right:
Right click eqgame.exe, properties, compatibility tab, change DPI settings, check the box "Override high DPI scaling behavior" and select "Application" in the dropdown.

Depending on your system, GPU, drivers, etc. you will most likely need a Direct3D 8 wrapper.  These are mods that work by placing a `d3d8.dll` file into the game directory.  Don't install these in the system directories.
Try it without any `d3d8.dll` file first to see if that works for you.  You may get drastically better or worse performance with or without a D3D8 mod and it's worth playing around with these even if seems to be working already.
- [dgVoodoo2](https://dege.freeweb.hu/dgVoodoo2/) - this is included in the TAKP 2.1 download.  It has some configuration options and is updated frequently by the author.  Try the latest version, maybe try old versions to isolate problems.
- [d3d8to9](https://github.com/crosire/d3d8to9) - simpler, can work better in some cases.

---

## obsolete eqclient.ini settings
- `Options -> WindowedMode` eqw_takp handles this now.
- `Options -> MouseRightHanded` handled by eqw_takp's `EqwGeneral -> SwapMouseButtons`
- `VideoMode -> RefreshRate` not used by windowed application

## new eqclient.ini settings
- `eqgame_dll -> EnableDeviceGammaRamp` can be set to TRUE if you want the game's gamma slider to change the global desktop gamma.  Note that this is unreliable and ugly but this feature was present in the old eqw program.
- `eqgame_dll -> DisableCpuTimebaseFix` eqw_takp also has this fix, you only need one of them enabled but it's fine to enable it in both.
- `eqgame_dll -> EnableFPSLimiter` enable this for an FPS limiter mod.  You may have better results using your GPU control panel or an app like RivaTuner, but this was available in the old eqgame and is brought over for feature parity.

## List of mods in this eqgame.dll

### CPU Timebase
CPU high clock speed overflow fix.  If you have a CPU that's more than 4.2 Ghz you probably need this to make the game run at the right speed.
This issue happens with AMD Ryzen 7xxx and 9xxxx CPUs at least and probably many other modern processors.  The game normally samples the TSC by using the `rdtsc` instruction.  This mod makes it use a modern API called `QueryPerformanceCounter`.

### Skip license screen and splash screen
Faster startup

### Window Title renaming
Rename windows to Client1, Client2, etc.  You can set a custom title with the command line option `/title:CustomTitleHere`.

### MGB for Beastlords
Adds the AA button Mass Group Buff for Beastlord class.

### Level of Detail option
Fix bug with Level of Detail setting always being turned on ignoring user's preference.  Normally the game turns it back on every time you zone even if you disabled it - this mod makes it honor the user's preference.

### UI Skin loading option window
Fix bug with option window UI skin load dialog always loading default instead of selected skin

### Spell casting timer window
Fix bug with spell casting bar not showing at high spell haste values

### Command line parsing
Adds some command line options:
- `/ticket:username/password` for providing login credentials on command line instead of typing into front end UI.
- `/title:MyWindowTitle` for renaming window

### Chat /command parsing
Adds some custom commands:
- `/fps` - show an FPS indicator
- `/raiddump` or `/outputfile raid` - write raid members to file `RaidRoster-%s.txt`

### Desktop Gamma
Adjusts the global desktop gamma ramp with the in game slider, a hack that was present in the old eqw.  Needs to be enabled in the ini file.

### FPS Limiter
Enable this for an FPS limiter mod.  You may have better results using your GPU control panel or an app like RivaTuner, but this was available in the old eqgame and is brought over for feature parity.
	The settings for this are under `Options`:
- `MaxFPS` - regular foreground FPS cap, set to 0 to disable this limiter
- `MaxBGFPS` - background FPS cap, set to 0 to disable this limiter
- `MaxMouseLookFPS` - this cap is active when holding the right mouse button to mouselook.  set to 0 to disable this limiter

### Alt-enter maximize stretch mode
This is integrated with eqw_takp to maximize and stretch the contents of the window to fill the screen without regard for aspect ratio or how it looks.  
This is provided for feature parity with the old version but most of the time you will get much better results by setting the Width/Height to your desktop resolution instead.  
There are mods that provide double sized UI elements including text and chat.

### Bypass filename check
Disables a check that validates the game exe file hasn't been renamed from eqgame.exe.

### Character save bypass
Disables sending character data with OP_Save as the server doesn't use it.

### No items on corpse fix
A fix for a bug that can cause looted corpses to appear empty.

### Ignore OP_ShopDelItem on old UI
Stone UI has an error with this packet.

### EXE file checksum
The game sends the checksum of the program file to the server.  The `eqmac.exe` file is actually the Intel Macintosh `EverQuest` program and this mod calculates the checksum of that file instead of `eqgame.exe`.

### INI file forced settings
The following settings are set by the mod:
- `Defaults -> VideoModeBitsPerPixel = 32` 
- `VideoMode -> BitsPerPixel` set to 32 if it's set to 16
- `Defaults -> ChatKeepAlive = 1`
