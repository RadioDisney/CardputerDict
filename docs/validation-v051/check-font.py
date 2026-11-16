from pathlib import Path
import sys,json,re
from PIL import Image,ImageChops
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
out=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/font-v051/after');out.mkdir(exist_ok=True)
results={}
for name,port,model in [('adv','/dev/cu.usbmodem414301','Cardputer-Adv'),('normal','/dev/cu.usbmodem414401','Cardputer board_id=14')]:
 with open_device(port,timeout=.1) as p:
  status=command(p,'status');assert 'CardputerDict 0.5.1' in status and 'DEVICE model='+model in status,status
  before=json.loads((out.parent/'before'/(name+'-status.json')).read_text())
  for key in ['wifi_configured','volume']:
   assert re.search(key+r'=(\d+)',status)[1]==re.search(key+r'=(\d+)',before['status'])[1],(key,status)
  command(p,'lookup 苹果');state=command(p,'press space');assert 'CL:個|个[ge4],顆|颗[ke1]' in state,state
  screenshot(p,out/(name+'-apple.png'))
  a=Image.open(out.parent/'before'/(name+'-apple.png')).convert('RGB');b=Image.open(out/(name+'-apple.png')).convert('RGB')
  diff=ImageChops.difference(a,b);bbox=diff.getbbox();assert bbox,bbox
  print(name,'apple changed bounds',bbox,flush=True)
  command(p,'lookup 蘋果');screenshot(p,out/(name+'-traditional-title.png'))
  command(p,'press backspace');screenshot(p,out/(name+'-traditional-list.png'))
  command(p,'ime pingguo');screenshot(p,out/(name+'-pinyin.png'))
  command(p,'lookup TESLA');screenshot(p,out/(name+'-tesla.png'))
  baseline=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/outputs/CardputerDict-0.5.0/validation')/('normal/tesla.png' if name=='normal' else 'tesla.png')
  assert ImageChops.difference(Image.open(baseline).convert('RGB'),Image.open(out/(name+'-tesla.png')).convert('RGB')).getbbox() is None,'TESLA layout changed'
  command(p,'lookup 苹果');command(p,'press space');final=command(p,'status')
  results[name]={'before':before['status'],'initial':status,'apple_state':state,'apple_changed_bbox':bbox,'tesla_pixels_unchanged':True,'final':final,'passed':True}
(out/'font-check.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print('Both devices: traditional title/list/body, pinyin, original settings and exact TESLA pixel regression passed.')
