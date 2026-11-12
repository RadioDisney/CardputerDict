import sys,time,re,json
from pathlib import Path
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
from check_pronunciation import wait_idle
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v042');root.mkdir(exist_ok=True)
result={'words':{}}
def query(text):
 return re.search(r'STATE direction=\S+ query=(.*?) composition=',text)[1]
with open_device('/dev/cu.usbmodem414301',timeout=.1) as p:
 status=command(p,'status');assert 'CardputerDict 0.4.2' in status and 'keyboard scanner=task scan_tick_ms=1 debounce_ms=2' in status,status
 result['initial']=status
 command(p,'lookup apple')
 for word in ['hello','letterll','bookkeeper','mississippi','eeeeeeee']:
  command(p,'press clear')
  ack=command(p,'keytest '+word);assert 'KEYTEST held_ms=8 gap_ms=4' in ack,ack
  time.sleep(.45)
  state=command(p,'input state');assert query(state)==word,(word,state)
  result['words'][word]=state
 command(p,'press clear');command(p,'type abcdefghijklmnopqrst')
 ack=command(p,'keytest-hold-backspace');assert 'KEYTEST backspace held_ms=1200' in ack,ack
 time.sleep(1.5)
 held=command(p,'input state');remaining=query(held);deleted=20-len(remaining)
 assert 6<=deleted<=8 and remaining=='abcdefghijklmnopqrst'[:len(remaining)],(deleted,held)
 time.sleep(.3);released=command(p,'input state');assert query(released)==remaining,released
 result['held_backspace']={'state':held,'deleted_in_1200ms':deleted,'release_stopped':True}
 command(p,'press clear');command(p,'type abcdef')
 command(p,'keytest *');time.sleep(.25)
 # '*' maps to Shift+8 in text injection, but raw base '*' is the backspace's
 # HID value; only this diagnostic path uses it to exercise a physical tap.
 tapped=command(p,'input state');assert query(tapped)=='abcde',tapped
 result['single_backspace']=tapped
 command(p,'lookup apple')
 assert 'AUDIO start word=apple' in command(p,'press pronounce')
 played=wait_idle(p);assert 'frames=56208' in played and 'peak=24918' in played,played
 result['audio']=played
 command(p,'press clear');command(p,'keytest hello');time.sleep(.45)
 assert query(command(p,'input state'))=='hello'
 final=command(p,'status');assert 'dropped=0' in final and 'sd=1 words=770611' in final,final
 result['final']=final
 screenshot(p,root/'fast-input.png')
 result['passed']=True
 (root/'fast-input-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(result,ensure_ascii=False,indent=2))
