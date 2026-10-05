#include "guard.h"
#define WIN32_LEAN_AND_MEAN
#include "core.h"
#include "profile.h"
#include "locator.h"
#include "diag.h"
#include <wincrypt.h>
#include <stdio.h>
#include <stdarg.h>
#include "MinHook.h"
static HMODULE selfModule;
static const TrustedBuild* trustedBuild=0;
#ifdef WC3_DEBUG
static wchar_t logPath[MAX_PATH];
static void Log(const char* fmt, ...) {
    FILE* f=0; if (_wfopen_s(&f,logPath,L"ab") || !f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f,"%04u-%02u-%02u %02u:%02u:%02u ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    va_list args; va_start(args,fmt); vfprintf(f,fmt,args); va_end(args);
    fputs("\r\n",f); fclose(f);
}
#else
#define Log(...) ((void)0)
#endif
static bool HashMatches(HMODULE game) {

    wchar_t path[MAX_PATH]; DWORD n=GetModuleFileNameW(game,path,MAX_PATH);
    if (!n || n>=MAX_PATH) return false;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if (f==INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider=0; HCRYPTHASH hash=0; bool ok=false;
    if (CryptAcquireContextW(&provider,0,0,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
        BYTE buffer[16384],result[32]; DWORD count=0,length=32; BOOL readOk;
        bool healthy=true;
        while ((readOk=ReadFile(f,buffer,sizeof(buffer),&count,0)) && count) {
            if (!CryptHashData(hash,buffer,count,0)) { healthy=false; break; }
        }
        if(healthy && readOk && CryptGetHashParam(hash,HP_HASHVAL,result,&length,0) && length==32) {
            for(const TrustedBuild& build:kTrustedBuilds)if(!memcmp(result,build.hash,32)) {trustedBuild=&build;ok=true;break;}
        }
    }
    if(hash) CryptDestroyHash(hash); if(provider) CryptReleaseContext(provider,0); CloseHandle(f); return ok;
}
static bool MemoryMatches(HMODULE game) {

    bool matches=false;
    if(!Guard([&]() {
        BYTE* base=(BYTE*)game; IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return;
        IMAGE_NT_HEADERS32* nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
        matches=nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && trustedBuild && nt->FileHeader.TimeDateStamp==trustedBuild->timestamp && nt->OptionalHeader.SizeOfImage==trustedBuild->imageSize && FindRepairSites(base,&g_sites) && g_sites.abi==trustedBuild->abi;
    })) { matches=false; }
    return matches;
}
static DWORD WINAPI Worker(void*) {
    // No MinHook or file operations under the loader lock. Never wait in DllMain.
#ifdef WC3_DEBUG
    DWORD n=GetModuleFileNameW(selfModule,logPath,MAX_PATH);
    if(!n || n>=MAX_PATH) return 0;
    wchar_t* name=wcsrchr(logPath,L'\\'); if(!name) return 0;
    wcscpy_s(name+1,MAX_PATH-(name+1-logPath),L"War3FontAtlasFix.log");
#endif
    HMODULE pinned=0;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&Worker,&pinned)) return 0;
    Log("START WC3FontAtlasFix debug pid=%lu x86",GetCurrentProcessId());
    HMODULE game=0;
    for(unsigned i=0;i<300 && !game;++i) { game=GetModuleHandleW(L"Game.dll"); if(!game) Sleep(100); }
    if(!game) { Log("SKIP Game.dll not loaded after 30 seconds"); return 0; }
    if(!HashMatches(game)) { Log("SKIP unsupported Game.dll SHA256; no patch applied"); return 0; }
#ifdef WC3_DEBUG
    if(!trustedBuild->debugSupported) {Log("SKIP Debug observers are not validated for this target; use Release");return 0;}
#endif
    if(!MemoryMatches(game)) { Log("SKIP memory signature mismatch (possible debugger breakpoint or other hook)"); return 0; }
    if(!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)) { Log("SKIP FXSAVE unavailable"); return 0; }
    MH_STATUS status=MH_Initialize();
    if(status!=MH_OK) { Log("ERROR MH_Initialize: %s",MH_StatusToString(status)); return 0; }
    void* target=g_sites.clear;
    status=MH_CreateHook(target,g_sites.abi?(void*)LegacyClearStub:(void*)HookStub,&g_trampoline);
    if(status!=MH_OK) { Log("ERROR MH_CreateHook: %s",MH_StatusToString(status)); MH_Uninitialize(); return 0; }
#ifdef WC3_DEBUG
    wchar_t marker[MAX_PATH];wcscpy_s(marker,logPath);wchar_t* markerName=wcsrchr(marker,L'\\');
    wcscpy_s(markerName+1,MAX_PATH-(markerName+1-marker),L"War3FontAtlasFix.observe-only");
    InterlockedExchange(&repairEnabled,GetFileAttributesW(marker)==INVALID_FILE_ATTRIBUTES?1:0);
    Log("MODE reallocation_dirty_fix=%ld (observe-only marker disables only the new fix)",repairEnabled);
#endif
    if(!DiagPrepare((BYTE*)game)) { Log("ERROR diagnostic instruction signature or hook preparation failed; no hooks enabled"); MH_Uninitialize(); return 0; }
#ifdef WC3_DEBUG
    wchar_t folder[MAX_PATH]; SYSTEMTIME time;GetLocalTime(&time);
    wcscpy_s(folder,logPath);wchar_t* tail=wcsrchr(folder,L'\\');
    swprintf_s(tail+1,MAX_PATH-(tail+1-folder),L"font-diagnostic-%04u%02u%02u-%02u%02u%02u-%lu",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId());
    if(!DiagInit(folder)) {Log("ERROR cannot create diagnostic output directory");MH_Uninitialize();return 0;}
#endif
    status=MH_EnableHook(MH_ALL_HOOKS);
    if(status!=MH_OK) { Log("ERROR MH_EnableHook: %s",MH_StatusToString(status)); MH_RemoveHook(target); MH_Uninitialize(); return 0; }
    Log("INSTALLED Game.dll=%p hook=%p strategy=clear-owned-slot-before-dirty-glyph abi=%u uv=%p",game,target,g_sites.abi,g_sites.uv);
#ifdef WC3_DEBUG
    Log("DIAGNOSTICS folder=%ls hotkey=Ctrl+Shift+F8 capture=10s max=4",folder);
    DWORD last=GetTickCount(); bool keyWasDown=false; DWORD reportedCapture=0,reportedComplete=0;
    for(;;) {
        Sleep(100);
        bool keyDown=(GetAsyncKeyState(VK_CONTROL)&0x8000) && (GetAsyncKeyState(VK_SHIFT)&0x8000) && (GetAsyncKeyState(VK_F8)&0x8000);
        DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        if(keyDown && !keyWasDown && foregroundPid==GetCurrentProcessId()) DiagTrigger();
        keyWasDown=keyDown;
        DiagPump();
        if(reportedCapture!=captureSerial) {reportedCapture=captureSerial;Log("CAPTURE %lu requested; keep affected text visible for 10 seconds",captureSerial);}
        if(reportedComplete!=captureCompleted) {reportedComplete=captureCompleted;Log("CAPTURE %lu COMPLETE io_errors=%ld",captureCompleted,InterlockedCompareExchange(&diagIoErrors,0,0));}
        if(GetTickCount()-last>=10000) {
            last=GetTickCount();Log("COUNTERS cleared=%ld skipped=%ld dropped=%ld diagnostic_faults=%ld io_errors=%ld captures=%lu",InterlockedCompareExchange(&g_cleared,0,0),InterlockedCompareExchange(&g_skipped,0,0),InterlockedCompareExchange(&diagDropped,0,0),InterlockedCompareExchange(&diagFaults,0,0),InterlockedCompareExchange(&diagIoErrors,0,0),captureSerial);
            Log("PROOF clean_assignments=%ld rearmed=%ld skipped_mismatch=%ld verified=%ld verify_mismatch=%ld verify_unavailable=%ld critical_dropped=%ld telemetry_dropped=%ld",reusedClean,rearmedGlyphs,skippedMismatch,verifiedGlyphs,verifyMismatch,verifyUnavailable,criticalDropped,telemetryDropped);
        }
    }
#endif
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        selfModule=module;
        HANDLE worker=CreateThread(0,0,Worker,0,0,0);
        if(worker) CloseHandle(worker);
    }
    return TRUE;
}
