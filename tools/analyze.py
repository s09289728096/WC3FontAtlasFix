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
    return [json.loads(p.read_text(encoding='utf-8')) for p in sorted((ROOT/'profiles').glob('*.json'))]

def analyze(path, available=None):
    data = Path(path).read_bytes()
    report = {'sha256': hashlib.sha256(data).hexdigest(), 'supported': False}
    try:
        pe = PE(data)
        report.update(machine=pe.machine, timestamp=pe.timestamp, image_size=pe.image_size)
        choices = profiles() if available is None else available
        for p in choices:
            if p['sha256'] != report['sha256']:
                continue
            if p['abi'] != 'classic-font-v1' or p['machine'] != pe.machine or p['timestamp'] != pe.timestamp or p['image_size'] != pe.image_size:
                raise ValueError('Profile metadata/ABI mismatch')
            checks = [(p['atlas_signature_rva'], p['atlas_signature'])] + [(h['rva'], h['bytes']) for h in p['hooks']]
            for rva, signature in checks:
                expected = bytes.fromhex(signature)
                if pe.read_code(rva, len(expected)) != expected:
                    raise ValueError('Profile instruction mismatch at RVA %X' % rva)
            report.update(supported=True, profile=p['name'])
            return report, p
        # These are review hints, never an automatically approved new profile.
        report['reason'] = 'Unknown SHA256; developer review required'
        report['candidate_rvas'] = {p['name']: {'atlas': pe.candidates(bytes.fromhex(p['atlas_signature'])), 'uv_tail': pe.candidates(bytes.fromhex(p['hooks'][0]['bytes']))} for p in choices}
    except ValueError as exc:
        report['reason'] = str(exc)
    return report, None

def emit_header(p, path):
    if p['abi'] != 'classic-font-v1':
        raise ValueError('Unsupported structure layout ABI')
    def array(name, value):
        return 'static const unsigned char %s[]={%s};' % (name, ','.join('0x%02x'%b for b in bytes.fromhex(value)))
    lines = ['#pragma once', array('kExpectedHash', p['sha256']), array('kAtlasSignature', p['atlas_signature'])]
    for key, name in [('timestamp','kTimestamp'), ('image_size','kImageSize'), ('clear_rva','kClearRva'), ('atlas_signature_rva','kAtlasSignatureRva')]:
        lines.append('static const unsigned long %s=0x%x;' % (name,p[key]))
    for hook in p['hooks']:
        if hook['debug_only']: lines.append('#ifdef WC3_DEBUG')
        lines += ['static const unsigned long kRva%d=0x%x;' % (hook['id'],hook['rva']), array('kSig%d'%hook['id'],hook['bytes'])]
        if hook['debug_only']: lines.append('#endif')
    Path(path).parent.mkdir(parents=True,exist_ok=True)
    Path(path).write_text('\n'.join(lines)+'\n',encoding='utf-8')

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
