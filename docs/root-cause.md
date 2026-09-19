# Root cause

The font renderer keeps source glyph bitmaps separately from their placement in a CPU texture atlas. A cached bitmap can outlive the atlas slot that once contained it. Two independent conditions must be handled when slots are reused:

1. **Transparent source pixels skip writes.** Drawing onto a reused rectangle can leave old nonzero alpha beneath transparent portions of the new glyph. Clear the rectangle owned by the incoming glyph, including padding, immediately before drawing it. Do not clear neighboring glyphs or the entire page.
2. **Changing placement does not rearm bitmap dirty.** The UV update sets a separate flag but does not mark the cached bitmap for drawing. A dirty atlas page still skips individual glyphs whose bitmap dirty flag is zero. The new UV can therefore reference pixels belonging to an older occupant. Mark the bitmap dirty whenever its placement/UV is assigned.

For the bundled x86 ABI, slot+0x3c points to the glyph, glyph+0x24 controls bitmap redraw, and glyph+0x58 is a different flag. The UV completion hook sets glyph+0x24 to 1. The clear hook executes after the dirty gate and clears the inclusive x range stored in slot+0x34/+0x38, using the renderer's current row position and height. The original renderer and page-dirty scheduling then complete the update.

These are CPU atlas lifecycle defects, so changing the graphics translation layer does not address these code paths. The patch preserves the original source text, glyph bitmap, UV coordinates and allocation algorithm.

Hooks preserve x86 general registers, flags, LastError, x87 and XMM state. They are installed outside DllMain's loader lock, using MinHook trampolines. The module remains loaded for the process lifetime; updates require a restart. The file hash, image metadata and loaded instruction bytes must match the selected profile before installation.

The structure offsets and calling conventions are specific to the reviewed ABI. Similar instructions in another DLL are not sufficient proof of compatibility. Debug comparisons inspect CPU alpha, not final GPU presentation, and exclude unsupported outline layouts.
