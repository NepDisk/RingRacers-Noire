# Ring Racers Noire

Noire is a modification of Dr. Robotnik's Ring Racers which adds new features and adds toggles to existing ones!

Some Features include:
- All of HEP2's features
- Rings toggle
- Combo toggle
- Hitlag toggle
- Tumble/Stumble toggles
- Item revert toggles
- Custom itemodds
- Slope Helper toggle
- Local Skins
- Flipcam and old trick input profile options
- New Pogo Spring Terrain type
and many more!

All gameplay tweaks are located in the Noire Gameplay menu inside Options > Gameplay.

These all work in GrandPrix as well but have to be activated before hand.

Documentation for new stuff added is located within the [documentation](https://github.com/NepDisk/RingRacers-Noire/tree/Noire/documentation) folder.

This contain information about all the new thing added by Noire.

# Credits

Thanks to Alug, Haya, LuigiBudd, Freaky Mutant Man, WumboSpasm, mncho and Anreol for their forks and Pull Requests!

Also thanks to Indev for creating the original Slope Helper script!

## Contributing

Have a feature or toggle idea? Want a new gameplay style preset added to the menu? Feel free to make PRs to this branch!

## Note

This is not net compatible with vanilla Ring Racers.

# Development

## Building from Source

Ring Racers is built using a compatible C++ toolchain (GCC, MinGW, Clang and Apple Clang as of this writing), CMake, and Microsoft vcpkg. The compiler and runtime libraries must support the ISO C++17 standard and ISO C11 standard.

On Linux platforms, you will need the following libraries available on the system.

- libcurl
- zlib
- libpng
- libogg
- libvorbis
- libvpx
- libyuv
- SDL2

On Windows and macOS, you will need to install [vcpkg] instead to build these dependencies alongside the game.

[vcpkg]: https://vcpkg.io/en/

To configure and build the game, there are [CMake presets] (declared in `CMakePresets.json`). These presets require the ninja build script tool in addition to cmake and your C++ toolchain. Here is a non-exhaustive list of them:

- ninja-debug: non-optimized, assertions enabled
- ninja-develop: optimized, assertions enabled
- ninja-release: optimized
- ninja-x86_mingw_static_vcpkg-debug
- ninja-x86_mingw_static_vcpkg-develop
- ninja-x86_mingw_static_vcpkg-release
- ninja-x64_osx_vcpkg-debug
- ninja-x64_osx_vcpkg-develop
- ninja-x64_osx_vcpkg-release
- ninja-arm64_osx_vcpkg-debug
- ninja-arm64_osx_vcpkg-develop
- ninja-arm64_osx_vcpkg-release

These were added and not originally included. Presets by Alug
- ninja-x86_64_mingw_static_vcpkg-debug
- ninja-x86_64_mingw_static_vcpkg-develop
- ninja-x86_64_mingw_static_vcpkg-release

[CMake presets]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

These presets depend on the `VCPKG_ROOT` environment variable being specified before the first run of the `cmake` command. Their build directories are pre-configured as subdirectories of `build/`.

After all prerequisites are set-up, configure and build using the following commands, adjusting according to your target system:

    cmake --preset ninja-x86_mingw_static_vcpkg-develop
    cmake --build --preset ninja-x86_mingw_static_vcpkg-develop

## Disclaimer

Dr. Robotnik's Ring Racers is a work of fan art made available for free without intent to profit or harm the intellectual property rights of the original works it is based on. Kart Krew Dev is in no way affiliated with SEGA Corporation. We do not claim ownership of any of SEGA's intellectual property used in Dr. Robotnik's Ring Racers.
