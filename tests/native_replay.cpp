// Optional native replay for locally supplied, trusted Game.dll files.
// No DLL initialization, game assets, or captured private data are used.
#include "../src/runtime.cpp"
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
extern "C" void Stub2();
extern void* trampoline2;
static void *targetSlot,*uvBody;
extern "C" __declspec(naked) void __stdcall RunFrame(DWORD row,DWORD height) {
 __asm {
  push ebp
  mov ebp,esp
  sub esp,60h
  push ebx
  push esi
  push edi
  mov edi,targetSlot
  mov esi,1
  jmp dword ptr [uvBody]
 }
}
extern "C" __declspec(naked) void __stdcall RunStack(DWORD row,DWORD height) {
 __asm {
  sub esp,68h
  push ebx
  push esi
  mov ebx,targetSlot
  jmp dword ptr [uvBody]
 }
}
static DWORD& U(BYTE* p,unsigned o){return *(DWORD*)(p+o);}
int main(int argc,char** argv) {
 setvbuf(stdout,0,_IONBF,0);CHECK(argc==2);
 HMODULE game=LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);CHECK(game);
 CHECK(HashMatches(game) && MemoryMatches(game));
 printf("LOCATED ABI=%u uv=%08lX clear=%08lX body=%08lX atlas=%08lX\n",g_sites.abi,(DWORD)(g_sites.uv-(BYTE*)game),(DWORD)(g_sites.clear-(BYTE*)game),(DWORD)(g_sites.uvBody-(BYTE*)game),(DWORD)(g_sites.atlasEntry-(BYTE*)game));
 static BYTE font[0x2c8]={},slot[64]={},glyph[92]={},source[600]={};static DWORD pixels[256*256],before[256*256],rows[4][4]={};
 for(unsigned i=0;i<600;++i)source[i]=(i%5)?(BYTE)(i%200+1):0;
 for(unsigned i=0;i<256*256;++i)pixels[i]=0x90abcdef;
 U(font,0x1b0)=26;BYTE* page=font+0x1c8;
 U(page,4)=(DWORD)pixels;U(page,12)=(DWORD)font;U(page,0x18)=4;U(page,0x1c)=(DWORD)rows;rows[3][3]=(DWORD)slot;
 U(slot,0x30)=3;U(slot,0x34)=26;U(slot,0x38)=49;U(slot,0x3c)=(DWORD)glyph;
 U(glyph,0x1c)=(DWORD)source;U(glyph,0x20)=600;U(glyph,0x28)=24;U(glyph,0x2c)=25;U(glyph,0x30)=24;U(glyph,0x3c)=24;
 targetSlot=slot;uvBody=g_sites.uvBody;
 auto uv=[&](){if(g_sites.abi)RunStack(3,26);else RunFrame(3,26);};
 typedef void(__thiscall* Update)(void*,DWORD,DWORD,DWORD,DWORD,DWORD*,void**);
 Update update=(Update)g_sites.atlasEntry;DWORD pitch=0;void* output=0;
 auto draw=[&](){U(page,0)=1;CHECK(Guard([&](){update(page,0,256,256,0,&pitch,&output);}));CHECK(pitch==1024 && output==pixels && U(page,0)==0 && U(glyph,0x24)==0);};
 auto differences=[&](){unsigned bad=0;for(unsigned y=0;y<26;++y)for(unsigned x=0;x<24;++x){BYTE expected=y<25?source[y*24+x]:0;bad+=((pixels[(78+y)*256+U(slot,0x34)+x]>>24)!=expected);}return bad;};
 CHECK(MH_Initialize()==MH_OK);CHECK(MH_CreateHook(g_sites.clear,g_sites.abi?(void*)LegacyClearStub:(void*)HookStub,&g_trampoline)==MH_OK);CHECK(MH_CreateHook(g_sites.uv,(void*)Stub2,&trampoline2)==MH_OK);
 CHECK(MH_EnableHook(g_sites.clear)==MH_OK);CHECK(Guard(uv));CHECK(U(glyph,0x24)==0 && U(glyph,0x58)==1);draw();unsigned bad=differences();CHECK(bad>0);printf("BASELINE clean bitmap remains wrong: %u/624 pixels\n",bad);
 CHECK(MH_EnableHook(g_sites.uv)==MH_OK);
 for(unsigned pass=0;pass<2;++pass) {
  if(pass)U(slot,0x34)=60,U(slot,0x38)=83;
  memcpy(before,pixels,sizeof(pixels));CHECK(Guard(uv));CHECK(U(glyph,0x24)==1);draw();CHECK(differences()==0);
  for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x)if(!(y>=78 && y<104 && x>=U(slot,0x34) && x<=U(slot,0x38)))CHECK(before[y*256+x]==pixels[y*256+x]);
  printf("FIXED pass=%u full slot correct; neighboring pixels unchanged\n",pass);
 }
 CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);CHECK(MH_Uninitialize()==MH_OK);
 puts("PASS native UV + atlas execution, no game initialization or rendering imports.");return 0;
}
