"""Check mode separation and PE shape without third-party packages."""
import hashlib
import json
from pathlib import Path
import struct
from analyze import ROOT, PE

def main():
    for mode in ['release','debug']:
        path=ROOT/'build'/mode/'War3FontAtlasFix.mix'
        data=path.read_bytes();pe=PE(data)
        nt=struct.unpack_from('<I',data,0x3c)[0]
        assert struct.unpack_from('<H',data,nt+22)[0]&0x2000, 'Not a DLL'
        manifest=json.loads(path.with_name('manifest.json').read_text())
        assert hashlib.sha256(data).hexdigest()==manifest['mix_sha256']
        diagnostic=b'GetAsyncKeyState' in data and b'PROOF clean_assignments=' in data
        if mode=='release':
            assert not diagnostic and b'GetAsyncKeyState' not in data
            assert 'War3FontAtlasFix.log'.encode('utf-16le') not in data
            assert pe.image_size<1024*1024, 'Diagnostic buffers in Release'
        else:
            assert diagnostic and pe.image_size>20*1024*1024
        print('PASS',mode,'x86 DLL; bytes=',len(data),'image=',pe.image_size)

if __name__=='__main__':main()
