# Debug mode

Install the Debug MIX instead of Release, never alongside another copy of the same fix. After startup, `War3FontAtlasFix.log` reports installation or the rejection reason. A successful installation requires `MODE reallocation_dirty_fix=1` and `INSTALLED`.

While the game is in the foreground, hold Ctrl+Shift+F8 for about half a second. Keep the relevant text visible for roughly 15 seconds; wait for `CAPTURE n COMPLETE io_errors=0`. A process supports four captures. Output is placed beside the MIX in `font-diagnostic-*`, with a `capture-n` directory for each archive.

```sh
python3 tools/decode.py /path/to/font-diagnostic-session --png
```

Snapshots include CPU atlas pixels, glyph/slot structures and source alpha for up to eight fonts per capture. Logs can contain game text and process-local pointers; review them before sharing. No automatic upload is performed.

`PROOF` counters distinguish clean UV assignments, rearmed glyphs, skipped-but-mismatched regions and verified redraws. A zero mismatch count is meaningful only when relevant paths were exercised. Outline modes and invalid comparison bounds increment `verify_unavailable`, not a success count. `critical_dropped` and `telemetry_dropped` report independent queue losses. Rolling history is finite; captures may overlap and must not be added together as independent totals.

Debug reserves about 30 MiB of static diagnostic memory. Four full captures plus rolling raw event files can use approximately 262 MiB on disk, excluding decoded JSON/PNG. Release does not link these buffers, counters, observers, file writers or hotkey loop.

For developer-controlled A/B comparisons only, an empty `War3FontAtlasFix.observe-only` file beside the Debug MIX disables the new dirty rearm while retaining rectangle clearing and diagnostics. Remove it and restart to restore the fix. Release ignores this marker and always enables both repairs.

UV events occur before all slot fields are finalized: use `assigned_glyph_code`, not the provisional slot key; slot page/row can also be provisional. Snapshot timing may precede a pending update, and snapshots are not globally atomic across threads. They are not GPU readbacks.
