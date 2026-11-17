"""Cardputer Wikipedia pack writer. Fixed disk index, 64-bit text offsets, bounded chunks."""
from pathlib import Path
import hashlib,json,sqlite3,struct,zlib

MAGIC=b'CPWIKI1\0'
VERSION=1
RECORD=struct.Struct('<IHHQI')
HEADER=struct.Struct('<8sIIIIQII24x')
DEFAULT_CHUNK=128*1024*1024
MAX_TITLE=768

def key(word):
    return bytes(c+32 if 65<=c<=90 else c for c in word.encode('utf-8'))

class PackWriter:
    def __init__(self, destination, chunk_bytes=DEFAULT_CHUNK, resume=False):
        self.root=Path(destination)
        if self.root.exists() and any(self.root.iterdir()) and not resume:raise ValueError('Output must be new or empty')
        if resume and ((self.root/'wiki.wki').exists() or not (self.root/'build.sqlite3').exists()):raise ValueError('Resume requires an unfinished conversion catalog')
        if not 4096<=chunk_bytes<=DEFAULT_CHUNK:raise ValueError('Invalid text chunk size')
        self.root.mkdir(parents=True,exist_ok=True)
        self.chunk_bytes=chunk_bytes;self.position=0;self.part=None;self.hash=None;self.parts=[]
        self.db=sqlite3.connect(self.root/'build.sqlite3')
        self.db.executescript('PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA temp_store=FILE;')
        if not resume:
            self.db.executescript('''CREATE TABLE pages(id INTEGER PRIMARY KEY,title TEXT UNIQUE,redirect TEXT,off INTEGER,len INTEGER);
              CREATE TABLE aliases(word BLOB PRIMARY KEY,sort BLOB,page INTEGER,rank INTEGER) WITHOUT ROWID;
              CREATE TABLE resolved(id INTEGER PRIMARY KEY,off INTEGER,len INTEGER);''')
        else:
            if self.db.execute('PRAGMA quick_check').fetchone()[0]!='ok':raise ValueError('Resume catalog is corrupt')
            self.db.execute('DELETE FROM resolved')
            actual=0
            for part in sorted(self.root.glob('text[0-9][0-9][0-9][0-9][0-9].bin')):
                if int(part.stem[4:])*chunk_bytes!=actual:raise ValueError('Noncontiguous resume text chunks')
                actual+=part.stat().st_size
            self.db.execute('DELETE FROM pages WHERE off IS NOT NULL AND off+len>?',(actual,))
            self.db.execute('DELETE FROM aliases WHERE page NOT IN (SELECT id FROM pages)')
            self.position=self.db.execute('SELECT COALESCE(MAX(off+len),0) FROM pages WHERE off IS NOT NULL').fetchone()[0]
            for part in sorted(self.root.glob('text[0-9][0-9][0-9][0-9][0-9].bin')):
                number=int(part.stem[4:]);keep=max(0,min(chunk_bytes,self.position-number*chunk_bytes))
                if not keep:part.unlink();continue
                with part.open('r+b') as f:f.truncate(keep)
                digest=hashlib.sha256()
                with part.open('rb') as f:
                    for block in iter(lambda:f.read(1024*1024),b''):digest.update(block)
                if keep==chunk_bytes:self.parts.append({'file':part.name,'bytes':keep,'sha256':digest.hexdigest()})
                else:self.part=part.open('ab',buffering=1024*1024);self.hash=digest;self.part_bytes=keep
            self.db.commit()
        self.articles=self.db.execute('SELECT COUNT(*) FROM pages WHERE off IS NOT NULL').fetchone()[0]
        self.redirects=self.db.execute('SELECT COUNT(*) FROM pages WHERE redirect IS NOT NULL').fetchone()[0]
        self.unresolved=0
    def contains(self,page_id):
        return self.db.execute('SELECT 1 FROM pages WHERE id=?',(page_id,)).fetchone() is not None
    def _finish_part(self):
        if self.part:
            self.part.flush();self.part.close()
            self.parts.append({'file':f'text{len(self.parts):05d}.bin','bytes':self.part_bytes,'sha256':self.hash.hexdigest()})
            self.part=None
    def _text(self, data):
        start=self.position
        while data:
            if self.part is None:
                self.part=(self.root/f'text{len(self.parts):05d}.bin').open('wb',buffering=1024*1024)
                self.hash=hashlib.sha256();self.part_bytes=0
            piece=data[:self.chunk_bytes-self.part_bytes]
            self.part.write(piece);self.hash.update(piece);self.part_bytes+=len(piece);self.position+=len(piece);data=data[len(piece):]
            if self.part_bytes==self.chunk_bytes:self._finish_part()
        return start
    def add(self,page_id,title,text,redirect=None,aliases=()):
        if not title or '\0' in title or len(title.encode('utf-8'))>MAX_TITLE:return False
        if redirect:
            off=None;length=0;self.redirects+=1
        else:
            data=text.encode('utf-8')
            if not data or b'\0' in data or len(data)>0xffffffff:return False
            off=self._text(data);length=len(data);self.articles+=1
        self.db.execute('INSERT INTO pages VALUES(?,?,?,?,?)',(page_id,title,redirect,off,length))
        for rank,word in [(0,title)]+[(1,a) for a in aliases]:
            encoded=word.encode('utf-8')
            if not encoded or len(encoded)>MAX_TITLE or b'\0' in encoded:continue
            self.db.execute('''INSERT INTO aliases VALUES(?,?,?,?) ON CONFLICT(word) DO UPDATE SET
             sort=excluded.sort,page=excluded.page,rank=excluded.rank WHERE excluded.rank<aliases.rank''',
             (encoded,key(word),page_id,rank))
        if (self.articles+self.redirects)%2000==0:self.db.commit()
        return True
    def finish(self,source,progress=lambda value:None):
        self._finish_part();self.db.commit()
        if not self.articles:raise ValueError('No articles in input')
        self.db.execute('INSERT INTO resolved SELECT id,off,len FROM pages WHERE off IS NOT NULL')
        # Resolve chains against original titles; skip cycles/missing targets explicitly.
        cache={}
        for number,(pid,target) in enumerate(self.db.execute('SELECT id,redirect FROM pages WHERE redirect IS NOT NULL')):
            seen={pid};trail=[];found=None
            for _ in range(128):
                target=target.split('#',1)[0].replace('_',' ').strip()
                if target in cache:found=cache[target];break
                row=self.db.execute('SELECT id,redirect,off,len FROM pages WHERE title=?',(target,)).fetchone()
                if not row or row[0] in seen:break
                seen.add(row[0]);trail.append(target)
                if row[2] is not None:found=(row[2],row[3]);break
                target=row[1]
            if len(cache)>65536:cache.clear()
            for title in trail:cache[title]=found
            if found:self.db.execute('INSERT INTO resolved VALUES(?,?,?)',(pid,*found))
            else:self.unresolved+=1
            if number%20000==0:progress(f'redirects resolved={number} unresolved={self.unresolved}')
        self.db.commit()
        count=0;title_offset=0
        with (self.root/'index.wix').open('wb',buffering=1024*1024) as index,(self.root/'titles.wit').open('wb',buffering=1024*1024) as titles:
            for word,off,length in self.db.execute('''SELECT a.word,r.off,r.len FROM aliases a JOIN resolved r ON a.page=r.id ORDER BY a.sort,a.word'''):
                if title_offset+len(word)>0xffffffff or (count+1)*RECORD.size>0xffffffff:raise ValueError('Index format capacity exceeded')
                titles.write(word);index.write(RECORD.pack(title_offset,len(word),0,off,length))
                title_offset+=len(word);count+=1
        files=list(self.parts)
        for name in ['index.wix','titles.wit']:
            p=self.root/name;digest=hashlib.sha256()
            with p.open('rb') as f:
                for block in iter(lambda:f.read(1024*1024),b''):digest.update(block)
            files.append({'file':name,'bytes':p.stat().st_size,'sha256':digest.hexdigest()})
        header=HEADER.pack(MAGIC,VERSION,count,title_offset,self.chunk_bytes,self.position,RECORD.size,0)
        header=header[:36]+struct.pack('<I',zlib.crc32(header[:36]))+header[40:]
        assert len(header)==64
        info={'format':'CardputerWiki/1','articles':self.articles,'redirect_pages':self.redirects,'unresolved_redirects':self.unresolved,
          'search_entries':count,'title_bytes':title_offset,'text_bytes':self.position,'chunk_bytes':self.chunk_bytes,
          'files':files,'source':source,'license':'CC-BY-SA-4.0','license_url':'https://creativecommons.org/licenses/by-sa/4.0/',
          'changes':'Unillustrated plain text derived from namespace-0 Wikipedia articles; formatting normalized and Chinese converted to simplified; original, simplified and traditional title aliases indexed.'}
        (self.root/'manifest.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n')
        (self.root/'LICENSE.txt').write_text('Wikipedia text contributors. CC BY-SA 4.0.\nhttps://creativecommons.org/licenses/by-sa/4.0/\nhttps://zh.wikipedia.org/wiki/Wikipedia:版权信息\nEach article retains its source revision URL and history attribution link.\nModified: unillustrated plain text; formatting normalized; simplified Chinese conversion.\n')
        if source.get('partial'):
            (self.root/'SAMPLE.txt').write_text('测试子集，仅用于验证设备阅读；完整百科请安装完整数据包。\n')
        self.db.close();(self.root/'build.sqlite3').unlink()
        # Publish the discovery header last: interrupted conversions are not mountable.
        (self.root/'wiki.wki').write_bytes(header)
        return info
