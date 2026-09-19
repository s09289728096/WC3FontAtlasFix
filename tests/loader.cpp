#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "profile.h"
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 bool expected=!strcmp(argv[3],"supported");
 HMODULE game=LoadLibraryExA(argv[1],0,DONT_RESOLVE_DLL_REFERENCES);
 if(!game){printf("Game mapping failed: %lu\n",GetLastError());return 3;}
 HMODULE mix=LoadLibraryA(argv[2]);
 if(!mix){printf("MIX load failed: %lu\n",GetLastError());return 4;}
 if(expected) {
  for(unsigned i=0;i<100;++i) {
   Sleep(50);
   if(*((BYTE*)game+kClearRva)==0xe9 && *((BYTE*)game+kRva2)==0xe9) {
    puts("PASS actual MIX worker installed both repair hooks in an uninitialized game mapping.");return 0;
   }
  }
  puts("FAIL hooks not installed");return 5;
 }
 Sleep(1500);
 puts("PASS MIX loader with unsupported mock; Debug log must report rejection.");
 return 0;
}
