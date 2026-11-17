#!/usr/bin/env python3
"""Convert a complete Wikimedia pages-articles XML(.bz2) dump into a Cardputer SD pack."""
import argparse,bz2,hashlib,html,io,json,re,time,xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import quote
from wiki_pack import PackWriter

def language_variant(text):
    def choose(match):
        content=match[1]
        if '|' in content and re.match(r'^[A-Za-z-]+\|',content):content=content.split('|',1)[1]
        pairs={}
        for part in content.split(';'):
            if ':' in part:
                name,value=part.split(':',1);pairs[name.strip()]=value.strip()
        return next((pairs[k] for k in ['zh-hans','zh-cn','zh-sg','zh'] if k in pairs),next(iter(pairs.values()),content))
    return re.sub(r'-\{(.*?)\}-',choose,text,flags=re.S)

class Cleaner:
    def __init__(self):
        import mwparserfromhell
        from opencc import OpenCC
        self.parser=mwparserfromhell;self.simplify=OpenCC('t2s');self.traditional=OpenCC('s2t')
    def aliases(self,title):return [self.simplify.convert(title),self.traditional.convert(title)]
    def plain(self,raw):
        p=self.parser
        def nodes(code):return ''.join(node(n) for n in code.nodes)
        def node(n):
            if isinstance(n,p.nodes.Text):return str(n)
            if isinstance(n,p.nodes.Comment):return ''
            if isinstance(n,p.nodes.HTMLEntity):return n.normalize()
            if isinstance(n,p.nodes.Heading):return '\n\n'+nodes(n.title).strip()+'\n'
            if isinstance(n,p.nodes.Wikilink):
                title=str(n.title).strip()
                if re.match(r'^:?(?:File|Image|Category|文件|檔案|图像|圖像|分类|分類)\s*:',title,re.I):return ''
                return nodes(n.text if n.text is not None else n.title)
            if isinstance(n,p.nodes.ExternalLink):return nodes(n.title) if n.title is not None else str(n.url)
            if isinstance(n,p.nodes.Argument):return nodes(n.default) if n.default is not None else ''
            if isinstance(n,p.nodes.Tag):
                tag=str(n.tag).strip().lower()
                if tag in ['ref','references','gallery','imagemap','timeline','score','style','script','templatestyles']:return ''
                inner=nodes(n.contents) if n.contents is not None else ''
                if tag=='br':return '\n'
                if tag in ['td','th']:return inner+' | '
                return ('\n'+inner+'\n') if tag in ['p','div','li','tr','table','blockquote','poem'] else inner
            if isinstance(n,p.nodes.Template):
                name=str(n.name).strip().lower().replace('_',' ')
                if name.startswith(('cite','citation','navbox','authority','reflist','coord','redirect','otheruse','about','noteta')) or name in ['sfn','harv','harvnb','notelist','refn','multiple issues']:return ''
                params=[x for x in n.params if str(x.value).strip()]
                if name in ['lang','langx'] and len(params)>1:return nodes(params[1].value)
                if name.startswith('lang-') or name in ['nowrap','small','big','center','color','font']:
                    return nodes(params[-1].value) if params else ''
                info='infobox' in name or '信息框' in name or '資訊框' in name
                values=[]
                for x in params:
                    k=str(x.name).strip()
                    if not info and not k.isdigit():continue
                    if re.search(r'image|logo|flag|map|style|class|align|width|图片|图像|图徽|图标|地圖|圖像',k,re.I):continue
                    value=nodes(x.value).strip()
                    if not value or re.search(r'\.(?:svg|png|jpe?g|gif|webp)(?:\||$)',value,re.I):continue
                    values.append((k+'：' if info and not k.isdigit() else '')+value)
                return ('\n' if info else ' ').join(values)+ ('\n' if info and values else '')
            return str(n)
        result=nodes(p.parse(language_variant(raw)))
        result=html.unescape(result).replace('\0','').replace('\r','')
        result=re.sub(r'(?m)^[*#;:]+[ \t]*','• ',result)
        result=re.sub(r'[ \t]+',' ',result)
        result=re.sub(r'\n[ \t]+','\n',result)
        result=re.sub(r'\n{3,}','\n\n',result).strip()
        return self.simplify.convert(result)

class HashedInput(io.RawIOBase):
    def __init__(self,path,expected_size=0,follow=False,wait=3600):
        self.path=Path(path);self.file=self.path.open('rb');self.expected=expected_size;self.follow=follow;self.wait=wait;self.count=0;self.sha1=hashlib.sha1()
    def readable(self):return True
    def seekable(self):return False
    def read(self,n=-1):
        waited=time.monotonic()
        while True:
            data=self.file.read(n)
            if data:
                self.count+=len(data);self.sha1.update(data);return data
            if not self.follow or self.count>=self.expected:return b''
            if time.monotonic()-waited>self.wait:raise TimeoutError('Download made no progress within the configured follow wait')
            time.sleep(.25)
    def close(self):
        self.file.close();super().close()

_WORKER_CLEANER=None

def init_worker():
    global _WORKER_CLEANER
    _WORKER_CLEANER=Cleaner()

def clean_record(record,cleaner=None):
    cleaner=cleaner or _WORKER_CLEANER
    pid,title,target,text,rid=record
    aliases=cleaner.aliases(title)
    if target:return pid,title,target,'',aliases,False
    body=cleaner.plain(text);empty=not bool(body)
    if empty:body='此页面没有可显示的纯文字正文。'
    canonical=cleaner.simplify.convert(title)
    url='https://zh.wikipedia.org/w/index.php?title='+quote(title.replace(' ','_'))+'&oldid='+rid
    footer='\n\n来源：中文维基百科\n'+url+'\n历史与作者：'+url+'&action=history\n许可：CC BY-SA 4.0\n纯文字整理，已转换简体。'
    return pid,title,None,canonical+'\n\n'+body+footer,aliases,empty

def converted_records(records,workers):
    if workers==1:
        cleaner=Cleaner()
        for record in records:yield clean_record(record,cleaner)
        return
    from concurrent.futures import ProcessPoolExecutor
    from collections import deque
    pending=deque()
    with ProcessPoolExecutor(max_workers=workers,initializer=init_worker) as pool:
        for record in records:
            pending.append(pool.submit(clean_record,record))
            if len(pending)>=workers*4:yield pending.popleft().result()
        while pending:yield pending.popleft().result()

def convert(args):
    identity={'input':Path(args.input).name,'sha1':args.sha1,'expected_bytes':args.expected_bytes,'chunk_mib':args.chunk_mib,'policy':'plain-simplified-v1'}
    identity_path=Path(args.output)/'build-source.json'
    if getattr(args,'resume',False):
        if not args.sha1 or not identity_path.exists() or json.loads(identity_path.read_text())!=identity:
            raise ValueError('Resume source identity differs or is unavailable; use a new output directory')
    writer=PackWriter(args.output,args.chunk_mib*1024*1024,getattr(args,'resume',False))
    if not getattr(args,'resume',False):identity_path.write_text(json.dumps(identity,indent=2)+'\n')
    raw=HashedInput(args.input,args.expected_bytes,args.follow,getattr(args,'follow_wait',3600))
    stream=bz2.BZ2File(raw,'rb') if str(args.input).endswith('.bz2') else raw
    count=writer.articles;empty_pages=0;last=time.monotonic();limited=False
    def records():
        context=ET.iterparse(stream,events=('start','end'));_,root=next(context)
        namespace=root.tag[:root.tag.index('}')+1] if '}' in root.tag else ''
        for event,element in context:
            if event!='end' or element.tag!=namespace+'page':continue
            record=None
            if element.findtext(namespace+'ns')=='0':
                title=element.findtext(namespace+'title','');pid=int(element.findtext(namespace+'id','0'))
                if not getattr(args,'resume',False) or not writer.contains(pid):
                    redirect=element.find(namespace+'redirect');target=redirect.get('title') if redirect is not None else None
                    revision=element.find(namespace+'revision')
                    text=revision.findtext(namespace+'text','') if revision is not None else ''
                    rid=revision.findtext(namespace+'id','0') if revision is not None else '0'
                    record=(pid,title,target,text,rid)
            element.clear();root.clear()
            if record is not None:yield record
    workers=getattr(args,'workers',1)
    results=converted_records(records(),workers)
    for pid,title,target,body,aliases,empty in results:
        accepted=writer.add(pid,title,body,redirect=target,aliases=aliases)
        if accepted and not target:count+=1;empty_pages+=int(empty)
        if args.limit and count>=args.limit:limited=True;break
        if time.monotonic()-last>=5:
            print(json.dumps({'articles':count,'redirects':writer.redirects,'compressed_read':raw.count,'text_bytes':writer.position,'workers':workers}),flush=True);last=time.monotonic()
    if limited:results.close()
    if not limited:
        for _ in iter(lambda:stream.read(1024*1024),b''):pass
        if args.expected_bytes and raw.count!=args.expected_bytes:raise ValueError('Compressed source size mismatch')
        if args.sha1 and raw.sha1.hexdigest()!=args.sha1:raise ValueError('Compressed source SHA-1 mismatch')
    source={'file':Path(args.input).name,'url':args.source_url,'sha1':raw.sha1.hexdigest(),'verified':bool(args.sha1 and not limited),'partial':limited,
      'empty_text_pages_in_this_run':empty_pages,'resumed':getattr(args,'resume',False)}
    result=writer.finish(source,lambda value:print(value,flush=True))
    identity_path.unlink(missing_ok=True)
    print(json.dumps(result,ensure_ascii=False),flush=True)
    stream.close();raw.close()
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--workers',type=int,default=1);p.add_argument('--resume',action='store_true');p.add_argument('--follow-wait',type=int,default=3600)
    p.add_argument('--sha1');p.add_argument('--expected-bytes',type=int,default=0);p.add_argument('--follow',action='store_true')
    p.add_argument('--source-url',default='');p.add_argument('--chunk-mib',type=int,default=128);p.add_argument('--limit',type=int,default=0)
    args=p.parse_args()
    if not 1<=args.workers<=8:p.error('--workers must be 1..8')
    if args.follow and args.expected_bytes<=0:p.error('--follow requires --expected-bytes')
    if args.limit and args.sha1:p.error('Partial packs cannot claim complete-source checksum verification')
    convert(args)
if __name__=='__main__':main()
