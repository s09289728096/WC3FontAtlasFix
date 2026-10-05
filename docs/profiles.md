# Version profiles and instruction locators

Two independent checks govern Release:

1. `classic-*.json`: exact SHA256, PE metadata and the reviewed ABI family. These authorize a build, not its hook addresses.
2. `locators.json`: masked instruction anchors and bounded ABI/control-flow witnesses. These discover addresses from executable sections. Each anchor must be unique, with all witnesses present and branch targets consistent.

The current `frame` family supports the reviewed 1.27/1.28 code layout; `stack` supports the reviewed 1.26 layout. One Release MIX contains both adapters and all three authorized hashes. It never selects addresses using a version-specific repair RVA. Debug alone retains extra fixed observation sites for 1.28.5.7680, and refuses other targets.

`tools/analyze.py /path/to/Game.dll` is local and does not execute the input. Its `candidate_sites` may identify a known ABI in an unknown binary, but `supported` remains false unless its hash/metadata are reviewed. `tools/build.py --game ...` validates a supplied input before compiling the multi-version Release.

To add support:

1. Supply a local DLL, outside Git or under ignored `local/`, and inspect the analyzer output.
2. Review UV assignment, bitmap dirty lifetime, atlas dimensions, rectangle ownership, register/frame conventions and overwritten instruction boundaries. A unique pattern alone is not enough.
3. Reuse a locator only if its full ABI/control-flow contract holds. Otherwise add an explicit adapter and corresponding witnesses to `locators.json`; do not wildcard structure offsets or registers to force a match.
4. Add exact hash/PE metadata to a reviewed target profile. Only enable Debug support after separately reviewing its observation hooks.
5. Run synthetic moved-section, ambiguity, branch-target and layout rejection tests in both Python and the actual C++ locator. Run the optional native replay and isolated MIX loader against the supplied DLL, then validate in the full game.

Do not commit proprietary DLLs, game assets, private captures or entire disassemblies. Keep only the small locator signatures and the ABI descriptions needed for the patch. A synthetic moved-section test demonstrates position independence; it is not a replacement for real-version testing.
