<div align="center">
  <img src="https://github.com/user-attachments/assets/44d0875e-d46f-40eb-88ea-ea89875f2f99" width="100px" alt="OpenRBX logo">

  [Discord] | [Contributing]
</div>

[Discord]: https://discord.gg/2JcBQFxVPv
[Contributing]: CONTRIBUTING.md

# OpenRBX

OpenRBX is a source port of Roblox as it was released in December 2007. It is built from the code recovered by [RBXDecomp](https://github.com/OpenRBX-org/RBXDecomp), and the goal is a modern, cross-platform codebase.

<img width="1919" height="1031" alt="Screenshot 2026-11-14 114011" src="https://github.com/user-attachments/assets/fed54e9f-48b7-46ef-9bc2-5b1b7d0a3137" />

## Getting OpenRBX

### Compiling from source

The client and the RBXGS web service build for Windows and Linux, and the client also builds for Android. Other platforms including macOS are planned.

| Platform | Toolchain | Notes |
|----------|-----------|-------|
| Windows (x64) | Visual Studio 2022 or later | Builds the client and the RBXGS web service. |
| Linux (x64) | GCC 14 or later | Builds the client and the RBXGS web service. |
| Linux (ARM64) | GCC 14 or later | Builds the client and the RBXGS web service. |
| Android (ARM64) | Android NDK r29, JDK 17 or later | Builds the client only, as an APK. Experimental. |

#### Prerequisites (Windows)

* [Git](https://git-scm.com/downloads)
* [Visual Studio](https://visualstudio.microsoft.com/) 2022 or later with the Desktop development with C++ workload
* [Strawberry Perl](https://strawberryperl.com/), used to build OpenSSL
* [Python](https://www.python.org/) with `pip install meson mako packaging pyyaml` and [WinFlexBison](https://github.com/lexxmark/winflexbison) on the `PATH`, used to build Mesa

#### Building (Windows)

Open an x64 Native Tools Command Prompt from the Visual Studio start menu folder, then:

```bat
:: Clone the repo
git clone https://github.com/OpenRBX-org/OpenRBX.git
cd OpenRBX

:: Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Once the build completes, the client (`Roblox.exe`) and the RBXGS web service (`WebService.exe`) are in the `build` folder.

#### Building (Linux)

Install Git, CMake, Ninja, Make, Perl, GCC 14 or later and the X11, EGL, GTK 3, WebKitGTK, PipeWire, PulseAudio and ALSA development packages. On Ubuntu 24.04:

```sh
sudo apt install git cmake ninja-build make perl g++-14 libgl-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libxt-dev libxaw7-dev libxxf86vm-dev \
  libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-icccm4-dev libxcb-image0-dev \
  libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev \
  libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libfontconfig1-dev libfreetype-dev libglib2.0-dev libgtk-3-dev libwebkit2gtk-4.1-dev libcups2-dev \
  libpipewire-0.3-dev libpulse-dev libasound2-dev libegl-dev libopengl-dev
```

Then:

```sh
git clone https://github.com/OpenRBX-org/OpenRBX.git
cd OpenRBX
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Once the build completes, the client (`Roblox`) and the RBXGS web service (`WebService`) are in the `build` folder with the libraries they load.

The client expects a `content` directory beside the executable for game assets.

On Linux, pass `-DOPENRBX_USE_SYSTEM_QT=ON` to use an installed Qt 6 instead of
building and bundling Qt. This requires the development packages for Qt
Widgets, Print Support, and Core5Compat, plus the Qt XCB platform plugin at
runtime. The ARM64 build was verified with Qt 6.11 on Fedora Asahi Remix 44.

#### Building (Android)

The Android build cross-compiles the bundled Qt, so it first needs a host build of the same
`3rdparty/qtbase` for Qt's tools (moc, rcc, uic and androiddeployqt):

```sh
cmake -S 3rdparty/qtbase -B qt-host-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/qt-host" -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF \
  -DFEATURE_opengl=OFF -DFEATURE_xcb=OFF -DFEATURE_dbus=OFF
cmake --build qt-host-build && cmake --install qt-host-build
```

Then, with an Android SDK (platform 37 and build-tools 37) and NDK r29:

```sh
cmake -B build-android -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DQT_HOST_PATH="$PWD/qt-host" \
  -DOPENRBX_ANDROID_CONTENT_DIR=/path/to/content
cmake --build build-android --target roblox_make_apk
```

The APK is written under `build-android/android-build-roblox/build/outputs/apk`. Sign it with your
own Android signing key before installing or sharing it. For an incremental debug build with an
existing deployment directory, copy the rebuilt native library before packaging:

```sh
cmake --build build-android --target roblox
cp build-android/libRoblox_arm64-v8a.so build-android/android-build/libs/arm64-v8a/
cd build-android
"$QT_HOST_PATH/bin/androiddeployqt" --input android-roblox-deployment-settings.json \
  --output android-build --apk android-build/roblox-debug.apk --builddir .
adb install -r android-build/roblox-debug.apk
```

Android has no
desktop OpenGL, so the client draws through [gl4es](https://github.com/ptitSeb/gl4es), which
translates it to OpenGL ES 2.0, and always uses the G3D renderer. `OPENRBX_ANDROID_CONTENT_DIR` is
optional and should stay unset for APKs you distribute. Without bundled content, the app asks on
first launch for a client content folder on the device (for example `data/clients/2007E/content`
from Novetus, or the folder containing it) and copies it into its data directory. Web pages are
shown as static HTML, without scripts.

The launcher opens an original obstacle course and starts solo play with automatic respawning.
Use File > Open to choose a compatible legacy XML `.rbxl` place, or a bzip2-compressed `.rbxl.bz2`
as distributed by Novetus, from Android storage. Selected documents are copied (and decompressed)
into app storage so Recent Files can reopen them. Tap Play in the game menu to
spawn a local character and run the open place, including automatic respawning after death.
Use the floating joystick on the left to move, the jump button on the right, drag on the right
to turn the camera, and pinch to zoom. Tap a tool at the bottom right to select it, then tap
the scene to use it. The Fullscreen menu item toggles the editor panels. Rocket and slingshot
projectile models and scripts are original replacements included with the Android source;
the remaining game content must be supplied separately and is not included in this repository.

The experimental Android port was tested on an Android 16 ARM64 device with a Mali-G615 GPU.
Device checks covered rendering, touch tool selection, rocket and slingshot launch, rocket
impact explosions, projectile cleanup, importing places through Android's file picker, switching
maps, and starting solo play with the Play button. Other devices may need additional compatibility work.

The vendored bzip2 1.0.8 library sources (`3rdparty/bzip2`) retain their BSD-style license.
The vendored gl4es source retains its MIT license. It includes a local fix that synchronizes
fixed-function texture generation state when restoring attributes. The Android port and the
local gl4es fix were developed with AI assistance from Claude and Codex.

## Contributing

If you would like to contribute, please see the [contributing guide](CONTRIBUTING.md).

If you want to report bugs or request new features, see [issues](https://github.com/OpenRBX-org/OpenRBX/issues).

## License

The OpenRBX source code is licensed under the [GNU General Public License v3.0](LICENSE).

This project includes third-party components that are separately licensed.
Those components are not covered by the license above and remain subject to their original licenses.
