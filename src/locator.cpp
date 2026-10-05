#include "locator.h"
#include "guard.h"
#include <string.h>
struct Pattern {const BYTE *bytes,*mask;size_t size;};
struct LocatorSpec {unsigned abi,uvOffset,clearOffset,gateJump,loopJump;Pattern uv,atlas,uvBody,entry,layout,post,next;};
#include "locator_patterns.h"
RepairSites g_sites={};
static bool Match(const BYTE* p,const Pattern& pattern) {
 if(p[0]!=pattern.bytes[0])return false;
 for(size_t i=0;i<pattern.size;++i)if((p[i]&pattern.mask[i])!=(pattern.bytes[i]&pattern.mask[i]))return false;
 return true;
}
static BYTE* Unique(BYTE* begin,BYTE* end,const Pattern& p) {
 BYTE* found=0;
 if(end<begin || (size_t)(end-begin)<p.size)return 0;
 for(BYTE* at=begin;at<=end-p.size;++at)if(Match(at,p)){if(found)return 0;found=at;}
 return found;
}
static BYTE* Before(BYTE* section,BYTE* at,const Pattern& p) {
 return Unique(at-section>768?at-768:section,at,p);
}
static BYTE* After(BYTE* at,BYTE* sectionEnd,const Pattern& p) {
 return Unique(at,sectionEnd-at>768?at+768:sectionEnd,p);
}
static bool Resolve(BYTE* image,RepairSites* output) {
 IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)image;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<64 || dos->e_lfanew>4096)return false;
 IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(image+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.Magic!=0x10b || nt->FileHeader.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER32))return false;
 unsigned size=nt->OptionalHeader.SizeOfImage,count=nt->FileHeader.NumberOfSections;
 if(size<4096 || size>0x40000000 || !count || count>96)return false;
 IMAGE_SECTION_HEADER* sections=IMAGE_FIRST_SECTION(nt);
 if((BYTE*)(sections+count)>image+size)return false;
 unsigned families=0;RepairSites selected={};
 for(const LocatorSpec& spec:kLocators) {
  BYTE *uv=0,*atlas=0,*uvBegin=0,*atlasBegin=0,*atlasEnd=0;unsigned uvCount=0,atlasCount=0;
  for(unsigned i=0;i<count;++i) {
   IMAGE_SECTION_HEADER& sec=sections[i];
   if(!(sec.Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
   unsigned n=sec.Misc.VirtualSize;
   if(sec.VirtualAddress>size || n>size-sec.VirtualAddress)return false;
   BYTE* begin=image+sec.VirtualAddress;BYTE* end=begin+n;
   // Count every match, including duplicates in one section. Never pick the first.
   for(BYTE* at=begin;(size_t)(end-at)>=spec.uv.size;++at)if(Match(at,spec.uv)){uv=at;uvBegin=begin;++uvCount;}
   for(BYTE* at=begin;(size_t)(end-at)>=spec.atlas.size;++at)if(Match(at,spec.atlas)){atlas=at;atlasBegin=begin;atlasEnd=end;++atlasCount;}
  }
  if(uvCount!=1 || atlasCount!=1)continue;
  BYTE *body=Before(uvBegin,uv,spec.uvBody),*entry=Before(atlasBegin,atlas,spec.entry),*layout=Before(atlasBegin,atlas,spec.layout);
  BYTE *post=After(atlas+spec.atlas.size,atlasEnd,spec.post),*next=After(atlas+spec.atlas.size,atlasEnd,spec.next);
  if(!body || !entry || !layout || !post || !next || layout<entry || post>=next)continue;
  // Masked displacements are still constrained by their actual control-flow targets.
  int displacement=0;memcpy(&displacement,atlas+spec.gateJump+2,4);
  if((intptr_t)(atlas+spec.gateJump+6)+displacement!=(intptr_t)next)continue;
  memcpy(&displacement,next+spec.loopJump+2,4);
  if((intptr_t)(next+spec.loopJump+6)+displacement!=(intptr_t)atlas)continue;
  selected={spec.abi,uv+spec.uvOffset,atlas+spec.clearOffset,body,entry};++families;
 }
 if(families!=1)return false;
 *output=selected;return true;
}
bool FindRepairSites(BYTE* image,RepairSites* output) {
 RepairSites result={};bool ok=false;
 if(!image || !output || !Guard([&](){ok=Resolve(image,&result);}) || !ok)return false;
 *output=result;return true;
}
