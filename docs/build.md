# Linux build

The build uses [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) rather than GCC: the hooks use Microsoft-style x86 assembly. Native structured exception handling is isolated in a tiny C ABI boundary (`src/guard.c`), compiled with Clang's MSVC target; all other sources use the MinGW target. No Windows SDK or proprietary library is needed. Linux is the **build host**, not a supported game runtime.

`tools/setup-toolchain.sh` installs the pinned 20250613 UCRT Linux x86_64 toolchain under ignored `.tools/`, after verifying its archive SHA256. Downloads and generated artifacts are never committed. To use an existing LLVM-MinGW installation, pass `--toolchain /path/to/llvm-mingw`, or put its `bin/` on PATH and omit the argument.

```sh
python3 -m unittest discover -s tests -v
python3 tools/build.py --tests --toolchain .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64
python3 tools/build.py --debug --tests --toolchain .tools/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64
```

Run each generated `runtime-test.exe` on Windows x86/x64. It tests a synthetic hook and atlas; no game is needed. Linux compilation alone cannot establish in-game behavior.

The project uses Python's standard library as its small build driver. It compiles vendored MinHook and project sources, generates a private `profile.h` from the selected version profile, and links a Windows x86 DLL with ASLR/NX. Link timestamps are disabled and source paths are remapped. Each output directory contains `manifest.json` with the compiler identity, target Game.dll hash and MIX hash. Builds use UCRT and statically link toolchain runtime libraries.

Release is the default. `--debug` is an explicit feature selection, not a request to disable optimization. Only the UV repair and rectangle-clear hooks are installed by Release. Debug additionally links diagnostics and installs observation hooks.

For a supplied `--game`, profile selection and instruction verification happen before compilation. A failure does not overwrite a previously built MIX; do not mistake an older artifact for a successful new build. The generated analysis report is under the selected build directory. Without `--game`, the bundled reviewed profile is used, allowing public CI without proprietary inputs.

Optional Windows installation checks (replace paths as needed):

```powershell
./build/release/loader-test.exe ./build/mock/Game.dll ./build/release/War3FontAtlasFix.mix unsupported
./build/debug/loader-test.exe ./build/mock/Game.dll ./build/debug/War3FontAtlasFix.mix unsupported
./build/release/loader-test.exe C:/private/Game.dll ./build/release/War3FontAtlasFix.mix supported
```

The supported-image check maps Game.dll without initialization and checks installation of both hooks. It does not execute game rendering or replace an in-game compatibility test. Debug's unsupported check should append a SHA256 rejection to its log. Release intentionally remains silent.
