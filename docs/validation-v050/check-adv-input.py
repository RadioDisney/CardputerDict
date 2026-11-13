import sys,time,re,json
from pathlib import Path
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/WorkSpace/CardputerDict/build/adv-device')
result={'words':{}}
def query(text):
 return re.search(r'STATE direction=\S+ query=(.*?) composition=',text)[1]
with open_device('/dev/cu.usbmodem414301',timeout=.1) as p:
 status=command(p,'status');assert 'CardputerDict 0.5.0' in status and 'DEVICE model=Cardputer-Adv' in status and 'i2c_errors=0' in status,status
 result['initial']=status
 command(p,'lookup apple')
 for word in ['hello','letterll','bookkeeper','mississippi','eeeeeeee']:
  command(p,'press clear');command(p,'keytest '+word);time.sleep(.45)
  state=command(p,'input state');assert query(state)==word,(word,state)
  result['words'][word]=state
 command(p,'press clear');command(p,'type abcdefghijklmnopqrst')
 command(p,'keytest-hold-backspace');time.sleep(1.5)
 held=command(p,'input state');remaining=query(held);deleted=20-len(remaining)
 assert 6<=deleted<=8 and remaining=='abcdefghijklmnopqrst'[:len(remaining)],(deleted,held)
 time.sleep(.3);assert query(command(p,'input state'))==remaining
 result['backspace']={'state':held,'deleted_in_1200ms':deleted,'release_stopped':True}
 command(p,'lookup apple')
 state=command(p,'input state');pages=int(re.search(r'DEFINITION page=0 pages=(\d+)',state)[1]);assert pages>1
 previous=command(p,'press previous');assert f'DEFINITION page={pages-1} pages={pages}' in previous,previous
 forward=command(p,'press space');assert f'DEFINITION page=0 pages={pages}' in forward,forward
 result['circular_pages']={'previous_from_first':previous,'next_from_last':forward}
 command(p,'lookup TESLA');screenshot(p,root/'tesla.png')
 result['final']=command(p,'status');assert 'dropped=0' in result['final'] and 'fifo_overflows=0' in result['final'] and 'i2c_errors=0' in result['final']
 screenshot(p,root/'keyboard-test.png')
 command(p,'lookup apple');command(p,'press clear')
 result['passed']=True
 (root/'input-regression.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(result,ensure_ascii=False,indent=2))
