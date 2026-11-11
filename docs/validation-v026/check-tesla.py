import sys,re,json
from pathlib import Path
from PIL import Image,ImageChops
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tests')
from test_ipa_font import PhoneticFontTests
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v026');root.mkdir(exist_ok=True)
with open_device('/dev/cu.usbmodem414301',timeout=.1) as port:
 status=command(port,'status');assert 'CardputerDict 0.2.6' in status,status
 match=re.search(r'definition font_height=(\d+) line_gap=(\d+) line_step=(\d+) rows=(\d+)',status)
 assert match,status
 height,gap,step,rows=map(int,match.groups())
 assert gap==4 and step==height+4 and rows==4 and 43+(rows-1)*step+height<=121,(height,gap,step,rows)
 tesla=command(port,'lookup TESLA');assert "/'teslә/" in tesla,tesla
 screenshot(port,root/'tesla.png')
 apple=command(port,'lookup apple');assert '苹果' in apple
 screenshot(port,root/'apple-page-one.png')
 command(port,'press space');screenshot(port,root/'apple-page-two.png')
 chinese=command(port,'lookup 你好');assert 'hello' in chinese.lower()
 screenshot(port,root/'chinese.png')
 command(port,'lookup TESLA')
PhoneticFontTests.setUpClass();reader=PhoneticFontTests()
offset,width,glyph_height,advance,x_offset,y_offset=reader.glyph('ә')
bitmap=reader.bitmap[offset:offset+(width*glyph_height+7)//8]
mask=[[bool(bitmap[(y*width+x)//8] & (1 << (7-(y*width+x)%8))) for x in range(width)] for y in range(glyph_height)]
def matches(path):
 image=Image.open(path).convert('RGB');found=[]
 for top in range(43,58-glyph_height+1):
  for left in range(45,78-width+1):
   if all((min(image.getpixel((left+x,top+y)))>180)==mask[y][x] for y in range(glyph_height) for x in range(width)):
    found.append([left,top])
 return found
visible=matches(root/'tesla.png');assert visible,'Schwa glyph bitmap not found on actual framebuffer'
missing_before=matches(Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/tesla-before.png'))
assert not missing_before,missing_before
old=Image.open('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v025/definition.png').convert('RGB')
new=Image.open(root/'apple-page-two.png').convert('RGB')
assert ImageChops.difference(old.crop((4,107,236,107+height)),new.crop((4,43,236,43+height))).getbbox() is None,'Final apple line missing or changed after pagination'
result={'passed':True,'status':status.strip(),'tesla':tesla.strip(),'source_codepoint':'U+04D9 CYRILLIC SMALL LETTER SCHWA','glyph_size':[width,glyph_height],'glyph_positions':visible,'glyph_absent_before':not missing_before,'line_gap':gap,'line_step':step,'rows_per_page':rows,'final_line_pagination_verified':True}
(root/'tesla-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
