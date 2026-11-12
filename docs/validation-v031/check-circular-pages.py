import sys,re,json
from pathlib import Path
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v031');root.mkdir(exist_ok=True)
def page(response):
 match=re.search(r'DEFINITION page=(\d+) pages=(\d+)',response)
 assert match,response
 return tuple(map(int,match.groups()))
checks=[]
with open_device('/dev/cu.usbmodem414301',timeout=.1) as port:
 status=command(port,'status');assert 'CardputerDict 0.3.1' in status,status
 for word in ['light','苹果']:
  command(port,'lookup '+word)
  for key in ['up','previous']:
   command(port,'lookup '+word)
   last,count=page(command(port,'press '+key));assert count>1 and last==count-1,(word,key,last,count)
   first,newcount=page(command(port,'press next'));assert first==0 and newcount==count
   checks.append({'word':word,'previous_key':key,'first_to_last':last,'pages':count})
  for key in ['space','enter','down','next']:
   command(port,'lookup '+word)
   current,count=page(command(port,'press previous'));assert current==count-1
   zero,actual=page(command(port,'press '+key));assert zero==0 and actual==count,(word,key,zero,count)
   for expected in range(1,count):
    actual_page,actual_count=page(command(port,'press '+key));assert actual_page==expected and actual_count==count
   wrapped,actual_count=page(command(port,'press '+key));assert wrapped==0 and actual_count==count
   checks.append({'word':word,'next_key':key,'last_to_first':wrapped,'full_cycle':True,'pages':count})
 command(port,'lookup TESLA')
 for key in ['space','enter','up','down','previous','next']:
  assert page(command(port,'press '+key))==(0,1),key
 checks.append({'word':'TESLA','single_page_stays_first':True})
 command(port,'lookup light');command(port,'press previous');screenshot(port,root/'last-page.png')
 command(port,'press space');screenshot(port,root/'first-page.png')
 command(port,'press up')
 result={'passed':True,'status':status.strip(),'checks':checks}
 (root/'circular-pages-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(result,ensure_ascii=False,indent=2))
