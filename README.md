# minir
minir is a libretro frontend intended to complement RetroArch, by focusing on the usecases RetroArch is not focused on.

It's intended to match or exceed the power and usability of bsnes-qt (preferably exceed), both for beginners and advanced users.
It does not intend to compete with RetroArch's feature set or portability; if you want the most advanced features, use RetroArch.

![Screenshot](https://github.com/Alcaro/minir/blob/master/scr/lin1.png) ![Screenshot](https://github.com/Alcaro/minir/blob/master/scr/lin3.png)

![Screenshot](https://github.com/Alcaro/minir/blob/master/scr/win.png)

# libretro
The core of minir is a system known as [libretro](http://libretro.com/), which allows many different
games and emulators to be built as shared libraries (known as 'libretro core'), which can then be
used by several different libretro frontends.

The biggest advantage of libretro over standalone games/etc is that a feature (for example real-time
rewind, or being ported to GameCube) can be written only once, and all the cores will benefit.
While the feature would need to be implemented in multiple frontends, there are far fewer frontends than cores,
and some features don't make sense in all fronts (while most features do make sense with most cores).

# RetroArch
RetroArch was the first libretro frontend to be created, and still remains the main driving force
behind libretro. While it is a powerful device, the author of minir does not share its design goals.

 | RetroArch | minir
----- | ----- | ------
Prefered setup | HTPC in fullscreen (many others supported) | Desktop computers, windowed mode
Input method | A gamepad (DualShock, Xbox, etc); use with keyboard and mouse is awkward | Keyboard and mouse, gamepads not supported (yet?)
Menus | Custom | System native
Core support | Full (limited by system hardware) | Limited (currently)
Automatic core selection | ? (it's been mentioned) | Yes
Major features | Real-time rewind, Dynamic Rate Control, user-defined shaders, netplay, low-latency fullscreen, ... | Real-time rewind, maximally configurable hotkeys, (more to come)
(Note that this table is based on the minir author's views; the RetroArch authors' views may differ.)

List of features and planned features where the idea is from RetroArch:
- libretro itself, of course
- Dynamic Rate Control (unimplemented)
- Real-time rewind
- Rewind-based netplay (unimplemented)
- Custom shaders (unfinished)

# Compiling
## Linux
./configure && make. Other Unix-likes are not supported.

On Debian, Ubuntu and derivates, you need the following: `libgtk-3-dev libpulse-dev libgl1-mesa-dev mesa-common-dev`

## Windows - MinGW
The author recommends [MinGW-w64](http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/installer/mingw-w64-install.exe/download),
with CPU x86_64, threads Win32, any exception model and version, and [these cores](http://buildbot.libretro.com/nightly/windows/x86_64_w32/latest/), to avoid requiring libwinpthread-1.dll nearby.
This may require using an older version of MinGW, because Win32 threads are sometimes a few versions late.

Everything else is included with MinGW, at least the version I tried (5.2.0-rev0). If (and ONLY IF, don't do it "just in case") you get errors,
see [this](http://wayback.archive.org/web/20100405012103/http://byuu.org/bsnes/compilation-guide)
(follow either the OpenGL or Direct3D sections depending on what errors are thrown).
video-direct3d.cpp may also throw an error about IDirect3D9Ex and RegisterSoftwareDevice; see comments there for further instructions.

Once you've downloaded everything you need, use configure.bat then mingw32-make.

## Windows - MSVC
To compile for Windows with MSVC, use mingw32-make -f Makefile.msvc from a MSVC command prompt (run any vcvars*.bat). MSVC 2008 and higher should work. configure.bat is not required.
This configuration is considerably less tested than MinGW, and may be broken for extended periods of time.

Like older MinGWs, older (or possibly even less old) MSVCs may require some fixes to the header files. See MinGW instructions for further details.
