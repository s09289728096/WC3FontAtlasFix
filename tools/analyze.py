"""Local PE inspection; never executes or uploads the input DLL."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]

class PE:
    def __init__(self, data):
        self.data = data
        def unpack(fmt, offset):
            try:
                return struct.unpack_from(fmt, data, offset)
            except struct.error as exc:
                raise ValueError('Truncated PE') from exc
        if data[:2] != b'MZ':
            raise ValueError('Not a PE image')
        nt, = unpack('<I', 0x3c)
        if data[nt:nt+4] != b'PE\0\0':
            raise ValueError('Invalid PE signature')
        self.machine, count, self.timestamp = unpack('<HHI', nt+4)
        optional_size, = unpack('<H', nt+20)
        magic, = unpack('<H', nt+24)
        if self.machine != 0x14c or magic != 0x10b:
            raise ValueError('Only Windows x86 PE32 is supported')
        if optional_size < 96 or not 0 < count <= 96:
            raise ValueError('Invalid PE headers')
        self.image_size, = unpack('<I', nt+24+56)
        self.sections = []
        for i in range(count):
            at = nt+24+optional_size+i*40
            virtual_size, rva, size, raw = unpack('<IIII', at+8)
            flags, = unpack('<I', at+36)
            if raw+size > len(data):
                raise ValueError('Section outside file')
            self.sections.append((rva, raw, size, flags))

    def read_code(self, rva, size):
        for start, raw, length, flags in self.sections:
            if flags & 0x20000000 and start <= rva and rva+size <= start+length:
                return self.data[raw+rva-start:raw+rva-start+size]
        raise ValueError('Hook is not backed by executable file bytes')

    def candidates(self, pattern):
        found = []
        for rva, raw, size, flags in self.sections:
            if not flags & 0x20000000:
                continue
            section = self.data[raw:raw+size]
            at = section.find(pattern)
            while at >= 0 and len(found) < 20:
                found.append(rva+at)
                at = section.find(pattern, at+1)
        return found

def profiles():
    return [json.loads(p.read_text(encoding='utf-8')) for p in sorted((ROOT/'profiles').glob('classic-*.json'))]

def analyze(path, available=None):
    from locate import resolve_sections
    data=Path(path).read_bytes()
    report={'sha256':hashlib.sha256(data).hexdigest(),'supported':False}
    try:
        pe=PE(data)
        report.update(machine=pe.machine,timestamp=pe.timestamp,image_size=pe.image_size)
        sites=resolve_sections([(base,data[raw:raw+size]) for base,raw,size,flags in pe.sections if flags&0x20000000])
        report['candidate_sites']=sites
        for p in profiles() if available is None else available:
            if p['sha256']!=report['sha256']:continue
            if p['machine']!=pe.machine or p['timestamp']!=pe.timestamp or p['image_size']!=pe.image_size:
                raise ValueError('Profile metadata mismatch')
            if not sites or sites['abi']!=p['abi']:
                raise ValueError('Unique locator/ABI/control-flow validation failed')
            report.update(supported=True,profile=p['name'],debug_supported=p.get('debug_supported',False))
            return report,p
        report['reason']='Unknown SHA256; candidate sites are NOT authorization to patch'
    except ValueError as exc:
        report['reason']=str(exc)
    return report,None

def emit_header(p,path):
    from locate import emit_patterns
    if p['abi'] not in ('frame','stack'):raise ValueError('Unsupported structure layout ABI')
    def values(h):return ','.join('0x%02x'%v for v in bytes.fromhex(h))
    lines=['#pragma once','struct TrustedBuild { unsigned char hash[32]; unsigned long timestamp,imageSize; unsigned abi; bool debugSupported; };','static const TrustedBuild kTrustedBuilds[]={']
    for target in profiles():
        lines.append('{{%s},0x%x,0x%x,%d,%s},'%(values(target['sha256']),target['timestamp'],target['image_size'],{'frame':0,'stack':1}[target['abi']],'true' if target.get('debug_supported') else 'false'))
    lines+=['};','#ifdef WC3_DEBUG']
    # Additional diagnostic sites remain limited to the reviewed Debug target.
    debug=next(t for t in profiles() if t.get('debug_supported'))
    for h in debug['hooks']:
        if h['id']==2:continue
        lines+=['static const unsigned long kRva%d=0x%x;'%(h['id'],h['rva']), 'static const unsigned char kSig%d[]={%s};'%(h['id'],values(h['bytes']))]
    lines.append('#endif')
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n')
    emit_patterns(path.with_name('locator_patterns.h'))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('game', type=Path)
    ap.add_argument('--out', type=Path, default=ROOT/'build/analysis.json')
    args=ap.parse_args()
    report, profile=analyze(args.game)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    return 0 if profile else 2

if __name__=='__main__':
    raise SystemExit(main())
