import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze import PE, analyze, emit_header, profiles

class AnalysisTests(unittest.TestCase):
    def fixture(self):
        p=json.loads(json.dumps(profiles()[0])); b=bytearray(0x800)
        b[:2]=b'MZ';struct.pack_into('<I',b,0x3c,0x80);b[0x80:0x84]=b'PE\0\0'
        struct.pack_into('<HHI',b,0x84,0x14c,1,p['timestamp']);struct.pack_into('<H',b,0x94,224);struct.pack_into('<H',b,0x98,0x10b)
        struct.pack_into('<I',b,0x98+56,p['image_size']);struct.pack_into('<IIII',b,0x178+8,0x600,0x1000,0x600,0x200);struct.pack_into('<I',b,0x178+36,0x60000020)
        p['atlas_signature_rva']=0x1000;p['clear_rva']=0x100d
        sig=bytes.fromhex(p['atlas_signature']);b[0x200:0x200+len(sig)]=sig
        for i,h in enumerate(p['hooks']):
            h['rva']=0x1080+i*32;sig=bytes.fromhex(h['bytes']);off=0x280+i*32;b[off:off+len(sig)]=sig
        p['sha256']=hashlib.sha256(b).hexdigest()
        return b,p
    def test_known_and_unknown_hash(self):
        b,p=self.fixture()
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'Game.dll';f.write_bytes(b)
            self.assertTrue(analyze(f,[p])[0]['supported'])
            b[-1]^=1;f.write_bytes(b)
            report,match=analyze(f,[p]);self.assertIsNone(match);self.assertIn('candidate_rvas',report)
    def test_signature_mismatch_fails_closed(self):
        b,p=self.fixture();b[0x280]^=1;p['sha256']=hashlib.sha256(b).hexdigest()
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'Game.dll';f.write_bytes(b);self.assertFalse(analyze(f,[p])[0]['supported'])
    def test_reject_x64_truncated_and_non_executable(self):
        b,p=self.fixture();struct.pack_into('<H',b,0x84,0x8664)
        with self.assertRaises(ValueError):PE(b)
        with self.assertRaises(ValueError):PE(b'MZ')
        b,p=self.fixture();struct.pack_into('<I',b,0x178+36,0x40000040)
        with self.assertRaises(ValueError):PE(b).read_code(0x1000,6)
    def test_unknown_layout_cannot_generate(self):
        p=profiles()[0].copy();p['abi']='unknown'
        with self.assertRaises(ValueError):emit_header(p,'unused.h')

if __name__=='__main__':unittest.main()
