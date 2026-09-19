"""Build a Windows x86 MIX with LLVM-MinGW on Linux."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from analyze import ROOT, analyze, emit_header, profiles

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--debug',action='store_true',help='Include logging, hotkey snapshots and diagnostic hooks')
    ap.add_argument('--tests',action='store_true',help='Also build the synthetic Windows test executable')
    ap.add_argument('--game',type=Path,help='Locally validate and select a profile from your Game.dll')
    ap.add_argument('--toolchain',type=Path,help='LLVM-MinGW root; otherwise use PATH')
    args=ap.parse_args()
    if args.toolchain: args.toolchain=args.toolchain.resolve()
    out=ROOT/'build'/('debug' if args.debug else 'release')
    out.mkdir(parents=True,exist_ok=True)
    if args.game:
        report,p=analyze(args.game)
        (out/'analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        if not p:
            raise SystemExit('Unsupported Game.dll; see analysis.json. No new MIX was built.')
    else:
        p=next(x for x in profiles() if x['name']=='classic-1.28.5.7680-x86')
    emit_header(p,out/'profile.h')
    def compiler(name):
        candidate=str(args.toolchain/'bin'/name) if args.toolchain else shutil.which(name)
        if not candidate or not Path(candidate).is_file():
            raise SystemExit('LLVM-MinGW missing: '+name+' (see docs/build.md)')
        return candidate
    cc=compiler('i686-w64-mingw32-clang');cxx=compiler('i686-w64-mingw32-clang++')
    # Fixed source paths avoid leaking checkout paths into the artifact.
    common=['-O2','-Wall','-Wextra','-Wno-unused-parameter','-Wno-unused-function','-fms-extensions','-fasm-blocks','-ffile-prefix-map='+str(ROOT)+'=.','-I',str(out),'-I','src','-I','vendor/minhook/include','-D_WIN32_WINNT=0x0601','-D_CRT_SECURE_NO_WARNINGS']
    if args.debug: common+=['-DWC3_DEBUG=1']
    sources=['src/runtime.cpp','src/repair.cpp','src/hooks.cpp']
    if args.debug: sources+=['src/diag.cpp']
    sources += ['vendor/minhook/src/'+p for p in ['buffer.c','hook.c','trampoline.c','hde/hde32.c']]
    objects=[]
    for source in sources:
        obj=out/(Path(source).stem+'.o');objects.append(str(obj))
        subprocess.run([cxx if source.endswith('.cpp') else cc,*common,'-c',source,'-o',str(obj)],cwd=ROOT,check=True)
    # Isolate native x86 SEH in one C ABI object. GNU x86 exception tables cannot
    # represent MS __try; no C++ objects cross this boundary.
    guard=out/'guard.o'
    subprocess.run([cc,'--target=i686-pc-windows-msvc','-O2','-c','src/guard.c','-o',str(guard)],cwd=ROOT,check=True)
    objects.append(str(guard))
    target=out/'War3FontAtlasFix.mix'
    subprocess.run([cxx,'-shared','-static','-Wl,--no-insert-timestamp','-Wl,--dynamicbase','-Wl,--nxcompat','-o',str(target),*objects,'-ladvapi32',*(['-luser32'] if args.debug else [])],cwd=ROOT,check=True)
    manifest={'mode':'debug' if args.debug else 'release','profile':p['name'],'game_sha256':p['sha256'],'mix_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'compiler':subprocess.check_output([cxx,'--version'],text=True).splitlines()[0]}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    if args.tests:
        subprocess.run([cxx,*common,'tests/runtime.cpp',*[o for o in objects if Path(o).name!='runtime.o'],'-static','-o',str(out/'runtime-test.exe'),'-ladvapi32',*(['-luser32'] if args.debug else [])],cwd=ROOT,check=True)
        subprocess.run([cxx,*common,'tests/loader.cpp','-static','-o',str(out/'loader-test.exe')],cwd=ROOT,check=True)
        mock=ROOT/'build/mock';mock.mkdir(exist_ok=True)
        subprocess.run([cc,'-shared','-static','tests/mock_game.c','-o',str(mock/'Game.dll')],cwd=ROOT,check=True)
    print(target)

if __name__=='__main__': main()
