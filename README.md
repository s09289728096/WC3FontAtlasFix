# WC3FontAtlasFix

**Warning: 100% vibe code, 0% code review**  
**"It works on my machine"**

A small runtime font-atlas fix for classic **Windows x86 Warcraft III**.
The output is a PE DLL named `War3FontAtlasFix.mix`, loaded by the game's existing MIX loader. It does not modify Game.dll on disk.

## Build on Linux

Requirements: Linux x86_64 (Ubuntu 22.04 or newer), Python 3.9+, curl, tar, xz and sha256sum. No Windows SDK or proprietary game files are needed to build the bundled profile.

```sh
sh tools/setup-toolchain.sh
python3 tools/build.py --toolchain .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64
```

Output: `build/release/War3FontAtlasFix.mix`.
The toolchain download is pinned and SHA256-verified. See [build details](docs/build.md).

To validate and select a profile using your own DLL:

```sh
python3 tools/analyze.py /path/to/Game.dll
python3 tools/build.py --game /path/to/Game.dll --toolchain .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64
```

You may place private inputs in the ignored `local/` directory. Analysis runs locally, without executing or uploading the DLL. Unknown builds produce an analysis report and stop; changing the SHA256 alone does **not** establish compatibility.

## Install

1. Exit the game.
2. Place the built MIX beside Game.dll. Keep only one copy of this font fix in that directory; store backups elsewhere.
3. Start the game normally.

Remove the MIX and restart to uninstall. Release creates no logs, snapshots or hotkey polling loop. It refuses to patch a DLL whose on-disk hash or loaded hook instructions do not match. Use Debug when installation status is needed. The LLVM-MinGW UCRT build requires the Universal CRT (included in Windows 10 and newer).

## Debug mode

```sh
python3 tools/build.py --debug --toolchain .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64
```

Output: `build/debug/War3FontAtlasFix.mix`. Debug adds a log, lifecycle events, CPU pixel verification and **Ctrl+Shift+F8** snapshots. These features and their extra hooks are excluded from Release, independently of compiler optimization level. See [Debug usage](docs/debug.md).

## Compatibility and contributions

Initial profile: Game.dll **1.28.5.7680, x86**. Runtime authorization uses the full SHA256, not the displayed version string. No x64 or Reforged support.

- [Root cause and repair](docs/root-cause.md)
- [Adding a reviewed version profile](docs/profiles.md)
- [Contributing and tests](CONTRIBUTING.md)

Original project code is MIT licensed. Vendored MinHook/HDE retain their licenses. No game executable, game data, captured text, screenshots or font assets are included.
