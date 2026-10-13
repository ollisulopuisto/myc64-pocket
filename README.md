# Analogue Pocket - MyC64

The MyC64 core is my go at writing a FPGA based C64 emulator. It is a project I
started during the summer of 2020, at that point targeting the [ULX3S
board](https://radiona.org/ulx3s/). Initially it made good progress but then
nothing happened for a long long time.

Years later the [Analogue Pocket](https://www.analogue.co/pocket) came along
with its [openFPGA framework](https://www.analogue.co/developer) and slightly
after Christmas 2023 I decided to get started on its current incarnation.

## What can it do?

Have a look at this [Youtube
playlist](https://youtube.com/playlist?list=PLZUvG8cL98Z_DWweg3JRiITaK35yoCMK0&si=WcyRysbsj9mhh51e)
(that I will try to keep up-to-date when something noteworthy is added).

In short though, the core is still work-in-progress but quite a lot of things
actually do work (plus minus some emulation discrepancies).

## Setup instructions

Simply unzip the `MyC64-Pocket.zip` (from releases) in the root directory of
the Analogue Pocket's SD card.

### System ROMs

For legal reasons all ROMs are kept outside the FPGA bitstream distributed
here. The Commodore ROMs (three for the C64 and two for the 1541) can be
fetched with
```
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/basic.901226-01.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/characters.901225-01.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/kernal.901227-03.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1540-c000.325302-01.bin
wget http://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1541-e000.901229-01.bin
```
they then need to be placed in `/media/Assets/c64/common/` as follows
```
/media/Assets/c64/common/basic.bin
/media/Assets/c64/common/characters.bin
/media/Assets/c64/common/kernal.bin
/media/Assets/c64/common/1540-c000.bin
/media/Assets/c64/common/1541-e000.bin
```

## Supported formats

All assets are to be placed in `/media/Assets/c64/common/` or sub-directories
thereof.

### PRG

Standard `.prg` files are supported.

Select the desired `.prg` file from the **Core Settings->Load & Start PRG** and
the following sequence will be initiated

1. Core resets
2. Wait 2.5s (until booted)
3. Inject `.prg` into RAM
4. Inject a `RUN<RET>` key sequence

Suggestions are [Supermon+64](https://github.com/jblang/supermon64) and the
[High Voltage SID Collection (HVSC)](https://www.hvsc.c64.org/) converted to
[PSID64 format](https://boswme.home.xs4all.nl/HVSC/HVSC80_PSID64_packed.7z).

For games it is recommended to get hold of the *OneLoad64 collection* of 2100+
titles `OneLoad64-Games-Collection-v5.7z` (just google it). From what I can
tell the majority of the collection's crunched `.prg` files (found in
`AlternativeFormats/PRGs/Crunched/`) load up properly. It is important to pick
the crunched ones as the uncrunched will attempt to replace the entire RAM
which is not supported while the system is running.

The Pocket appears to truncate directories containing more than 1500 files so
indexing as below is suggested.

```
$ p7zip -d OneLoad64-Games-Collection-v5.7z
$ cd AlternativeFormats/PRGs/Crunched/
$ for letter in {a..z}; do mkdir ${letter^}; mv ${letter}*.prg ${letter^}/; mv ${letter^}*.prg ${letter^}/; done
$ for digit in {0..9}; do mkdir ${digit}; mv ${digit}*.prg ${digit}/; done
```

Of course as the core is in development not every `.prg` will function
correctly. So to avoid frustration and help new users out we have the following
wiki page [Working
PRGs](https://github.com/markus-zzz/myc64-pocket/wiki/Working-PRGs) that anyone
(on github) can edit. Please feel free to update it with your findings!

### CRT

Currently the following cartridge formats are supported

- *Magic Desk* - as defined in the `.crt`
  [format specification](https://ist.uwaterloo.ca/~schepers/formats/CRT.TXT).
  This is the main format used by the `.crt` files in the *OneLoad64* games
  collection. The cartridge is limited to 128KB of storage.
- *EasyFlash* - as defined in [EasyFlash Programmer’s Guide](http://skoe.de/easyflash/files/devdocs/EasyFlash-ProgRef.pdf).
  The secondary format used by some of the larger games in the *OneLoad64*
  collection.  A modern cartridge that can hold up to 1MB of storage. Seemingly
  the preferred cartridge format for new content.

Select the desired `.crt` file from the **Core Settings->Load & Start CRT**
browser. The emulated cartridge will be inserted into the system and the C64 is
automatically reset to boot up its contents.

Cartridges of unsupported formats are silently ignored (and no reset is
triggered).


### D64 & G64 Disk Images

The core features full, native in-core `.d64` support with on-the-fly GCR bitstream synthesis in hardware, as well as raw `.g64` image support!

* **Native `.d64` loading**: Simply place your `.d64` images in `/media/Assets/c64/common/` (or subdirectories). Select **Load & Mount D64** from the core menu.
* **Automatic Fast Boot**: Inserting a `.d64` automatically resets the C64 to clean BASIC prompt and injects `LOAD "*",8,1` followed by `RUN`.
* **Track / Motor Status Bar**: The core displays an unobtrusive track and drive activity HUD whenever the 1541 disk drive is seeking.

## Pocket Controls & In-Game Shortcuts

### Control Modes (Core Menu)
Principle: face buttons on their own are joystick only and never type keys, so they can be mashed freely in a game. Keys and typed commands need L or R held (below).

* **Standard (D-pad & B)**: Classic joystick. A, X and Y fire (A is where the Pocket's own accept button is); D-pad Up or B jumps (joystick up).
* **Platformer (Btn B Jump)**: Pure button jump layout for platformers (*Giana Sisters*, *Turrican*, *Mayhem in Monsterland*). Button B is exclusively Jump (Up), disabling D-pad Up jump.
* **Driving (L=Brake, R=Gas)**: Dedicated racing layout. Right trigger is Gas (Up), Left trigger is Brake/Reverse (Down); all four face buttons fire.

### Shortcuts & Port Swap
* **Hold L + Press Select (Joystick Port 1/2 Swap)**: Swaps joystick ports on the fly with a clear on-screen OSD notification (`[C64 JOYSTICK PORT 1/2 ACTIVE]`).
* **Hold L-Trigger (Shortcut Layer 1)**:
  * D-pad: `F1` (Up), `F3` (Right), `F5` (Down), `F7` (Left)
  * Face buttons: `Space` (A), `Return` (B), `Run/Stop` (X), `1` (Y)
* **Hold L + R Triggers (Shortcut Layer 2)**:
  * D-pad: `F2` (Up), `F4` (Right), `F6` (Down), `F8` (Left)
  * Face buttons: `Space` (A), `N` (B), `Y` (X), `2` (Y)
  * Start: `Commodore (C=)` key
* **Hold R (typed commands)**: types a BASIC command at the prompt
  * A: `LOAD"*",8,1` + RETURN
  * B: `RUN` + RETURN
  * X: `LIST` + RETURN
  * Y: `LOAD"$",8` + RETURN (disk directory)
* **Base Face Buttons** (no L/R held):
  * Buttons A, X, Y: Fire
  * Button B: Jump (Up); fire in Driving mode

##### Windows

You can download pre-build binaries for windows from [c64preservation.com
files](https://c64preservation.com/files/nibtools/).

##### Linux

For Linux it is obtained and built as follows

```
$ git clone --recurse-submodules https://github.com/OpenCBM/OpenCBM.git
$ cd OpenCBM
$ make -f LINUX/Makefile
```

##### MacOSX

On MacOSX you can auto-build nibtools using [Homebrew](https://brew.sh) and
[hitorisensei/homebrew-taps/nibtools](https://github.com/HitoriSensei/homebrew-taps/nibtools)
tap

```
$ brew install hitorisensei/homebrew-taps/nibtools
```

#### Loading G64 images

Select the desired `.g64` file from the **Core Settings->Load G64 Slot**
browser and the disk image will be inserted into the emulated 1541 floppy
drive.

At this point the user needs to do normal interaction such as `LOAD"$",8`
followed by a `LIST` to get a directory listing or simply a `LOAD"*",8`
followed by a `RUN` to load and run the first program of the disk.

Note that on the C64 keyboard `"` is `<SHIFT>+2` and `$` is `<SHIFT>+4` (read
the section about sticky keys for how to access the shift modifier on the
virtual keyboard).

### Cartridges
Supported `.crt` types: Magic Desk, EasyFlash, Action Replay and Retro Replay (64K, normal mode; no flash mode or Nordic Replay). **L + R + Select** is the freeze button for Action Replay and Retro Replay. Warp stays off while a cartridge is mapped in; a Retro Replay that has switched itself off (e.g. after starting a game) no longer blocks it.

### Warp (fast disk loading)
Core menu **Warp (fast load)**: *Auto while loading* (default) or *Off*. In Auto, once the 1541 motor has run for ~20 ms the whole C64 and drive run 4x faster, together, so custom fastloaders and copy protection keep working. The screen goes black (the drive status bar shows `WARP`) and sound is muted; ~250 ms after the motor stops the picture returns. Pressing any button or key during warp cancels it until the motor stops. Warp is not used while a cartridge is inserted.

## General usage

The gamepad is mapped to the C64 joystick port as follows

- **dpad** - as expected
- **face-a, face-x, face-y** - fire button
- **face-b** - jump (joystick up)
- keys (Space, Return, ...) with L held, see above

Pressing **left-of-analogue** brings up (toggles) the on-screen-display
containing a virtual keyboard and a small menu system for settings.

For the virtual keyboard the **face-x** button toggles sticky keys (useful for
modifiers such as shift) and the **face-y** button clears any sticky key.
Normal key press is accomplished with the **face-a** button.

Pressing **trig-l1** and **trig-r1** navigates through the different menu tabs.

Controllers can be mapped to C64 joystick ports in the **MISC** menu tab and
there you will also find a reset button for the emulator core.

A physical keyboard is supported while docked. The keyboard is expected to show
up on the third input (i.e. `cont3_joy`). For details on the key mapping see
table `hid2c64` in source file `src/bios/main.c`.
