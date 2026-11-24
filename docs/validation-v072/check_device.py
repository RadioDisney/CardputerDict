from pathlib import Path
import sys,json
from PIL import Image,ImageChops
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
out=Path(__file__).resolve().parent;results={}
with open_device('/dev/cu.usbmodem413301',timeout=.1) as p:
 status=command(p,'status');assert 'CardputerDict 0.7.2' in status and 'sd=1' in status and 'wifi_configured=1' in status and 'volume=223' in status,status
 (out/'status-after.txt').write_text(status)
 command(p,'lookup apple');screenshot(p,out/'reading-after.png')
 command(p,'press ctrl-f');screenshot(p,out/'selection-after.png')
 command(p,'press ctrl-f');command(p,'press clear');screenshot(p,out/'search-after.png')
 for name in ['reading','selection','search']:
  before=Image.open(out/(name+'-before.png')).convert('RGB');after=Image.open(out/(name+'-after.png')).convert('RGB')
  region=(0,118,240,135) if name!='search' else (0,18,240,135)
  a=before.crop(region);b=after.crop(region);diff=ImageChops.difference(a,b);assert diff.getbbox(),name
  # Every change must be a recolor from the original muted gray to key cyan.
  colors={}
  for c,d in zip(a.getdata(),b.getdata()):
   if c!=d:colors[str((c,d))]=colors.get(str((c,d)),0)+1
  assert len(colors)==1,(name,colors)
  results[name]={'recolored_pixels':sum(colors.values()),'color_pairs':colors,'unchanged_geometry':True}
 # Main reading body and header must remain identical.
 assert ImageChops.difference(Image.open(out/'reading-before.png').crop((0,0,240,118)),Image.open(out/'reading-after.png').crop((0,0,240,118))).getbbox() is None
 command(p,'lookup apple');final=command(p,'status');assert 'dropped=0' in final,final
 results['passed']=True;results['body_unchanged']=True
(out/'validation.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(results,ensure_ascii=False,indent=2))
