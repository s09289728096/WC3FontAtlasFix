import hashlib,json,struct,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze import PE,analyze,profiles,emit_header
from locate import specs,resolve_sections

class AnalysisTests(unittest.TestCase):
    def fixture(self,abi='frame',shift=0):
        spec=next(s for s in specs() if s['abi']==abi);p=next(t.copy() for t in profiles() if t['abi']==abi)
        raw=bytearray(0x1400);where={'uv_body':0x100,'uv':0x250,'entry':0x400,'layout':0x460,'atlas':0x500,'post':0x600,'next':0x660}
        where={k:v+shift for k,v in where.items()}
        for name,at in where.items():
            code=bytes.fromhex(spec['patterns'][name]['bytes']);raw[at:at+len(code)]=code
        jump=where['atlas']+spec['gate_jump'];struct.pack_into('<i',raw,jump+2,where['next']-jump-6)
        jump=where['next']+spec['loop_jump'];struct.pack_into('<i',raw,jump+2,where['atlas']-jump-6)
        b=bytearray(0x200)+raw;b[:2]=b'MZ';struct.pack_into('<I',b,0x3c,0x80);b[0x80:0x84]=b'PE\0\0'
        struct.pack_into('<HHI',b,0x84,0x14c,1,p['timestamp']);struct.pack_into('<H',b,0x94,224);struct.pack_into('<H',b,0x98,0x10b)
        struct.pack_into('<I',b,0x98+56,p['image_size']);struct.pack_into('<IIII',b,0x178+8,len(raw),0x1000,len(raw),0x200);struct.pack_into('<I',b,0x178+36,0x60000020)
        p['sha256']=hashlib.sha256(b).hexdigest();return b,p,spec,where
    def resolve(self,b):
        pe=PE(b);return resolve_sections([(base,b[raw:raw+size]) for base,raw,size,flags in pe.sections if flags&0x20000000])
    def test_both_abis_and_moved_code(self):
        for abi in ['frame','stack']:
            b,p,s,w=self.fixture(abi);initial=self.resolve(b)
            b,p,s,w=self.fixture(abi,73);moved=self.resolve(b)
            self.assertEqual(moved['uv_rva'],initial['uv_rva']+73);self.assertEqual(moved['clear_rva'],initial['clear_rva']+73)
    def test_known_and_unknown_hash(self):
        b,p,s,w=self.fixture()
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'Game.dll';f.write_bytes(b);self.assertTrue(analyze(f,[p])[0]['supported'])
            b[-1]^=1;f.write_bytes(b);report,match=analyze(f,[p]);self.assertIsNone(match);self.assertIsNotNone(report['candidate_sites'])
    def test_duplicate_anchor_rejected(self):
        b,p,s,w=self.fixture();code=bytes.fromhex(s['patterns']['uv']['bytes']);b[0xb00:0xb00+len(code)]=code;self.assertIsNone(self.resolve(b))
    def test_wrong_branch_target_rejected(self):
        b,p,s,w=self.fixture();b[0x200+w['atlas']+s['gate_jump']+2]^=1;self.assertIsNone(self.resolve(b))
    def test_wrong_layout_rejected(self):
        b,p,s,w=self.fixture();b[0x200+w['layout']+2]^=1;self.assertIsNone(self.resolve(b))
    def test_reject_x64_truncated_and_non_executable(self):
        b,p,s,w=self.fixture();struct.pack_into('<H',b,0x84,0x8664)
        with self.assertRaises(ValueError):PE(b)
        with self.assertRaises(ValueError):PE(b'MZ')
        b,p,s,w=self.fixture();struct.pack_into('<I',b,0x178+36,0x40000040);self.assertIsNone(self.resolve(b))
    def test_unknown_layout_cannot_generate(self):
        p=profiles()[0].copy();p['abi']='unknown'
        with self.assertRaises(ValueError):emit_header(p,'unused.h')
if __name__=='__main__':unittest.main()
