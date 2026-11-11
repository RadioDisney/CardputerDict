import sys,time,re,json,struct
from pathlib import Path
from PIL import Image,ImageChops
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v027');root.mkdir(exist_ok=True)
index=Path('/Users/huxunxun/WorkSpace/CardputerDict/sdcard/dictionaries/ecdict.idx').read_bytes();pos=0;words=[];found=False
while pos<len(index):
 end=index.index(0,pos);raw=index[pos:end];pos=end+9
 if raw.lower()==b'apple':found=True
 if found:words.append(raw.decode('utf-8'))
 if len(words)==5:break
assert len(words)==5
field=lambda text,name:int(re.search(r'\b'+name+r'=(\d+)',text).group(1))
with open_device('/dev/cu.usbmodem414301',timeout=.1) as port:
 status=command(port,'status');assert 'CardputerDict 0.2.7' in status,status
 command(port,'lookup apple');command(port,'press clear');command(port,'type apple');time.sleep(.15)
 state=command(port,'status');assert 'results line_step=17 rows=4 start=0 selected=0 count=5' in state,state
 screenshot(port,root/'list.png')
 for _ in range(4):last=command(port,'press down')
 assert field(last,'selected')==4 and field(last,'list_start')==1 and field(last,'list_rows')==4,last
 screenshot(port,root/'list-scrolled.png')
 selected=command(port,'press enter');assert 'LOOKUP: '+words[4]+'\n' in selected,selected
 command(port,'ime zhong');chosen=command(port,'press 1');assert 'query=中 composition= ' in chosen,chosen
 time.sleep(.15);command(port,'type guo')
 for _ in range(3):composing=command(port,'press down')
 assert field(composing,'selected')==3 and field(composing,'list_start')==2 and field(composing,'list_rows')==2,composing
 screenshot(port,root/'composing.png')
 command(port,'lookup apple');command(port,'press clear');command(port,'type apple');time.sleep(.15)
before=Image.open('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/word-list-before.png').convert('RGB');after=Image.open(root/'list.png').convert('RGB')
for row in range(4):
 assert ImageChops.difference(before.crop((4,43+row*15,236,43+row*15+14)),after.crop((4,43+row*17,236,43+row*17+14))).getbbox() is None,('row changed',row)
image=Image.open(root/'composing.png').convert('RGB')
assert not any(min(image.getpixel((x,y)))>180 for y in range(75,88) for x in range(4,236)),'List text overlaps composition panel'
result={'passed':True,'status':state.strip(),'row_step':17,'previous_row_step':15,'added_pixels':2,'visible_rows':4,'visible_rows_while_composing':2,'words':words,'fifth_selection':selected.strip(),'scrolled_state':last.strip(),'composition_state':composing.strip(),'pixel_rows_identical':True,'composition_overlap':False}
(root/'word-list-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='fifth_selection'},ensure_ascii=False,indent=2))
