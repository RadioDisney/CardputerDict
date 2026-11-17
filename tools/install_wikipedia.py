#!/usr/bin/env python3
"""Verify/install Cardputer Wikipedia packs. Prefer a card reader for full packs.

  python3 tools/install_wikipedia.py /path/wikipedia --sd-root /Volumes/CARD
  python3 tools/install_wikipedia.py /path/sample/wikipedia --port /dev/cu.usbmodemXXXX
"""
import argparse,hashlib,json,os,re,shutil,struct,sys,tempfile,time,zlib
from pathlib import Path
from wiki_pack import HEADER,RECORD,MAGIC

def safe_name(name):
    return name in ('wiki.wki','index.wix','titles.wit','manifest.json','LICENSE.txt','SAMPLE.txt') or bool(re.fullmatch(r'text\d{5}\.bin',name))
def plan(root):
    root=Path(root);info=json.loads((root/'manifest.json').read_text())
    header=(root/'wiki.wki').read_bytes()
    if len(header)!=64:raise ValueError('Invalid Wikipedia header length')
    magic,version,count,titles,chunk,text,record,crc=HEADER.unpack(header)
    if magic!=MAGIC or version!=1 or record!=RECORD.size or crc!=zlib.crc32(header[:36]) or any(header[40:]) or not count or count>0xffffffff//RECORD.size or not titles or not text or not 4096<=chunk<=128*1024*1024 or (text-1)//chunk>=100000:raise ValueError('Invalid Wikipedia header')
    if info['search_entries']!=count or info['text_bytes']!=text:raise ValueError('Manifest/header mismatch')
    files={}
    for item in info['files']:
        name=item['file']
        if not safe_name(name) or name in files:raise ValueError('Unsafe or duplicate pack filename')
        if not re.fullmatch('[0-9a-f]{64}',item['sha256']):raise ValueError('Invalid checksum')
        if (root/name).stat().st_size!=item['bytes']:raise ValueError('Truncated pack file: '+name)
        files[name]=item
    if files.get('index.wix',{}).get('bytes')!=count*RECORD.size or files.get('titles.wit',{}).get('bytes')!=titles:raise ValueError('Index/title length mismatch')
    for number in range((text+chunk-1)//chunk):
        if files.get(f'text{number:05d}.bin',{}).get('bytes')!=min(chunk,text-number*chunk):raise ValueError('Missing or truncated text chunk')
    expected={'index.wix','titles.wit'}|{f'text{n:05d}.bin' for n in range((text+chunk-1)//chunk)}
    if set(files)!=expected:raise ValueError('Unexpected files in data manifest')
    if bool(info.get('source',{}).get('partial'))!=(root/'SAMPLE.txt').exists():raise ValueError('Sample marker/metadata mismatch; replace the complete folder instead of merging files')
    names=[*sorted(files), 'LICENSE.txt','manifest.json']
    if (root/'SAMPLE.txt').exists():names.append('SAMPLE.txt')
    names.append('wiki.wki') # Publish discovery header last.
    return info,files,names

def digest(path,nocache=False):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        if nocache and sys.platform=='darwin':
            import fcntl
            fcntl.fcntl(f.fileno(),fcntl.F_NOCACHE,1)
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def copy_to_card(root,sdroot,progress=print):
    root=Path(root);sdroot=Path(sdroot)
    if not sdroot.is_dir():raise ValueError('SD mount directory does not exist')
    info,files,names=plan(root)
    needed=sum((root/name).stat().st_size for name in names)
    if shutil.disk_usage(sdroot).free<needed+1024*1024:raise ValueError('Insufficient card space for a verified staged install')
    staging=Path(tempfile.mkdtemp(prefix='.wikipedia-install-',dir=sdroot))
    for name in names:
        source=root/name;destination=staging/name
        with source.open('rb') as a,destination.open('wb') as b:
            shutil.copyfileobj(a,b,1024*1024);b.flush();os.fsync(b.fileno())
        actual=digest(destination,nocache=True)
        if actual!=digest(source) or (name in files and actual!=files[name]['sha256']):raise ValueError('Checksum mismatch; old installation preserved: '+name)
        progress(f'{name}: verified')
    target=sdroot/'wikipedia';backup=None
    if target.exists():
        backup=sdroot/('wikipedia-backup-'+time.strftime('%Y%m%d-%H%M%S'))
        if backup.exists():raise ValueError('Backup path already exists; staged files preserved')
        target.rename(backup)
    try:staging.rename(target)
    except Exception:
        if backup and not target.exists():backup.rename(target)
        raise
    progress(f'Installed {info["articles"]} articles; '+(f'previous data kept at {backup}' if backup else 'new card directory'))
    return target

def fnv_file(path):
    state=2166136261
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(65536),b''):
            for byte in block:state=((state^byte)*16777619)&0xffffffff
    return state

def upload(root,portname,log=None,replace_sample=False):
    from serial_link import open_device
    from device_check import command
    from upload_sd import line
    root=Path(root);info,files,names=plan(root)
    if sum((root/name).stat().st_size for name in names)>64*1024*1024:raise ValueError('Use --sd-root and a card reader for full packs over 64MiB')
    result=[]
    with open_device(portname,timeout=.1) as port:
        status=command(port,'status');assert 'CardputerDict 0.6.' in status,status
        command(port,'audio stop');command(port,'lookup apple')
        if replace_sample:
            backup=command(port,'wiki-backup-sample')
            if 'WIKI_SAMPLE_BACKUP ' not in backup:raise RuntimeError(backup)
            print(backup,flush=True)
        for name in names:
            path=root/name;actual=digest(path)
            if name in files and actual!=files[name]['sha256']:raise ValueError('Source checksum mismatch: '+name)
            size=path.stat().st_size;port.reset_input_buffer();port.write(f'putwiki {name} {size} {fnv_file(path):08x}\n'.encode())
            reply=line(port)
            while not reply.startswith(('READY','SKIP','ERR')):reply=line(port)
            if reply.startswith('ERR'):raise RuntimeError(name+': '+reply)
            if reply.startswith('READY'):
                sent=0
                with path.open('rb') as source:
                    for block in iter(lambda:source.read(4096),b''):
                        port.write(block);target=sent+len(block);reply=line(port,timeout=30)
                        while reply.startswith('ACK ') and int(reply[4:])<=sent:reply=line(port,timeout=30)
                        if reply!=f'ACK {target}':raise RuntimeError(name+': '+reply)
                        sent=target
                reply=line(port,timeout=120)
                if reply!='DONE verified':raise RuntimeError(name+': '+reply)
            result.append({'file':name,'bytes':size,'result':reply});print(name,reply,flush=True)
        command(port,'reload',timeout=180)
        command(port,'wiki')
        final=command(port,'status');assert f'entries={info["search_entries"]} ' in final,final
    if log:Path(log).write_text(json.dumps({'files':result,'status':final},ensure_ascii=False,indent=2)+'\n')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('pack',type=Path)
    group=parser.add_mutually_exclusive_group(required=True);group.add_argument('--sd-root',type=Path);group.add_argument('--port')
    parser.add_argument('--log',type=Path);parser.add_argument('--replace-sample',action='store_true');args=parser.parse_args()
    if args.port:upload(args.pack,args.port,args.log,args.replace_sample)
    else:copy_to_card(args.pack,args.sd_root)
if __name__=='__main__':main()
