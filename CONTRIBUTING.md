# Contributing

Guarded callbacks must not own C++ objects requiring destructor-based unwinding: hardware faults leave through the C SEH boundary. The runtime tests deliberately fault on PAGE_NOACCESS and verify recovery and nesting.

Keep changes small. Explain the defect or behavior, include relevant tests, and distinguish synthetic/build checks from in-game validation. Preserve the two minimal Release hooks and keep diagnostics behind `WC3_DEBUG`.

Run `python3 -m unittest discover -s tests -v`, build both modes with `--tests`, and run both Windows test executables. The mandatory tests use synthetic buffers and PE fixtures; they do not require a game installation. Optional native replay with a privately supplied, trusted DLL exercises the production repair sources against original UV/atlas instructions. Keep such inputs out of Git. Refer to docs/profiles.md for target-specific work.

Do not commit private DLLs, diagnostic captures, screenshots, local paths or toolchain downloads. Do not weaken the fail-closed version checks to get an unknown binary to load. Preserve third-party copyright and license notices.

Project layout:

- `src/`: repair, runtime installation and optional diagnostics.
- `profiles/`: reviewed target metadata and instruction signatures.
- `tools/`: Linux build, local analysis and Debug decoder.
- `tests/`: synthetic analyzer and x86 hook tests.
- `vendor/minhook/`: pinned MinHook source and license.
