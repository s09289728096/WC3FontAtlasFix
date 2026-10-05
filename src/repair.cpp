#include "locator.h"
#include "guard.h"
#include "core.h"
#ifdef WC3_DEBUG
#include "diag.h"
#endif
void* g_trampoline = 0;
#ifdef WC3_DEBUG
volatile LONG g_cleared=0, g_skipped=0;
#endif
extern "C" void __stdcall ClearSlot(void* page, void* slot, void* frame) {
#ifdef WC3_DEBUG
    DiagClear(page,slot,frame);
#endif
    bool ok=false;
    if(!Guard([&]() {
        BYTE* p=(BYTE*)page; BYTE* s=(BYTE*)slot; BYTE* f=(BYTE*)frame;
        unsigned offset=*(unsigned*)(f-8), height=*(unsigned*)(f+0x10);
        unsigned y=offset/256, x=*(unsigned*)(s+0x34), end=*(unsigned*)(s+0x38);
        if (page==*(void**)(f-4) && offset%256==0 && y==*(unsigned*)(f+0x1c))
            ok=ClearRectangle(*(uint32_t**)(p+4),x,end,y,height);
    })) { ok=false; }
#ifdef WC3_DEBUG
    InterlockedIncrement(ok ? &g_cleared : &g_skipped);
#else
    (void)ok;
#endif
}
extern "C" __declspec(naked) void HookStub() {
    __asm {
        pushfd
        pushad
        cld
        mov ebx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        mov esi, esp
        push dword ptr fs:[34h]
        push dword ptr [ebx+8]
        push dword ptr [ebx+20]
        push dword ptr [ebx+4]
        call ClearSlot
        pop eax
        mov dword ptr fs:[34h],eax
        fxrstor [esi]
        mov esp, ebx
        popad
        popfd
        jmp dword ptr [g_trampoline]
    }
}

#ifndef WC3_DEBUG
// Same dirty repair as Debug, without telemetry or pixel verification.
struct Registers { DWORD edi,esi,ebp,savedEsp,ebx,edx,ecx,eax,flags; };
extern "C" void __stdcall Observe(DWORD, Registers* r) {
    if(!Guard([&]() {
        DWORD slot=g_sites.abi?r->ebx:r->edi;
        BYTE* glyph=*(BYTE**)(slot+0x3c);
        if(glyph) InterlockedExchange((volatile LONG*)(glyph+0x24),1);
    })) {}
}
#endif

// 1.26 omits the EBP frame; recover arguments from the original ESP.
struct LegacyRegisters { DWORD edi,esi,ebp,savedEsp,ebx,edx,ecx,eax,flags; };
extern "C" void __stdcall ClearLegacy(LegacyRegisters* r) {
    if(!Guard([&]() {
        DWORD sp=r->savedEsp+4;
        if(*(DWORD*)(sp+0x10)!=r->ebx)return;
        DWORD frame[16]={};BYTE* f=(BYTE*)&frame[4];
        *(DWORD*)(f-4)=r->ebx;
        *(DWORD*)(f-8)=*(DWORD*)(sp+0x44);
        *(DWORD*)(f+0x10)=*(DWORD*)(sp+0x38);
        *(DWORD*)(f+0x1c)=*(DWORD*)(sp+0x3c);
        ClearSlot((void*)r->ebx,(void*)r->edx,f);
    })) {}
}
extern "C" __declspec(naked) void LegacyClearStub() {
 __asm {
  pushfd
  pushad
  cld
  mov ebx,esp
  sub esp,528
  and esp,-16
  fxsave [esp]
  mov esi,esp
  push dword ptr fs:[34h]
  push ebx
  call ClearLegacy
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [g_trampoline]
 }
}
