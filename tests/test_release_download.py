import json,sys,tempfile,unittest,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from download_release_data import extract
class ReleaseExtractionTests(unittest.TestCase):
    def test_safe_subdirectories_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'sample.zip'
            with zipfile.ZipFile(archive,'w') as z:z.writestr('wikipedia/text00000.bin',b'content')
            extract(archive,root/'output',['wikipedia/text00000.bin'])
            self.assertEqual((root/'output/wikipedia/text00000.bin').read_bytes(),b'content')
            with self.assertRaises(ValueError):extract(archive,root/'output',['wikipedia/text00000.bin'])
    def test_path_traversal_and_missing_files_rejected(self):
        for name in ['../escape','/absolute','folder\\escape']:
            with self.subTest(name=name),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);archive=root/'bad.zip'
                with zipfile.ZipFile(archive,'w') as z:z.writestr(name,b'bad')
                with self.assertRaises(ValueError):extract(archive,root/'output')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'missing.zip'
            with zipfile.ZipFile(archive,'w') as z:z.writestr('safe',b'ok')
            with self.assertRaises(ValueError):extract(archive,root/'output',['missing'])
