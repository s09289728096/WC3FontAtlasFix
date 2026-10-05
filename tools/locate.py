"""Conservative masked signatures plus bounded ABI/control-flow witnesses."""
import json
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]

def specs():
    return json.loads((ROOT/'profiles/locators.json').read_text())

def hits(data,pattern):
    needle=bytes.fromhex(pattern['bytes']);mask=bytes.fromhex(pattern['mask'])
    prefix=needle[:next((i for i,b in enumerate(mask) if not b),len(mask))]
    if not prefix or len(needle)!=len(mask):raise ValueError('Invalid locator pattern')
    found=[];at=data.find(prefix)
    while at>=0:
        if at+len(needle)<=len(data) and all((data[at+i]&m)==(needle[i]&m) for i,m in enumerate(mask)):found.append(at)
        at=data.find(prefix,at+1)
    return found

def resolve_sections(sections, families=None):
    accepted=[]
    for spec in specs() if families is None else families:
        pats=spec['patterns'];locations={}
        for name in ['uv','atlas']:
            candidates=[(base,raw,at) for base,raw in sections for at in hits(raw,pats[name])]
            if len(candidates)==1:locations[name]=candidates[0]
        if len(locations)!=2:continue
        ub,ur,ua=locations['uv'];ab,ar,aa=locations['atlas']
        def before(raw,at,name):
            start=max(0,at-768);found=hits(raw[start:at],pats[name])
            return start+found[0] if len(found)==1 else None
        def after(raw,at,name):
            found=hits(raw[at:at+768],pats[name]);return at+found[0] if len(found)==1 else None
        body=before(ur,ua,'uv_body');entry=before(ar,aa,'entry');layout=before(ar,aa,'layout')
        end=aa+len(bytes.fromhex(pats['atlas']['bytes']));post=after(ar,end,'post');nxt=after(ar,end,'next')
        if None in (body,entry,layout,post,nxt) or layout<entry or post>=nxt:continue
        def target(at):return at+6+struct.unpack_from('<i',ar,at+2)[0]
        if target(aa+spec['gate_jump'])!=nxt or target(nxt+spec['loop_jump'])!=aa:continue
        accepted.append({'abi':spec['abi'],'uv_rva':ub+ua+spec['uv_offset'],'clear_rva':ab+aa+spec['clear_offset'],'uv_body_rva':ub+body,'atlas_entry_rva':ab+entry})
    return accepted[0] if len(accepted)==1 else None

def emit_patterns(path):
    data=specs();lines=['#pragma once']
    for n,s in enumerate(data):
        for name,pat in s['patterns'].items():
            for field in ['bytes','mask']:
                lines.append('static const unsigned char p%d_%s_%s[]={%s};'%(n,name,field,','.join('0x%02x'%v for v in bytes.fromhex(pat[field]))))
    lines.append('static const LocatorSpec kLocators[]={')
    for n,s in enumerate(data):
        patterns=','.join('{p%d_%s_bytes,p%d_%s_mask,sizeof(p%d_%s_bytes)}'%(n,k,n,k,n,k) for k in ['uv','atlas','uv_body','entry','layout','post','next'])
        lines.append('{%d,%d,%d,%d,%d,%s},'%({'frame':0,'stack':1}[s['abi']],s['uv_offset'],s['clear_offset'],s['gate_jump'],s['loop_jump'],patterns))
    lines.append('};');Path(path).write_text('\n'.join(lines)+'\n')
