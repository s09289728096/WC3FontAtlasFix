#include "guard.h"
#include "diag.h"
#include <stdio.h>
#include <string.h>
struct Channel {
 Event records[2048]; volatile LONG lock; unsigned read,write;
 FILE* file; unsigned fileIndex; size_t fileBytes; DWORD sequence;
};
static Channel channels[2]={}; // 0: critical lifecycle, 1: throttled UI observations
static volatile LONG queueLock=0; // snapshot reservation only
volatile LONG diagDropped=0,diagFaults=0,diagIoErrors=0,criticalDropped=0,telemetryDropped=0;
volatile LONG repairEnabled=1,reusedClean=0,rearmedGlyphs=0,skippedMismatch=0,verifiedGlyphs=0,verifyMismatch=0,verifyUnavailable=0;
static volatile LONG capturing=0;
static DWORD deadline=0;
DWORD captureSerial=0,captureCompleted=0;
SnapshotSlot snapshots[8]={};
static wchar_t root[MAX_PATH],captureDir[MAX_PATH];
static DWORD Load(DWORD p,unsigned o=0) { return *(DWORD*)(p+o); }
static bool IsCritical(DWORD kind) {return kind==2 || kind==3 || kind==10 || kind==11 || kind==12 || kind==16 || kind==17 || kind>=100;}
static void Dropped(bool critical) {InterlockedIncrement(&diagDropped);InterlockedIncrement(critical?&criticalDropped:&telemetryDropped);}
static void Enqueue(Event& event) {
 event.tick=GetTickCount();event.thread=GetCurrentThreadId();
 bool critical=IsCritical(event.kind);Channel& c=channels[critical?0:1];
 if(InterlockedCompareExchange(&c.lock,1,0)) {Dropped(critical);return;}
 unsigned next=(c.write+1)%2048;
 if(next==c.read) Dropped(critical);
 else {c.records[c.write]=event;c.write=next;}
 InterlockedExchange(&c.lock,0);
}
extern "C" DWORD CompareGlyph(DWORD page,DWORD slot,DWORD y,DWORD rowHeight,DWORD* compared) {
 *compared=0;
 // Caller uses SEH. Unsupported outline mode is explicitly excluded from evidence.
 DWORD font=Load(page,12),glyph=Load(slot,0x3c),pixels=Load(page,4);
 if(!font || !glyph || !pixels || (Load(font,0x1b8)&1))return 0xffffffff;
 DWORD x=Load(slot,0x34),end=Load(slot,0x38),w=Load(glyph,0x28),h=Load(glyph,0x2c),pitch=Load(glyph,0x3c),top=Load(glyph,0x44);
 DWORD source=Load(glyph,0x1c),size=Load(glyph,0x20);
 if(x>end || end>=256 || y>=256 || !rowHeight || rowHeight>256-y || w>end-x+1 || top>rowHeight || h>rowHeight-top || pitch<w)return 0xffffffff;
 if(w && h && (!source || !pitch || size>0x100000 || h>size/pitch))return 0xffffffff;
 DWORD mismatch=0;
 for(DWORD row=0;row<rowHeight;++row)for(DWORD col=0;col<=end-x;++col) {
  BYTE expected=0;
  if(row>=top && row-top<h && col<w)expected=*(BYTE*)(source+(row-top)*pitch+col);
  BYTE actual=*(BYTE*)(pixels+((y+row)*256+x+col)*4+3);
  mismatch+=expected!=actual;++*compared;
 }
 return mismatch;
}
static void VerifyStage(DWORD kind,Registers* r) {
 DWORD page=Load(r->ebp-4),slot=r->edx,glyph=Load(slot,0x3c),dirty=Load(glyph,0x24);
 if(kind==16 && dirty)return; // Only inspect glyphs the original dirty gate will skip.
 DWORD compared=0,y=Load(r->ebp-8)/256,height=Load(r->ebp,0x10);
 DWORD mismatch=CompareGlyph(page,slot,y,height,&compared);
 if(mismatch==0xffffffff) {InterlockedIncrement(&verifyUnavailable);return;}
 if(kind==16 && !mismatch)return;
 if(kind==16)InterlockedIncrement(&skippedMismatch);
 else {InterlockedIncrement(&verifiedGlyphs);if(mismatch)InterlockedIncrement(&verifyMismatch);}
 Event e={};e.kind=kind;
 e.data[0]=Load(page,12);e.data[1]=page;e.data[2]=slot;e.data[3]=glyph;e.data[4]=Load(glyph);
 e.data[5]=Load(page,0x10);e.data[6]=y;e.data[7]=Load(slot,0x34);e.data[8]=Load(slot,0x38);
 e.data[9]=height;e.data[10]=dirty;e.data[11]=mismatch;e.data[12]=compared;e.data[13]=repairEnabled;
 Enqueue(e);
}
static bool ReadText(DWORD address,char* out,unsigned cap) {
 unsigned i=0; bool ended=false;
 if(!Guard([&]() { for(;i+1<cap;++i) { out[i]=*(char*)(address+i); if(!out[i]) {ended=true;break;} } })) {InterlockedIncrement(&diagFaults);}
 out[i]=0; return ended;
}
// No heap allocation, disk I/O, game calls or wait in render-thread callbacks.
static void CaptureFont(DWORD font) {
 if(!font || !InterlockedCompareExchange(&capturing,0,0)) return;
 // Claim a font while holding only the nonblocking queue lock; never wait for a producer.
 if(InterlockedCompareExchange(&queueLock,1,0)) return;
 if(!InterlockedCompareExchange(&capturing,0,0)) {InterlockedExchange(&queueLock,0);return;}
 SnapshotSlot* chosen=0;
 for(unsigned i=0;i<8;++i) if(snapshots[i].state && snapshots[i].value.font==font) {
  InterlockedExchange(&queueLock,0); return;
 }
 for(unsigned i=0;i<8;++i) if(!snapshots[i].state) {
  chosen=&snapshots[i]; chosen->value.font=font; chosen->state=1; break;
 }
 InterlockedExchange(&queueLock,0);
 if(!chosen) return;
 Snapshot& s=chosen->value;
 s.magic=0x53414657; s.version=1; s.capture=captureSerial; s.tick=GetTickCount();
 s.status=0; s.pageMask=0;s.glyphCount=0;s.sourceUsed=0;
 if(!Guard([&]() {
  memcpy(s.fontBytes,(void*)font,sizeof(s.fontBytes));
  for(unsigned page=0;page<8;++page) {
   DWORD p=font+0x1c8+page*0x20;
   if(!Load(p,4) && !Load(p,0x18))continue;
   if(Load(p,0xc)!=font) { s.status|=1;continue; }
   DWORD pixels=Load(p,4),rows=Load(p,0x1c),count=Load(p,0x18);
   if(pixels) { memcpy(s.atlas[page],(void*)pixels,0x40000);s.pageMask|=1<<page; }
   if(count>256 || (count && !rows)) {s.status|=2;continue;}
   for(unsigned row=0;row<count;++row) {
    DWORD rowp=rows+16*row,slot=Load(rowp,0xc),linkOffset=Load(rowp,4);
    unsigned visited=0;
    while((LONG)slot>0 && visited++<256) {
     if(s.glyphCount>=1024) {s.status|=4;break;}
     GlyphCopy& g=s.glyphs[s.glyphCount++];
     g.slot=slot;g.page=page;g.row=row;g.sourceOffset=0;g.sourceSize=0;
     memcpy(g.slotBytes,(void*)slot,64);
     DWORD glyph=Load(slot,0x3c);
     if(glyph) {
      memcpy(g.glyphBytes,(void*)glyph,92);
      DWORD n=Load(glyph,0x20),source=Load(glyph,0x1c);
      if(source && n && n<=0x10000 && n<=sizeof(s.source)-s.sourceUsed) {
       g.sourceOffset=s.sourceUsed;g.sourceSize=n;
       memcpy(s.source+s.sourceUsed,(void*)source,n);s.sourceUsed+=n;
      } else if(source && n) s.status|=8;
     } else memset(g.glyphBytes,0,92);
     DWORD next=Load(slot,linkOffset+4);
     if(next==slot) {s.status|=16;break;} slot=next;
    }
    if(visited>256) s.status|=16;
   }
  }
 })) {s.status|=32;InterlockedIncrement(&diagFaults);}
 InterlockedExchange(&chosen->state,2);
}
struct TextSeen { DWORD object,last,source,dirty,mask,capture; };
static TextSeen seen[4096]={};
static volatile LONG seenLock=0;
static bool ShouldRecordText(DWORD object,DWORD source,DWORD dirty,DWORD mask,DWORD kind) {
 DWORD now=GetTickCount(); bool yes=false;
 if(InterlockedCompareExchange(&seenLock,1,0)) return false;
 TextSeen& s=seen[((object>>3)^kind*131)%4096];
 yes=s.object!=object || s.source!=source || s.dirty!=dirty || s.mask!=mask || now-s.last>=5000 || s.capture!=captureSerial;
 if(yes) {s.object=object;s.source=source;s.dirty=dirty;s.mask=mask;s.last=now;s.capture=captureSerial;}
 InterlockedExchange(&seenLock,0); return yes;
}
extern "C" void __stdcall Observe(DWORD kind,Registers* r) {
 Event e={};e.kind=kind;
 if(!Guard([&]() {
  DWORD esp=r->savedEsp+4,object=0,font=0,slot=0;
  switch(kind) {
  case 1: return; // Disabled in 0.3; no high-frequency lookup hooks.

  case 2: // UV calculation completed; EDI still slot, EAX will be restored by trampoline.
   slot=r->edi;break;
  case 3: // Before reclaim; font, page, row, first victim.
   font=r->ebx;e.data[0]=font;e.data[1]=r->edx;e.data[2]=r->esi;
   slot=Load(r->ebp-0xc);e.data[3]=slot;
   if(slot) {e.data[4]=Load(slot);e.data[5]=Load(slot,0x2c);e.data[6]=Load(slot,0x30);}
   Enqueue(e);CaptureFont(font);return;
  case 4: // Invalidation entry: predicted mask test, not proof of subsequent completion.
   object=r->ecx;e.data[4]=Load(esp,4);break;
  case 5:object=r->edi;break; // draw A, before dirty test
  case 6:object=r->esi;break; // draw B
  case 7:object=r->edi;e.data[4]=r->eax;break; // validator return A
  case 8:object=r->esi;e.data[4]=r->eax;break; // validator return B
  case 9:object=r->ecx;break; // geometry rebuild entry
  case 16:case 17:VerifyStage(kind,r);return;
  case 13:case 14:case 15:return;
  case 10:case 11:slot=r->esi;break; // each actual victim prior to free
  default:return;
  }
  if(slot) {
   e.data[0]=slot;e.data[1]=Load(slot);e.data[2]=Load(slot,0x2c);e.data[3]=Load(slot,0x30);
   e.data[4]=Load(slot,0x34);e.data[5]=Load(slot,0x38);DWORD glyph=Load(slot,0x3c);e.data[6]=glyph;
   if(glyph) {e.data[7]=Load(glyph);e.data[8]=Load(glyph,0x18);e.data[9]=Load(glyph,0x24);memcpy(e.data+10,(void*)(glyph+0x48),16);}
  }
  if(kind==2 && slot && e.data[6]) {
   // Original UV assignment only sets +58. Preserve pre-write dirty as causal evidence.
   DWORD glyph=e.data[6];DWORD oldDirty=e.data[9];
   if(!oldDirty)InterlockedIncrement(&reusedClean);
   bool fix=InterlockedCompareExchange(&repairEnabled,0,0)!=0;
   if(fix) {InterlockedExchange((volatile LONG*)(glyph+0x24),1);if(!oldDirty)InterlockedIncrement(&rearmedGlyphs);}
   e.data[16]=Load(glyph,0x24);e.data[17]=fix;
   if(r->ebp){e.data[18]=Load(r->ebp,8);e.data[19]=Load(r->ebp,12);}
   e.data[20]=Load(glyph,0x1c);e.data[21]=Load(glyph,0x20);e.data[22]=Load(glyph,0x58);
  }
  if(object) {
   font=Load(object,0x44);DWORD source=Load(object,0x5c),dirty=Load(object,0x80),mask=Load(object,0x7c);
   CaptureFont(font);
   if(kind!=4 && !ShouldRecordText(object,source,dirty,mask,kind))return;
   e.data[0]=object;e.data[1]=font;e.data[2]=dirty;e.data[3]=mask;e.data[5]=source;
   e.data[6]=ReadText(source,e.text,sizeof(e.text))?0:1;
   // Cache container header only; format of GPU geometry is not inferred here.
   memcpy(e.data+8,(void*)(object+0x48),12);
  }
  Enqueue(e);
 })) {InterlockedIncrement(&diagFaults);}
}
void DiagClear(void* page,void* slot,void* frame) {
 Event e={};e.kind=12;
 if(!Guard([&]() {
  DWORD p=(DWORD)page,s=(DWORD)slot,f=(DWORD)frame;
  e.data[0]=Load(p,0xc);e.data[1]=p;e.data[2]=Load(p,0x10);e.data[3]=s;
  e.data[4]=Load(s);e.data[5]=Load(s,0x34);e.data[6]=Load(s,0x38);e.data[7]=Load(f-8)/256;e.data[8]=Load(f,0x10);
  e.data[9]=Load(s,0x3c);Enqueue(e);
 })) {InterlockedIncrement(&diagFaults);}
}
static const wchar_t* ChannelName(unsigned n) {return n?L"events":L"critical";}
static void OpenEvents(unsigned n) {
 Channel& c=channels[n];wchar_t path[MAX_PATH];swprintf_s(path,L"%s\\%s-%u.bin",root,ChannelName(n),c.fileIndex);
 _wfopen_s(&c.file,path,L"wb");c.fileBytes=0;
 if(c.file){DWORD h[4]={0x44414657,1,sizeof(Event),++c.sequence};fwrite(h,1,sizeof(h),c.file);c.fileBytes=sizeof(h);}
 else InterlockedIncrement(&diagIoErrors);
}
bool DiagInit(const wchar_t* directory) {
 wcscpy_s(root,directory);CreateDirectoryW(root,0);OpenEvents(0);OpenEvents(1);return channels[0].file && channels[1].file;
}
void DiagTrigger() {
 if(captureSerial!=captureCompleted) return;
 if(captureSerial>=4 || InterlockedCompareExchange(&capturing,0,0)) return;
 for(unsigned i=0;i<8;++i) if(snapshots[i].state==1 || snapshots[i].state==2) return;
 // A closing epoch has no outstanding copy. Reset under same lock as producers.
 if(InterlockedCompareExchange(&queueLock,1,0)) return;
 for(unsigned i=0;i<8;++i) {snapshots[i].value.font=0;snapshots[i].state=0;}
 ++captureSerial;deadline=GetTickCount()+10000;
 swprintf_s(captureDir,L"%s\\capture-%lu",root,captureSerial);CreateDirectoryW(captureDir,0);
 InterlockedExchange(&capturing,1);InterlockedExchange(&queueLock,0);
 Event e={};e.kind=100;e.data[0]=captureSerial;Enqueue(e);
}
void DiagPump() {
 static Event batch[64];
 for(unsigned channel=0;channel<2;++channel) {
  Channel& c=channels[channel];
  for(unsigned pass=0;pass<32;++pass) {
   unsigned count=0;
   if(InterlockedCompareExchange(&c.lock,1,0))break;
   while(c.read!=c.write && count<64){batch[count++]=c.records[c.read];c.read=(c.read+1)%2048;}
   InterlockedExchange(&c.lock,0);
   if(!count)break;
   if(c.file) {
    if(c.fileBytes+count*sizeof(Event)>8*1024*1024){fclose(c.file);c.file=0;c.fileIndex=(c.fileIndex+1)%2;OpenEvents(channel);}
    if(c.file){size_t written=fwrite(batch,sizeof(Event),count,c.file);c.fileBytes+=written*sizeof(Event);if(written!=count)InterlockedIncrement(&diagIoErrors);}
   }
  }
  if(c.file)fflush(c.file);
 }
 for(unsigned i=0;i<8;++i) if(InterlockedCompareExchange(&snapshots[i].state,3,2)==2) {
  wchar_t path[MAX_PATH];swprintf_s(path,L"%s\\font-%08lX.bin",captureDir,snapshots[i].value.font);
  FILE* f=0;_wfopen_s(&f,path,L"wb");if(f){if(fwrite(&snapshots[i].value,1,sizeof(Snapshot),f)!=sizeof(Snapshot))InterlockedIncrement(&diagIoErrors);fclose(f);}else InterlockedIncrement(&diagIoErrors);
 }
 if(InterlockedCompareExchange(&capturing,0,0) && (LONG)(GetTickCount()-deadline)>=0) {
  InterlockedExchange(&capturing,0);
  Event e={};e.kind=101;e.data[0]=captureSerial;e.data[1]=diagDropped;e.data[2]=diagFaults;Enqueue(e);
  return; // Pump the queued end marker before archiving on the next worker iteration.
 }
 static DWORD archived=0;
 bool busy=false;for(unsigned i=0;i<8;++i) if(snapshots[i].state==1 || snapshots[i].state==2)busy=true;
 if(captureSerial && !InterlockedCompareExchange(&capturing,0,0) && !busy && archived!=captureSerial) {
  for(unsigned n=0;n<2;++n) {
   Channel& c=channels[n];if(c.file){fclose(c.file);c.file=0;}
   for(unsigned i=0;i<2;++i) {
    wchar_t from[MAX_PATH],to[MAX_PATH];swprintf_s(from,L"%s\\%s-%u.bin",root,ChannelName(n),i);swprintf_s(to,L"%s\\%s-%u.bin",captureDir,ChannelName(n),i);
    if(!CopyFileW(from,to,FALSE) && GetLastError()!=ERROR_FILE_NOT_FOUND)InterlockedIncrement(&diagIoErrors);
   }
   wchar_t current[MAX_PATH];swprintf_s(current,L"%s\\%s-%u.bin",root,ChannelName(n),c.fileIndex);
   if(_wfopen_s(&c.file,current,L"ab"))InterlockedIncrement(&diagIoErrors);
  }
  archived=captureSerial;captureCompleted=captureSerial;
 }
}
