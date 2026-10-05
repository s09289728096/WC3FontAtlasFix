#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>
static bool ClearRectangle(uint32_t* pixels, unsigned x, unsigned end, unsigned y, unsigned height) {
    if (!pixels || x > end || end >= 256 || y >= 256 || !height || height > 256-y) return false;
    for (unsigned row=0; row<height; ++row)
        memset(pixels+(y+row)*256+x, 0, (end-x+1)*4);
    return true;
}
extern void* g_trampoline;
#ifdef WC3_DEBUG
extern volatile LONG g_cleared, g_skipped;
#endif
extern "C" void HookStub();

extern "C" void LegacyClearStub();
