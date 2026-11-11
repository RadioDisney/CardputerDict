import sys,re,json
from pathlib import Path
from PIL import Image,ImageChops
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v025');root.mkdir(exist_ok=True)
with open_device('/dev/cu.usbmodem414301',timeout=.1) as port:
 status=command(port,'status')
 assert 'CardputerDict 0.2.5' in status and 'display=240x135 rotation=0' in status,status
 match=re.search(r'definition font_height=(\d+) line_gap=(\d+) line_step=(\d+) rows=(\d+)',status)
 assert match,status
 height,gap,step,rows=map(int,match.groups())
 assert gap==2 and step==height+2 and 43+(rows-1)*step+height<=121,(height,gap,step,rows)
 apple=command(port,'lookup apple');assert '苹果' in apple
 screenshot(port,root/'definition.png')
 long_text=command(port,'lookup light');assert len(long_text.splitlines())>10,long_text
 screenshot(port,root/'long-page-one.png')
 command(port,'press space');screenshot(port,root/'long-page-two.png')
 assert (root/'long-page-one.png').read_bytes()!=(root/'long-page-two.png').read_bytes()
 chinese=command(port,'lookup 你好');assert 'hello' in chinese.lower()
 screenshot(port,root/'chinese.png')
 command(port,'lookup apple')
before=Image.open('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v024/definition.png').convert('RGB')
after=Image.open(root/'definition.png').convert('RGB')
checked=[]
for row in range(rows):
 a=before.crop((4,43+row*15,236,43+row*15+height))
 b=after.crop((4,43+row*step,236,43+row*step+height))
 assert ImageChops.difference(a,b).getbbox() is None,('row pixels differ',row)
 checked.append({'row':row+1,'previous_y':43+row*15,'new_y':43+row*step})
result={'passed':True,'status':status.strip(),'font_height':height,'line_gap':gap,'line_step':step,'rows_per_page':rows,'pixel_rows':checked,'pagination_verified':True}
(root/'line-gap-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
