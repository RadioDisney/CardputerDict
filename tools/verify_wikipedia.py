#!/usr/bin/env python3
"""Check pack checksums, every title/index record and real 64-bit/cross-chunk reads."""
import argparse,json
from pathlib import Path
from install_wikipedia import plan,digest
from wiki_pack import RECORD,key,MAX_TITLE

def read_body(root,info,offset,length):
    data=bytearray();chunk=info['chunk_bytes']
    while length:
        part,local=divmod(offset,chunk)
        with (root/f'text{part:05d}.bin').open('rb') as f:f.seek(local);piece=f.read(min(length,chunk-local))
        if not piece:raise ValueError('Truncated article')
        data.extend(piece);offset+=len(piece);length-=len(piece)
    return data.decode('utf-8')

def verify(root,require_complete=False,quick=False,progress=print):
    root=Path(root);info,files,names=plan(root)
    source=info.get('source',{})
    if require_complete and (source.get('partial') or not source.get('verified')):raise ValueError('Pack is not a verified complete snapshot')
    if not quick:
        for name,item in files.items():
            if digest(root/name)!=item['sha256']:raise ValueError('Pack checksum mismatch: '+name)
            progress(name+': checksum verified')
    probes={};wanted={'苹果','蘋果','中国','中國','计算机','計算機','数学','维基百科','Linux'}
    previous=None;seen=0;count=0
    with (root/'index.wix').open('rb',buffering=1024*1024) as index,(root/'titles.wit').open('rb',buffering=1024*1024) as titles:
        while True:
            block=index.read(RECORD.size*32768)
            if not block:break
            if len(block)%RECORD.size:raise ValueError('Truncated index record')
            for at,length,flags,offset,size in RECORD.iter_unpack(block):
                if at!=seen or not 0<length<=MAX_TITLE or flags or not size or offset>info['text_bytes'] or size>info['text_bytes']-offset:raise ValueError('Invalid index record')
                raw=titles.read(length)
                if len(raw)!=length or b'\0' in raw:raise ValueError('Invalid title record')
                word=raw.decode('utf-8');order=(key(word),raw)
                if previous is not None and previous>=order:raise ValueError('Index is not uniquely sorted')
                previous=order;seen+=length;count+=1
                if word in wanted:probes[word]=(offset,size)
                if offset>=2**32 and 'offset_over_4GiB' not in probes:probes['offset_over_4GiB']=(offset,size)
                if offset//info['chunk_bytes']!=(offset+size-1)//info['chunk_bytes'] and 'cross_chunk' not in probes:probes['cross_chunk']=(offset,size)
            if quick:break
    if not quick and (count!=info['search_entries'] or seen!=info['title_bytes']):raise ValueError('Index counts disagree with header')
    probe_results={}
    for name,(offset,size) in probes.items():
        text=read_body(root,info,offset,size)
        if not text or '\0' in text or 'CC BY-SA 4.0' not in text:raise ValueError('Article/attribution missing: '+name)
        probe_results[name]={'offset':offset,'bytes':size,'first_line':text.splitlines()[0],'last_line':text.splitlines()[-1]}
    result={'passed':True,'complete':not source.get('partial',True),'articles':info['articles'],'search_entries':info['search_entries'],
      'text_bytes':info['text_bytes'],'index_records_checked':count,'probes':probe_results}
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('pack',type=Path);p.add_argument('--require-complete',action='store_true');p.add_argument('--quick',action='store_true');p.add_argument('--log',type=Path)
    args=p.parse_args();result=verify(args.pack,args.require_complete,args.quick)
    if args.log:args.log.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
