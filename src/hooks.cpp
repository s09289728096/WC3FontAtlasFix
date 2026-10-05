#include "diag.h"
#include "profile.h"
#include "locator.h"
#include "MinHook.h"
#include <string.h>
void* trampoline2=0;
extern "C" __declspec(naked) void Stub2() {
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
  push 2
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline2]
 }
}

#ifdef WC3_DEBUG
void* trampoline3=0;
extern "C" __declspec(naked) void Stub3() {
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
  push 3
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline3]
 }
}

void* trampoline4=0;
extern "C" __declspec(naked) void Stub4() {
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
  push 4
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline4]
 }
}

void* trampoline5=0;
extern "C" __declspec(naked) void Stub5() {
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
  push 5
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline5]
 }
}

void* trampoline6=0;
extern "C" __declspec(naked) void Stub6() {
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
  push 6
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline6]
 }
}

void* trampoline7=0;
extern "C" __declspec(naked) void Stub7() {
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
  push 7
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline7]
 }
}

void* trampoline8=0;
extern "C" __declspec(naked) void Stub8() {
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
  push 8
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline8]
 }
}

void* trampoline9=0;
extern "C" __declspec(naked) void Stub9() {
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
  push 9
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline9]
 }
}

void* trampoline10=0;
extern "C" __declspec(naked) void Stub10() {
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
  push 10
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline10]
 }
}

void* trampoline11=0;
extern "C" __declspec(naked) void Stub11() {
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
  push 11
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline11]
 }
}

void* trampoline16=0;
extern "C" __declspec(naked) void Stub16() {
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
  push 16
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline16]
 }
}

void* trampoline17=0;
extern "C" __declspec(naked) void Stub17() {
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
  push 17
  call Observe
  pop eax
  mov dword ptr fs:[34h],eax
  fxrstor [esi]
  mov esp,ebx
  popad
  popfd
  jmp dword ptr [trampoline17]
 }
}

#endif
bool DiagPrepare(BYTE* game) {
 if(!g_sites.uv) return false;
#ifdef WC3_DEBUG
 if(memcmp(game+kRva3,kSig3,sizeof(kSig3))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva4,kSig4,sizeof(kSig4))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva5,kSig5,sizeof(kSig5))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva6,kSig6,sizeof(kSig6))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva7,kSig7,sizeof(kSig7))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva8,kSig8,sizeof(kSig8))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva9,kSig9,sizeof(kSig9))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva10,kSig10,sizeof(kSig10))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva11,kSig11,sizeof(kSig11))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva16,kSig16,sizeof(kSig16))) return false;
#endif
#ifdef WC3_DEBUG
 if(memcmp(game+kRva17,kSig17,sizeof(kSig17))) return false;
#endif
 if(MH_CreateHook(g_sites.uv,(void*)Stub2,&trampoline2)!=MH_OK) return false;
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva3,(void*)Stub3,&trampoline3)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva4,(void*)Stub4,&trampoline4)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva5,(void*)Stub5,&trampoline5)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva6,(void*)Stub6,&trampoline6)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva7,(void*)Stub7,&trampoline7)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva8,(void*)Stub8,&trampoline8)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva9,(void*)Stub9,&trampoline9)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva10,(void*)Stub10,&trampoline10)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva11,(void*)Stub11,&trampoline11)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva16,(void*)Stub16,&trampoline16)!=MH_OK) return false;
#endif
#ifdef WC3_DEBUG
 if(MH_CreateHook(game+kRva17,(void*)Stub17,&trampoline17)!=MH_OK) return false;
#endif
 return true;
}
