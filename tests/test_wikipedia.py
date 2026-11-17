import argparse,contextlib,hashlib,importlib.util,io,json,struct,sys,tempfile,unittest,zlib
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from wiki_pack import PackWriter,HEADER,RECORD

def entries(root):
    titles=(root/'titles.wit').read_bytes();index=(root/'index.wix').read_bytes();result={}
    for at in range(0,len(index),RECORD.size):
        offset,size,flags,body,length=RECORD.unpack_from(index,at)
        result[titles[offset:offset+size].decode('utf-8')]=(body,length)
    return result

def body(root,location):
    _,_,_,_,chunk,_,_,_=HEADER.unpack((root/'wiki.wki').read_bytes())
    offset,length=location;data=bytearray()
    while length:
        part,local=divmod(offset,chunk);piece=(root/f'text{part:05d}.bin').read_bytes()[local:local+length]
        data.extend(piece);offset+=len(piece);length-=len(piece)
    return data.decode('utf-8')

class WikiPackTests(unittest.TestCase):
    def test_chunk_boundaries_alias_priority_redirects_and_integrity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'wikipedia';writer=PackWriter(root,4096)
            article='蘋果\n\n'+'正文段落。'*1800+'文章最后一句。'
            writer.add(1,'蘋果',article,aliases=['苹果'])
            writer.add(2,'果实','另一篇完整正文')
            writer.add(3,'苹果树','',redirect='蘋果')
            writer.add(4,'果树','',redirect='苹果树')
            writer.add(5,'循环甲','',redirect='循环乙');writer.add(6,'循环乙','',redirect='循环甲')
            writer.add(7,'失效跳转','',redirect='不存在')
            writer.add(8,'干','简体本义',aliases=['幹']);writer.add(9,'幹','繁体本义',aliases=['干'])
            info=writer.finish({'test':True})
            self.assertEqual(info['articles'],4);self.assertEqual(info['unresolved_redirects'],3)
            values=entries(root)
            for word in ['蘋果','苹果','苹果树','果树']:self.assertEqual(body(root,values[word]),article)
            self.assertEqual(body(root,values['干']),'简体本义');self.assertEqual(body(root,values['幹']),'繁体本义')
            self.assertNotIn('循环甲',values);self.assertNotIn('失效跳转',values)
            header=(root/'wiki.wki').read_bytes();self.assertEqual(len(header),64)
            self.assertEqual(struct.unpack_from('<I',header,36)[0],zlib.crc32(header[:36]))
            for record in info['files']:
                data=(root/record['file']).read_bytes()
                self.assertEqual(len(data),record['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),record['sha256'])
                if record['file'].startswith('text'):self.assertLessEqual(len(data),4096)
            self.assertTrue((root/'LICENSE.txt').exists());self.assertFalse((root/'build.sqlite3').exists())
    def test_failed_or_empty_build_does_not_publish_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'wikipedia';writer=PackWriter(root)
            self.assertFalse(writer.add(1,'bad\0title','text'))
            with self.assertRaises(ValueError):writer.finish({})
            self.assertFalse((root/'wiki.wki').exists())
            with self.assertRaises(ValueError):PackWriter(root)

HAS_PARSERS=importlib.util.find_spec('mwparserfromhell') and importlib.util.find_spec('opencc')
@unittest.skipUnless(HAS_PARSERS,'Install tools/requirements-wiki.txt to test XML and wikitext conversion')
class WikiConversionTests(unittest.TestCase):
    def test_cleaner_keeps_body_tables_and_variants_without_control_markup(self):
        from prepare_wikipedia import Cleaner
        source="{{Refimprove|time=2020}}{{NoteTA|G1=Science|1=zh-cn:对象;zh-tw:物件;}}'''正文'''[[链接|标签]]{{lang|en|Apple}}<ref>隐藏引用</ref>[[File:图.png]]\n==章节==\n-{zh-hans:计算机;zh-hant:計算機;}-\n{|\n!表头\n|-\n|数据\n|}"
        result=Cleaner().plain(source)
        for text in ['正文','标签','Apple','章节','计算机','表头','数据']:self.assertIn(text,result)
        for text in ['Refimprove','Science','2020','隐藏引用','File:','{{','[[','-{']:self.assertNotIn(text,result)
    def test_full_xml_namespace_filter_long_body_and_empty_articles(self):
        from prepare_wikipedia import convert
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);xml=ET.Element('mediawiki',xmlns='http://www.mediawiki.org/xml/export-0.11/')
            for pid,title,namespace,text,redirect in [(1,'蘋果',0,'果实正文。',None),(2,'苹果',0,'#REDIRECT [[蘋果]]','蘋果'),(3,'讨论页',1,'不应收录',None),(4,'空页',0,'{{纯模板}}',None),(5,'长文',0,'第一段\n'+'保留完整正文。'*4000+'最后一句',None)]:
                page=ET.SubElement(xml,'page');ET.SubElement(page,'title').text=title;ET.SubElement(page,'ns').text=str(namespace);ET.SubElement(page,'id').text=str(pid)
                if redirect:ET.SubElement(page,'redirect',title=redirect)
                revision=ET.SubElement(page,'revision');ET.SubElement(revision,'id').text=str(pid*10);ET.SubElement(revision,'text').text=text
            data=ET.tostring(xml,encoding='utf-8');source=root/'dump.xml';source.write_bytes(data)
            args=argparse.Namespace(input=source,output=root/'wikipedia',chunk_mib=1,expected_bytes=len(data),follow=False,limit=0,sha1=hashlib.sha1(data).hexdigest(),source_url='fixture')
            with contextlib.redirect_stdout(io.StringIO()):info=convert(args)
            self.assertEqual(info['articles'],3);self.assertTrue(info['source']['verified']);self.assertEqual(info['source']['empty_text_pages_in_this_run'],1)
            words=entries(args.output);self.assertNotIn('讨论页',words)
            self.assertIn('最后一句',body(args.output,words['长文']));self.assertGreater(words['长文'][1],12288)
            self.assertIn('oldid=10',body(args.output,words['苹果']));self.assertIn('CC BY-SA 4.0',body(args.output,words['苹果']))
            self.assertIn('没有可显示',body(args.output,words['空页']))
            first=args.output;args.output=root/'parallel';args.workers=2
            with contextlib.redirect_stdout(io.StringIO()):parallel=convert(args)
            self.assertEqual(parallel['files'],info['files'])
            self.assertEqual((first/'wiki.wki').read_bytes(),(args.output/'wiki.wki').read_bytes())

class WikiInstallTests(unittest.TestCase):
    def test_usb_fnv_checksum_matches_standard_vector(self):
        from install_wikipedia import fnv_file
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'value';p.write_bytes(b'hello')
            self.assertEqual(fnv_file(p),0x4f9f2cab)

    def test_card_copy_preserves_old_pack_and_verifies_content(self):
        from install_wikipedia import copy_to_card,plan
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);pack=root/'source';writer=PackWriter(pack,4096);writer.add(1,'苹果','完整正文'*2000);writer.finish({'partial':True})
            card=root/'card';card.mkdir();old=card/'wikipedia';old.mkdir();(old/'old.txt').write_text('preserve me')
            target=copy_to_card(pack,card,lambda _:None)
            self.assertTrue((target/'wiki.wki').exists());self.assertTrue((target/'SAMPLE.txt').exists())
            backups=list(card.glob('wikipedia-backup-*'));self.assertEqual(len(backups),1);self.assertEqual((backups[0]/'old.txt').read_text(),'preserve me')
            data=bytearray((pack/'text00000.bin').read_bytes());data[0]^=1;(pack/'text00000.bin').write_bytes(data)
            with self.assertRaises(ValueError):copy_to_card(pack,card,lambda _:None)
            self.assertNotEqual((target/'text00000.bin').read_bytes(),(pack/'text00000.bin').read_bytes())
            info=json.loads((pack/'manifest.json').read_text());info['files'][0]['file']='../escape';(pack/'manifest.json').write_text(json.dumps(info))
            with self.assertRaises(ValueError):plan(pack)

class WikiResumeTests(unittest.TestCase):
    def test_uncommitted_tail_is_removed_and_catalog_resumes_safely(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'wikipedia';writer=PackWriter(root,4096)
            first='第一篇正文'*800;second='第二篇正文'*900
            writer.add(1,'第一页',first);writer.db.commit()
            writer.add(2,'第二页','uncommitted'*3000)
            writer.part.flush();writer.part.close();writer.part=None;writer.db.close()
            recovered=PackWriter(root,4096,resume=True)
            self.assertTrue(recovered.contains(1));self.assertFalse(recovered.contains(2))
            self.assertEqual(recovered.position,len(first.encode('utf-8')))
            recovered.add(2,'第二页',second);info=recovered.finish({'partial':True})
            words=entries(root);self.assertEqual(info['articles'],2)
            self.assertEqual(body(root,words['第一页']),first);self.assertEqual(body(root,words['第二页']),second)
