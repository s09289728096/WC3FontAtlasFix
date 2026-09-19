"""Decode bounded MIX diagnostics; standard library only. Does not access game memory."""
from pathlib import Path
import argparse,struct,json,collections,zlib
EVENT_SIZE=876
NAMES={1:'lookup_entry',2:'uv_assigned',3:'reclaim_request',4:'invalidate_entry',5:'draw_a',6:'draw_b',7:'validator_a',8:'validator_b',9:'rebuild_entry',10:'victim_a',11:'victim_b',12:'clear_slot',13:'lookup_return_a',14:'lookup_return_b',15:'lookup_return_new',16:'clean_glyph_skipped_mismatch',17:'glyph_redraw_verified',100:'capture_begin',101:'capture_end'}
def character(n):
    return chr(n) if 0<n<=0x10ffff and not 0xd800<=n<=0xdfff else None

def events(path):
    with path.open('rb') as f:
        header=f.read(16)
        if len(header)!=16:return
        magic,version,size,sequence=struct.unpack('<4I',header)
        if (magic,version,size)!=(0x44414657,1,EVENT_SIZE):raise ValueError(f'Invalid event header: {path}')
        index=0
        while raw:=f.read(size):
            if len(raw)!=size:break # Last record can be incomplete if copied while game is running.
            words=struct.unpack_from('<27I',raw);kind,tick,thread=words[:3];d=list(words[3:])
            e={'kind':NAMES.get(kind,str(kind)),'tick_ms':tick,'thread':thread,'data':d,'sequence':sequence,'record':index}
            if kind in (4,5,6,7,8,9):
                b=raw[108:].split(b'\0',1)[0]
                e.update(object=f'{d[0]:08X}',font=f'{d[1]:08X}',dirty=d[2],page_mask=d[3],source=b.decode('utf-8',errors='replace'),source_truncated_or_fault=bool(d[6]))
                if kind in (7,8):e['validator_result']=d[4]
                if kind==4:e['invalidated_page']=d[4]
            elif kind==1:e.update(font=f'{d[0]:08X}',code=d[1],character=character(d[1]),caller=f'{d[2]:08X}')
            elif kind in (2,10,11,13,14,15):
                e.update(slot=f'{d[0]:08X}',code=d[1],character=character(d[1]),page=d[2],row=d[3],x_start=d[4],x_end=d[5],glyph=f'{d[6]:08X}',glyph_key=d[7],glyph_code=d[8],uv=struct.unpack('<4f',struct.pack('<4I',*d[10:14])))
                if kind in (13,14,15):e['font']=f'{d[14]:08X}'
            if kind==2 and d[19]:
                e.update(dirty_before=d[9],dirty_after=d[16],repair_enabled=bool(d[17]),allocation_row=d[18],allocation_row_height=d[19],source_pointer=f'{d[20]:08X}',source_size=d[21],slot_page_row_provisional=True,slot_key_provisional=True,assigned_glyph_code=d[8],assigned_character=character(d[8]))
            if kind in (16,17):
                e.update(font=f'{d[0]:08X}',page_object=f'{d[1]:08X}',slot=f'{d[2]:08X}',glyph=f'{d[3]:08X}',code=d[4],character=character(d[4]),page=d[5],y=d[6],x_start=d[7],x_end=d[8],row_height=d[9],dirty=d[10],mismatched_pixels=d[11],compared_pixels=d[12],repair_enabled=bool(d[13]))
            yield e
            index+=1

def png(path,raw):
    # Raw game pixels are little-endian BGRA. Export RGBA without altering original evidence.
    rows=[]
    for y in range(256):
        row=bytearray(b'\0')
        for x in range(256):
            b,g,r,a=raw[(y*256+x)*4:(y*256+x+1)*4];row.extend((r,g,b,a))
        rows.append(row)
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',256,256,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))

def snapshot(path,export):
    raw=path.read_bytes()
    if len(raw)!=3326700:raise ValueError(f'Invalid snapshot length: {path}')
    h=struct.unpack_from('<9I',raw)
    if h[:2]!=(0x53414657,1):raise ValueError('Unknown snapshot format')
    _,_,capture,font,tick,status,mask,count,used=h
    if count>1024 or used>0x100000:raise ValueError('Invalid snapshot bounds')
    atlas_start=36+0x2c8;glyph_start=atlas_start+8*0x40000;source_start=glyph_start+1024*176
    height=struct.unpack_from('<I',raw,36+0x1b0)[0];flags=struct.unpack_from('<I',raw,36+0x1b8)[0]
    glyphs=[]
    for i in range(count):
        start=glyph_start+i*176;slot,page,row,off,n=struct.unpack_from('<5I',raw,start)
        sl=struct.unpack_from('<16I',raw,start+20);g=struct.unpack_from('<23I',raw,start+84)
        uv=struct.unpack_from('<4f',raw,start+84+0x48)
        d={'slot':f'{slot:08X}','page':page,'row':row,'code':sl[0],'character':character(sl[0]),'glyph_code':g[6],'glyph':f'{sl[15]:08X}','glyph_key':g[0],'x_start':sl[13],'x_end':sl[14],'width':g[10],'height':g[11],'pitch':g[15],'dirty':g[9],'top_blank':g[17],'uv_vu_vu':uv,'source_offset':off,'source_size':n}
        d['slot_glyph_key_mismatch']=sl[0]!=g[0] or g[0]!=g[6]
        if n and off+n<=used and page<8 and mask&(1<<page) and not(flags&1) and g[10]<=256 and g[11]<=256 and g[15]>=g[10]:
            x0=sl[13];y0=row*height+g[17];width=g[10];bh=g[11];pitch=g[15]
            if x0+width<=256 and y0+bh<=256 and (bh==0 or (bh-1)*pitch+width<=n):
                mismatch=0
                for y in range(bh):
                    for x in range(width):
                        a=raw[source_start+off+y*pitch+x]
                        b=raw[atlas_start+page*0x40000+((y0+y)*256+x0+x)*4+3]
                        mismatch+=a!=b
                d['source_vs_cpu_alpha_mismatches']=mismatch
                d['comparison_caveat']='CPU snapshot is taken on text callback, may precede pending texture update; mismatch alone is not proof of wrong rendering.'
        glyphs.append(d)
    if export:
        for page in range(8):
            if mask&(1<<page):png(path.with_name(path.stem+f'-page{page}.png'),raw[atlas_start+page*0x40000:atlas_start+(page+1)*0x40000])
    out={'file':path.name,'capture':capture,'font':f'{font:08X}','tick_ms':tick,'status_flags':status,'valid_page_mask':mask,'row_height':height,'font_flags':flags,'source_bytes':used,'glyphs':glyphs}
    path.with_suffix('.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
    return {'file':str(path),'font':f'{font:08X}','status_flags':status,'glyph_count':count,'slot_glyph_key_mismatches':sum(g['slot_glyph_key_mismatch'] for g in glyphs)}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);ap.add_argument('--png',action='store_true');args=ap.parse_args()
    root=args.directory;summary={'note':'Diagnostic observations, not proof of root cause. Event loss is reported in the game log. Snapshots contain CPU atlases, not GPU textures.','events':{},'snapshots':[]}
    for directory in [root,*sorted(p for p in root.glob('capture-*') if p.is_dir())]:
        files=[]
        for pattern in ['critical-*.bin','events-*.bin']:
            for p in directory.glob(pattern):
                try:
                    with p.open('rb') as inp:header=inp.read(16)
                    if len(header)==16:files.append((p,struct.unpack('<4I',header)[3]))
                except OSError:pass # Current live file can be exclusively open; archived captures are preferred.
        files.sort(key=lambda item:(item[0].name.split('-')[0],item[1]))
        files=[p for p,sequence in files]
        counts=collections.Counter(); pending={}; pairs=0; mismatches=[]
        with (directory/'events.jsonl').open('w',encoding='utf-8') as out:
            for file in files:
                for e in events(file):
                    e['channel']='critical' if file.name.startswith('critical-') else 'telemetry'
                    counts[e['kind']]+=1
                    if e['kind']=='lookup_entry':pending[e['thread']]=e
                    if e['kind'].startswith('lookup_return'):
                        prior=pending.pop(e['thread'],None)
                        if prior and prior['font']==e['font']:
                            pairs+=1;e['requested_code']=prior['code'];e['requested_character']=prior['character']
                            if e['data'][0] and prior['code']!=e['code']:mismatches.append({'requested':prior['code'],'returned':e['code'],'tick':e['tick_ms'],'note':'Candidate only: dropped events or nested calls can invalidate pairing.'})
                    out.write(json.dumps(e,ensure_ascii=False,allow_nan=True)+'\n')
        summary['events'][str(directory)]={'counts':dict(counts),'paired_lookups':pairs,'candidate_key_mismatches':mismatches[:100]}
        for file in directory.glob('font-*.bin'):summary['snapshots'].append(snapshot(file,args.png))
    (root/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'event_groups':len(summary['events']),'snapshots':len(summary['snapshots']),'output':str(root/'summary.json')}))
if __name__=='__main__':main()
