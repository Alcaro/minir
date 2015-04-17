# minir
minir is a libretro frontend intended to complement RetroArch, by focusing on the usecases RetroArch is not focused on.

It's intended to match or exceed the power and usability of bsnes-qt (preferably exceed), both for beginners and advanced users.

# libretro
The core of minir is a system known as [libretro](http://libretro.com/), which allows many different
games and emulators to be built as shared libraries (known as 'libretro core'), which can then be
used by one of a large number of libretro frontends.

The biggest advantage of libretro over standalone games/etc is that a feature (for example real-time
rewind, or being ported to GameCube) can be written only once, and all the cores will benefit.

# RetroArch
RetroArch was the first libretro frontend to be created, and still remains the main driving force
behind libretro. While it is a powerful device, the author of minir does not share its design goals.

 | RetroArch | minir
----- | ----- | ------
Prefered setup | HTPC in fullscreen (many others supported) | Desktop computers, windowed mode
Input method | A gamepad (DualShock, Xbox, etc); use with keyboard and mouse is awkward | Keyboard and mouse, gamepads not supported (yet)
Menus | Custom | System native
Core support | Full (limited by system hardware) | Limited (currently)
Automatic core selection | ? (it's been mentioned) | Yes
Major features | Real-time rewind, Dynamic Rate Control, user-defined shaders, netplay, low-latency fullscreen, ... | Real-time rewind, maximally configurable hotkeys, (more to come)
(Note that this table is based on the minir author's views; the RetroArch authors' views may differ.)

List of features (including unimplemented ones) where the idea is from RetroArch:
- libretro itself, of course
- Dynamic Rate Control (unimplemented)
- Real-time rewind
- Rewind-based netplay (unimplemented)
- Custom shaders (unfinished)
