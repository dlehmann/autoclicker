# Autoclicker

A small auto clicker for Linux with KDE Plasma. It clicks on a previously
selected spot on the screen at an adjustable rate and stops immediately as
soon as you move the mouse.

Written in C++ with Qt 6. Works on Wayland and X11. The user interface follows
the system language: German or English (English is the fallback).

![Icon](packaging/autoclicker.svg)

## Features

- **Pick the target with a crosshair:** The screen is dimmed and a crosshair
  with a coordinate display appears, across multiple monitors.
- **Click rate** from 1–50 clicks per second or 1–600 clicks per minute.
  Switching the unit converts the value.
- **Stop after N clicks** (0 = unlimited).
- **Stop on mouse movement:** The real mouse always wins. Moving it ends the
  clicking immediately.
- **2-second countdown** before starting, so you can let go of the mouse.
  Clicking the countdown cancels it.
- **Always on top**, with a status bar showing state, target and click count.
- **No setup, no root:** Everything runs with normal user permissions.

## Requirements

- **KDE Plasma 6** (Wayland or X11). Mouse movement detection and
  "always on top" are implemented with KWin scripts, so the auto clicker does
  not work on GNOME or other desktops.
- **Write access to `/dev/uinput`** for the logged-in user. On KDE this is
  usually already set up, e.g. by KDE Connect or Steam. You can check it with:

  ```bash
  getfacl /dev/uinput | grep "user:$USER"
  ```

## Installation (AppImage)

Ready-to-use AppImages that bundle all Qt libraries are available under
[Releases](../../releases). Download, make executable, run:

```bash
chmod +x Autoclicker-*-x86_64.AppImage
./Autoclicker-*-x86_64.AppImage
```

The AppImage is built on Debian 13 and runs on systems with glibc 2.41 or
newer, for example Arch, CachyOS, Fedora 42, openSUSE Tumbleweed,
Kubuntu 25.04 and Debian 13.

If FUSE is not available on your system, you can run it without:

```bash
./Autoclicker-*-x86_64.AppImage --appimage-extract-and-run
```

Note: The AppImage does not ship KDE's Breeze style and uses Qt's default
"Fusion" style instead. Functionality is the same.

## Usage

1. Click **Select target**, then click on the desired spot. Esc cancels the
   selection.
2. Enter the **click rate** and choose **per second** or **per minute**. After
   every program start the rate is set to 5 clicks per second.
3. Optionally set **Stop after** a number of clicks.
4. Click **Start clicking** and keep the mouse still during the countdown.
5. **Move the mouse** to stop clicking.

The mouse pointer jumps to the target on every click. The target is
intentionally not saved and has to be selected again after each start.

## Building from source

### Dependencies

Arch / CachyOS:

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-wayland qt6-tools layer-shell-qt
```

Debian 13 / Ubuntu 25.04:

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-base-private-dev qt6-wayland qt6-tools-dev qt6-l10n-tools qt6-translations-l10n liblayershellqtinterface-dev layer-shell-qt
```

### Build and run

```bash
cmake -B build
cmake --build build -j
./build/autoclicker
```

Install (including start menu entry and icon):

```bash
sudo cmake --install build
```

### Building the AppImage locally

The script [`packaging/build-appimage.sh`](packaging/build-appimage.sh) builds
the AppImage with [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy).
For the result to run on as many systems as possible, build it in a Debian 13
container:

```bash
docker run --rm -v "$PWD":/src -w /src debian:trixie bash -c '
  apt-get update &&
  apt-get install -y --no-install-recommends build-essential cmake file wget ca-certificates \
    qt6-base-dev qt6-base-private-dev qt6-wayland qt6-tools-dev qt6-l10n-tools \
    qt6-translations-l10n liblayershellqtinterface-dev layer-shell-qt libgl-dev &&
  VERSION=dev packaging/build-appimage.sh'
```

## Releases

A release is created automatically whenever a tag with a version number is
pushed. The GitHub Action [`release.yml`](.github/workflows/release.yml) then
builds the AppImage and attaches it to a new release.

```bash
git tag -a v1.0.0 -m "Autoclicker 1.0.0"
git push origin v1.0.0
```

Tags with a suffix such as `v1.1.0-rc1` are published as pre-releases.

## Translations

English is the source language in the code. Translations live in
[`i18n/`](i18n) as Qt Linguist files and are embedded into the program. At
startup the app picks the first language from the system settings for which
a translation exists; if English comes first or none matches, it stays
English.

After changing texts in the code, update the translation files and fill in
the new entries (for example with Qt Linguist):

```bash
cmake --build build --target update_translations
linguist i18n/autoclicker_de.ts
```

To add another language, add e.g. `i18n/autoclicker_fr.ts` to `TS_FILES` in
[`CMakeLists.txt`](CMakeLists.txt) and run `update_translations`.

## How it works

| Task | Implementation |
|---|---|
| Sending clicks | Virtual pointer device via `/dev/uinput` with absolute coordinates; works in all applications, including on Wayland |
| Selecting the target | Semi-transparent overlay per monitor using LayerShellQt |
| Detecting mouse movement | A KWin script reports via D-Bus as soon as the pointer leaves the target |
| Always on top | A KWin script sets "keep above others" for the app's own window |
| Timing | Dedicated thread with a fixed rate; button hold time is 50 ms, at high rates at most a quarter of the click interval |

The KWin scripts are loaded on start and removed again on exit.

### Source code

| File | Contents |
|---|---|
| [`src/MainWindow.cpp`](src/MainWindow.cpp) | Main window, states, settings |
| [`src/TargetPicker.cpp`](src/TargetPicker.cpp) | Crosshair overlay |
| [`src/VirtualPointer.cpp`](src/VirtualPointer.cpp) | uinput device: move the pointer and click |
| [`src/ClickWorker.cpp`](src/ClickWorker.cpp) | Click loop with timing |
| [`src/MouseWatcher.cpp`](src/MouseWatcher.cpp) | Movement detection via KWin |
| [`src/KWinScript.cpp`](src/KWinScript.cpp) | Loading and removing KWin scripts |

### Test and developer options

```bash
./build/autoclicker --test-move                    # move the pointer by 10 px and verify its position
./build/autoclicker --test-click X Y               # click once at X,Y
./build/autoclicker --screenshot image.png [X Y]   # save the window as an image (optionally with a target)
```

## License

[MIT](LICENSE)
