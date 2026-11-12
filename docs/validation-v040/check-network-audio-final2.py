import sys,time,re,json
from pathlib import Path
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v040');root.mkdir(exist_ok=True)
result={}
with open_device('/dev/cu.usbmodem414301',timeout=.1) as port:
 status=command(port,'status');assert 'wifi_configured=1' in status,status
 result['initial']=status
 result['first']=command(port,'speak language');print(result['first'],flush=True)
 assert 'state=connecting' in result['first'],result['first']
 end=time.monotonic()+60
 logs=[]
 while time.monotonic()<end:
  state=command(port,'audio status',timeout=35);logs.append(state);print(state,flush=True)
  if 'state=idle' in state:
   assert 'path=/audio/cache/en/language.mp3' in state,state
   assert int(re.search(r'frames=(\d+)',state)[1])>1000 and int(re.search(r'peak=(\d+)',state)[1])>100,state
   result['first_done']=state;break
  time.sleep(.2)
 else:raise TimeoutError('Network audio timed out')
 result['network_log']=logs
 result['offline']=command(port,'speaklocal language');print(result['offline'],flush=True)
 assert 'AUDIO start word=language' in result['offline'] and 'state=playing' in result['offline'],result['offline']
 assert 'wifi_connected=0' in result['offline'],result['offline']
 screenshot(port,root/'cached-language.png')
 end=time.monotonic()+8
 while time.monotonic()<end:
  state=command(port,'audio status')
  if 'state=idle' in state:
   result['offline_done']=state;break
  time.sleep(.1)
 else:raise TimeoutError('Cached playback did not complete')
 result['lookup']=command(port,'lookup language');assert '语言' in result['lookup'],result['lookup']
 screenshot(port,root/'pronunciation-ready.png')
 result['final_status']=command(port,'status')
 assert 'sd=1 words=770611' in result['final_status'],result['final_status']
 result['passed']=True
 (root/'network-audio-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print('HTTPS pronunciation, SD caching, and offline replay verified',flush=True)
