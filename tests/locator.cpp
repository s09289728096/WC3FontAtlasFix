#include "locator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Pattern {const BYTE *bytes,*mask;size_t size;};
struct LocatorSpec {unsigned abi,uvOffset,clearOffset,gateJump,loopJump;Pattern uv,atlas,uvBody,entry,layout,post,next;};
#include "locator_patterns.h"
#define CHECK(x) do{if(!(x)){printf("FAIL locator line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void TestLocator() {
 BYTE* image=(BYTE*)VirtualAlloc(0,0x8000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(image);
 for(const LocatorSpec& s:kLocators)for(unsigned shift=0;shift<=73;shift+=73) {
  memset(image,0,0x8000);IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)image;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
  IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(image+0x80);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
  nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);nt->FileHeader.NumberOfSections=1;nt->OptionalHeader.Magic=0x10b;nt->OptionalHeader.SizeOfImage=0x8000;
  IMAGE_SECTION_HEADER* sec=IMAGE_FIRST_SECTION(nt);sec->Characteristics=IMAGE_SCN_MEM_EXECUTE;sec->VirtualAddress=0x1200;sec->Misc.VirtualSize=0x1400;
  BYTE* base=image+sec->VirtualAddress+shift;
  memcpy(base+0x100,s.uvBody.bytes,s.uvBody.size);memcpy(base+0x250,s.uv.bytes,s.uv.size);
  memcpy(base+0x400,s.entry.bytes,s.entry.size);memcpy(base+0x460,s.layout.bytes,s.layout.size);memcpy(base+0x500,s.atlas.bytes,s.atlas.size);
  memcpy(base+0x600,s.post.bytes,s.post.size);memcpy(base+0x660,s.next.bytes,s.next.size);
  *(int*)(base+0x500+s.gateJump+2)=0x660-(0x500+s.gateJump+6);*(int*)(base+0x660+s.loopJump+2)=0x500-(0x660+s.loopJump+6);
  RepairSites found={};CHECK(FindRepairSites(image,&found));CHECK(found.abi==s.abi && found.uv==base+0x250+s.uvOffset && found.clear==base+0x500+s.clearOffset);
  BYTE* operand=base+0x500+s.gateJump+2;*operand^=1;CHECK(!FindRepairSites(image,&found));*operand^=1;
  memcpy(base+0x900,s.uv.bytes,s.uv.size);CHECK(!FindRepairSites(image,&found));memset(base+0x900,0,s.uv.size);
  base[0x462]^=1;CHECK(!FindRepairSites(image,&found));base[0x462]^=1;
  sec->Characteristics=0;CHECK(!FindRepairSites(image,&found));
 }
 VirtualFree(image,0,MEM_RELEASE);puts("PASS locator: both ABIs, relocated sections, duplicates, branch targets and layouts.");
}
