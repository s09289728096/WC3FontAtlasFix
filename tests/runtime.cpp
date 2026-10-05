#include "core.h"
#include "guard.h"
#include "diag.h"
#include "MinHook.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
extern "C" void Stub2();
extern void* trampoline2;
extern "C" void __stdcall ClearSlot(void*,void*,void*);
static void* fixture;
static DWORD actual[6],lastError;
__declspec(align(16)) static BYTE fxBefore[512],fxAfter[512];
static BYTE slot[64],glyph[92];
static DWORD frame[32];
static void __declspec(naked) Invoke() {
 __asm {
  push ebp
  push ebx
  push esi
  push edi
  lea ebp,[frame+32]
  lea edi,[slot]
  mov eax,12345678h
  mov ebx,23456789h
  mov esi,3456789ah
  mov edx,456789abh
  mov ecx,56789abch
  mov dword ptr fs:[34h],123abcdeh
  fld1
  fxsave [fxBefore]
  call dword ptr [fixture]
  mov dword ptr [actual],eax
  mov dword ptr [actual+4],ebx
  mov dword ptr [actual+8],esi
  mov dword ptr [actual+12],edx
  mov dword ptr [actual+16],ecx
  pushfd
  pop dword ptr [actual+20]
  mov eax,dword ptr fs:[34h]
  mov lastError,eax
  fxsave [fxAfter]
  fstp st(0)
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret
 }
}
void TestLocator();
int main() {
 TestLocator();
 void* inaccessible=VirtualAlloc(0,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);CHECK(inaccessible);
 CHECK(!Guard([&](){ *(volatile DWORD*)inaccessible=1; }));
 bool completed=false;CHECK(Guard([&](){completed=true;}));CHECK(completed);
 CHECK(Guard([&](){CHECK(!Guard([&](){*(volatile DWORD*)inaccessible=2;}));}));
 VirtualFree(inaccessible,0,MEM_RELEASE);

 static uint32_t pixels[256*256];BYTE page[32]={};
 for(unsigned i=0;i<256*256;++i)pixels[i]=0x90ffffff;
 *(DWORD*)(page+4)=(DWORD)pixels;
 *(DWORD*)(slot+0x34)=24;*(DWORD*)(slot+0x38)=47;*(DWORD*)(slot+0x3c)=(DWORD)glyph;
 BYTE* f=(BYTE*)&frame[8];*(DWORD*)(f-8)=3*26*256;*(DWORD*)(f-4)=(DWORD)page;
 *(DWORD*)(f+0x10)=26;*(DWORD*)(f+0x1c)=3*26;
 ClearSlot(page,slot,f);
 for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x)
  CHECK(pixels[y*256+x]==((x>=24 && x<=47 && y>=78 && y<104)?0:0x90ffffff));
 CHECK(!ClearRectangle(pixels,0,256,0,1));CHECK(!ClearRectangle(pixels,0,2,255,2));
 fixture=VirtualAlloc(0,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(fixture);
 // Set ZF before the hook; the observer must preserve it as well as all register state.
 BYTE code[]={0x31,0xc0,0x90,0x90,0x90,0x90,0x90,0x90,0xc3};
 memcpy(fixture,code,sizeof(code));FlushInstructionCache(GetCurrentProcess(),fixture,sizeof(code));
 CHECK(MH_Initialize()==MH_OK);
 CHECK(MH_CreateHook((BYTE*)fixture+2,(void*)Stub2,&trampoline2)==MH_OK);CHECK(MH_EnableHook((BYTE*)fixture+2)==MH_OK);
 Invoke();
 CHECK(*(DWORD*)(glyph+0x24)==1);
 CHECK(actual[0]==0 && actual[1]==0x23456789 && actual[2]==0x3456789a && actual[3]==0x456789ab && actual[4]==0x56789abc);
 CHECK(actual[5]&0x40);CHECK(lastError==0x123abcde);CHECK(!memcmp(fxBefore,fxAfter,416));
 CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);CHECK(MH_Uninitialize()==MH_OK);VirtualFree(fixture,0,MEM_RELEASE);
#ifdef WC3_DEBUG
 CHECK(diagFaults==0 && rearmedGlyphs==1 && g_cleared==1);
 static BYTE font[0x2c8]={},text[0x90]={},source[600]={};static DWORD row[4]={};
 *(DWORD*)(font+0x1b0)=26;BYTE* atlas=font+0x1c8;
 *(DWORD*)(atlas+4)=(DWORD)pixels;*(DWORD*)(atlas+12)=(DWORD)font;
 *(DWORD*)(atlas+0x18)=1;*(DWORD*)(atlas+0x1c)=(DWORD)row;row[3]=(DWORD)slot;
 *(DWORD*)(glyph+0x1c)=(DWORD)source;*(DWORD*)(glyph+0x20)=600;
 *(DWORD*)(glyph+0x28)=24;*(DWORD*)(glyph+0x2c)=25;*(DWORD*)(glyph+0x3c)=24;
 *(DWORD*)(text+0x44)=(DWORD)font;*(DWORD*)(text+0x5c)=(DWORD)"synthetic";
 wchar_t directory[MAX_PATH];swprintf_s(directory,L"build/debug/test-capture-%lu",GetCurrentProcessId());
 CHECK(DiagInit(directory));DiagTrigger();Registers regs={};regs.ecx=(DWORD)text;Observe(9,&regs);
 CHECK(snapshots[0].state==2 && snapshots[0].value.status==0);
 CHECK(snapshots[0].value.glyphCount==1 && snapshots[0].value.sourceUsed==600);
 CHECK(!memcmp(snapshots[0].value.atlas[0],pixels,sizeof(pixels)));
 DiagPump();Sleep(10100);DiagPump();DiagPump();
 CHECK(captureCompleted==1 && diagIoErrors==0 && diagFaults==0);
 wchar_t archived[MAX_PATH];swprintf_s(archived,L"%s/capture-1/critical-0.bin",directory);
 CHECK(GetFileAttributesW(archived)!=INVALID_FILE_ATTRIBUTES);
 puts("PASS Debug snapshot and archive.");
#endif
 puts("PASS native SEH fault/recovery/nesting, slot isolation, invalid bounds, dirty repair, x86 registers/flags/LastError/x87/XMM.");
 return 0;
}

