# Version profiles

Profiles contain an exact Game.dll SHA256, PE metadata, hook RVAs and expected instruction bytes. `classic-font-v1` selects the structure offsets and register/frame conventions implemented by the current sources. It is not an assertion that every classic release uses that layout.

`tools/analyze.py` accepts a DLL path, reads executable sections and checks all profile signatures. Known matching files can select the generated build configuration. Unknown files return a nonzero status and a report with candidate RVAs from exact pattern searches. Candidates are limited review hints; no new profile or MIX is approved automatically. Analysis never loads the DLL.

To add support:

1. Analyze a locally supplied DLL. Keep it outside Git or under ignored `local/`.
2. Review the bitmap dirty gate, UV lifecycle, atlas dimensions, rectangle ownership, structure offsets, frame/register conventions and overwritten instruction boundaries. Check Debug observation sites as well.
3. If the same ABI is established, add a profile with exact metadata and signatures. Otherwise implement an explicit new ABI adapter before admitting the version. Do not simply copy a profile and replace its hash.
4. Extend synthetic tests, compile both modes, and validate the new target in the game. A structurally similar pattern is not a substitute for this validation.

The current build driver defaults to the bundled profile. Passing `--game` selects any reviewed profile matching the supplied file. The compiled MIX authorizes that exact hash at runtime; this initial project does not make one binary dynamically support every version.

Contributions must not include Game.dll, disassemblies of entire proprietary binaries, game assets or private diagnostic captures. Small hook signatures and ABI descriptions belong in the profile and source.
