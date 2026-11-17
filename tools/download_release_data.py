#!/usr/bin/env python3
"""Download and verify CardputerDict release data, then prepare a new SD directory."""
import argparse,hashlib,json,os,re,shutil,stat,time,urllib.error,urllib.request,zipfile
from pathlib import Path,PurePosixPath
REPO='RadioDisney/CardputerDict'

def sha256(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()

def download(url,target,expected=None):
    target=Path(target);target.parent.mkdir(parents=True,exist_ok=True)
    if target.exists() and (expected is None or (target.stat().st_size==expected['bytes'] and sha256(target)==expected['sha256'])):return target
    partial=target.with_suffix(target.suffix+'.part')
    last_error=None
    for attempt in range(6):
        start=partial.stat().st_size if partial.exists() else 0
        if expected and start==expected['bytes']:
            if sha256(partial)==expected['sha256']:partial.replace(target);return target
            partial.unlink();start=0
        request=urllib.request.Request(url,headers={'User-Agent':'CardputerDict-release-data/1','Range':f'bytes={start}-'} if start else {'User-Agent':'CardputerDict-release-data/1'})
        try:
            with urllib.request.urlopen(request,timeout=90) as response:
                append=start>0 and response.status==206
                if append and not response.headers.get('Content-Range','').startswith(f'bytes {start}-'):raise ValueError('Unexpected resumed range')
                with partial.open('ab' if append else 'wb') as output:
                    copied=start if append else 0;last=copied
                    while True:
                        chunk=response.read(1024*1024)
                        if not chunk:break
                        output.write(chunk);copied+=len(chunk)
                        if copied-last>=20*1024*1024:print(f'{target.name}: {copied//1048576} MiB',flush=True);last=copied
                    output.flush();os.fsync(output.fileno())
            if expected and (partial.stat().st_size!=expected['bytes'] or sha256(partial)!=expected['sha256']):raise ValueError('Release asset checksum/length mismatch: '+target.name)
            partial.replace(target);return target
        except (urllib.error.URLError,OSError) as error:
            last_error=error;time.sleep(min(2**attempt,20))
    raise RuntimeError('Download failed; partial data preserved for retry: '+target.name) from last_error

def extract(archive,destination,expected_names=None):
    destination=Path(destination)
    with zipfile.ZipFile(archive) as z:
        seen=set()
        for entry in z.infolist():
            path=PurePosixPath(entry.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in entry.filename or stat.S_ISLNK(entry.external_attr>>16):raise ValueError('Unsafe ZIP path: '+entry.filename)
            if entry.filename in seen:raise ValueError('Duplicate ZIP path')
            seen.add(entry.filename)
            if entry.is_dir():continue
            if expected_names is not None and entry.filename not in expected_names and entry.filename!='README-SD.txt':raise ValueError('Unexpected file in release archive')
            target=destination.joinpath(*path.parts)
            if target.exists():raise ValueError('Duplicate output file across archives: '+str(target))
            target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(entry) as source,target.open('xb') as output:shutil.copyfileobj(source,output,1024*1024)
        if expected_names is not None and not set(expected_names).issubset(seen):raise ValueError('Release archive is missing declared files')

def prepare(manifest,cache,output,wikipedia,base_url):
    output=Path(output);cache=Path(cache)
    if output.exists():raise ValueError('Output already exists; choose a new directory to preserve existing data')
    selected=[a for a in manifest['assets'] if a['kind']=='dictionaries' or (wikipedia and a['kind']=='wikipedia')]
    if len([a for a in selected if a['kind']=='dictionaries'])!=1 or (wikipedia and len([a for a in selected if a['kind']=='wikipedia'])!=5):raise ValueError('Incomplete release asset manifest')
    for item in selected:
        if not re.fullmatch(r'[A-Za-z0-9_.-]+\.zip',item['name']) or not re.fullmatch('[0-9a-f]{64}',item['sha256']) or item['bytes']<=0:raise ValueError('Invalid asset manifest entry')
    cache.mkdir(parents=True,exist_ok=True)
    # Keep archives on the Mac; the SD only receives the expanded directories.
    required=sum(a['bytes'] for a in selected)+ (manifest['unpacked_bytes'] if wikipedia else 70*1024*1024)
    if shutil.disk_usage(cache).free<required:raise ValueError('Insufficient Mac disk space for archives and extracted data')
    for item in selected:
        download(base_url+'/'+item['name'],cache/item['name'],item);print(item['name']+': SHA-256 verified',flush=True)
    staging=output.with_name(output.name+'.extracting')
    if staging.exists():raise ValueError('Previous extraction directory exists; preserve or rename it before retrying')
    staging.mkdir(parents=True)
    for item in selected:extract(cache/item['name'],staging,item['files'])
    if wikipedia:
        from install_wikipedia import plan
        info,_,_=plan(staging/'wikipedia')
        if info['source']['partial'] or not info['source']['verified']:raise ValueError('Expected complete verified encyclopedia')
    staging.rename(output)
    print('SD directory ready: '+str(output.resolve()),flush=True)
    print('Copy its dictionaries/, licenses/, audio/ and (if selected) wikipedia/ to the FAT32 SD root. Eject safely; power off Cardputer before inserting.')
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--release',default='v0.6.4');parser.add_argument('--wikipedia',action='store_true');parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cache',type=Path,default=Path('build/release-data-downloads'))
    args=parser.parse_args()
    if not re.fullmatch(r'v\d+\.\d+\.\d+',args.release):parser.error('Use a release tag such as v0.6.4')
    url=f'https://github.com/{REPO}/releases/download/{args.release}';manifest_file=args.cache/(args.release+'-data-assets.json')
    download(url+'/data-assets.json',manifest_file)
    manifest=json.loads(manifest_file.read_text());prepare(manifest,args.cache,args.output,args.wikipedia,url)
if __name__=='__main__':main()
