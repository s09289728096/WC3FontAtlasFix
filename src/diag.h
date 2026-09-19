#pragma once
#include <windows.h>
#include <stdint.h>
// All binary structures use fixed-width DWORDs and are decoded by decode.py.
struct Registers { DWORD edi,esi,ebp,savedEsp,ebx,edx,ecx,eax,flags; };
struct Event { DWORD kind,tick,thread,data[24]; char text[768]; };
struct GlyphCopy { DWORD slot,page,row,sourceOffset,sourceSize; BYTE slotBytes[64],glyphBytes[92]; };
struct Snapshot {
 DWORD magic,version,capture,font,tick,status,pageMask,glyphCount,sourceUsed;
 BYTE fontBytes[0x2c8];
 BYTE atlas[8][0x40000];
 GlyphCopy glyphs[1024];
 BYTE source[0x100000];
};
struct SnapshotSlot { volatile LONG state; Snapshot value; };
bool DiagInit(const wchar_t* directory);
void DiagPump();
void DiagTrigger();
void DiagClear(void* page,void* slot,void* frame);
extern "C" void __stdcall Observe(DWORD kind,Registers* regs);
bool DiagPrepare(BYTE* game);
extern volatile LONG diagDropped,diagFaults,diagIoErrors;
extern SnapshotSlot snapshots[8];
extern DWORD captureSerial,captureCompleted;

extern volatile LONG repairEnabled,reusedClean,rearmedGlyphs,skippedMismatch,verifiedGlyphs,verifyMismatch,verifyUnavailable;
extern volatile LONG criticalDropped,telemetryDropped;
extern "C" DWORD CompareGlyph(DWORD page,DWORD slot,DWORD y,DWORD rowHeight,DWORD* compared);
