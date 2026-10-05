#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "locator.h"
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 bool expected=!strcmp(argv[3],"supported");
 HMODULE game=LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);
 if(!game){printf("Game mapping failed: %lu\n",GetLastError());return 3;}
 RepairSites sites={};
 bool located=FindRepairSites((BYTE*)game,&sites);
 if(expected && !located){puts("FAIL cannot locate repair sites");return 6;}
 BYTE beforeUv[6]={},beforeClear[6]={};
 if(located){memcpy(beforeUv,sites.uv,6);memcpy(beforeClear,sites.clear,6);}
 HMODULE mix=LoadLibraryA(argv[2]);
 if(!mix){printf("MIX load failed: %lu\n",GetLastError());return 4;}
 if(expected) {
  for(unsigned i=0;i<100;++i) {
   Sleep(50);
   if(*sites.clear==0xe9 && *sites.uv==0xe9) {
    puts("PASS actual MIX worker installed both repair hooks in an uninitialized game mapping.");return 0;
   }
  }
  puts("FAIL hooks not installed");return 5;
 }
 Sleep(1500);
 if(located && (memcmp(beforeUv,sites.uv,6) || memcmp(beforeClear,sites.clear,6))){puts("FAIL rejected target was modified");return 7;}
 puts("PASS rejected target unchanged; inspect Debug log for rejection reason.");
 return 0;
}
