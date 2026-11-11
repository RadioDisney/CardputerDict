import sys,json
from pathlib import Path
from PIL import Image
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work')
p=open_device('/dev/cu.usbmodem414301',timeout=.1)
with p:
 status=command(p,'status')
 assert 'CardputerDict 0.2.3' in status and 'display=240x135 rotation=0' in status,status
 apple=command(p,'lookup apple');assert '苹果' in apple,apple
 screenshot(p,root/'spacing2-after.png')
 long_word=command(p,'lookup dictionary');assert 'LOOKUP: dictionary' in long_word,long_word
 screenshot(p,root/'spacing2-long.png')
 chinese=command(p,'lookup 你好');assert 'hello' in chinese.lower(),chinese
 screenshot(p,root/'spacing2-chinese.png')
 command(p,'lookup apple')
def ink_bounds(path):
 image=Image.open(path).convert('RGB')
 ink=[(x,y) for y in range(43,58) for x in range(4,236) if min(image.getpixel((x,y)))>180]
 assert ink
 return [min(x for x,y in ink),min(y for x,y in ink),max(x for x,y in ink),max(y for x,y in ink)]
before=ink_bounds(root/'spacing-after.png');after=ink_bounds(root/'spacing2-after.png')
assert before[0]==after[0] and before[1]==after[1] and before[3]==after[3],(before,after)
assert after[2]-before[2]==len("/'æpl/")-1,(before,after)
result={'passed':True,'status':status.strip(),'before_first_line_bounds':before,'after_first_line_bounds':after,'expected_extra_width':len("/'æpl/")-1,'actual_extra_width':after[2]-before[2],'apple':apple.strip(),'long_word':long_word.strip(),'chinese':chinese.strip()}
(root/'spacing2-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('long_word','chinese','apple')},ensure_ascii=False,indent=2))
