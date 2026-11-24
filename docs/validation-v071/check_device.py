from pathlib import Path
from PIL import Image,ImageChops
import sys,json,re,time
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
root=Path(__file__).resolve().parent;out=root/'device';out.mkdir(exist_ok=True);result={}
def field(text,key):return int(re.search(r'\b'+key+r'=(\d+)',text)[1])
def selection(text):return next(l for l in text.splitlines() if l.startswith('TEXT_SELECTION '))
def selected(text):return next((l[16:] for l in text.splitlines() if l.startswith('SELECTION_TEXT: ')),None)
with open_device('/dev/cu.usbmodem413301',timeout=.1) as p:
 def run(cmd):
  p.write((cmd+'\n').encode());p.flush();data=bytearray();deadline=time.monotonic()+240
  while b'CMD_DONE' not in data:
   assert time.monotonic()<deadline,('Timeout',cmd,data[-200:]);data.extend(p.read(p.in_waiting or 1))
  answer=data.decode('utf-8',errors='replace');assert 'ERR ' not in answer,(cmd,answer);return answer
 def state():return run('input state')
 def snap(name):screenshot(p,out/(name+'.png'))
 initial=run('status');assert 'CardputerDict 0.7.1' in initial,initial;result['initial']=initial
 run('lookup apple');old=run('press ctrl-s');assert field(selection(old),'active')==0,old
 run('press ctrl-f');entered=state();assert field(selection(entered),'active')==1 and field(entered.split('SELECTION_UI ')[1],'cursor_visible')==1,entered
 visible=None;hidden=None
 for i in range(16):
  s=state();snap('blink-'+str(i));image=Image.open(out/('blink-'+str(i)+'.png')).convert('RGB')
  # A screenshot may cross a blink edge; use the pixels as the authority.
  if image.getpixel((5,43))!=image.getpixel((6,43)):visible=image;visible.save(out/'cursor-visible.png')
  else:hidden=image;hidden.save(out/'cursor-hidden.png')
  if visible is not None and hidden is not None:break
  time.sleep(.11)
 assert visible is not None and hidden is not None,'Did not observe both blink phases'
 delta=ImageChops.difference(visible,hidden);bbox=delta.getbbox();assert bbox==(5,43,6,57),bbox
 result['cursor_blink']={'changed_bbox':bbox,'single_vertical_line':True}
 moved=run('press right');assert field(moved.split('SELECTION_UI ')[1],'cursor_visible')==1,moved
 run('press ctrl-f');run('lookup apple');run('press ctrl-f')
 repeat_before=field(state().split('SELECTION_UI ')[1],'direction_repeats')
 # Raw Ctrl+right hold comes through the same scanner and queue as physical input.
 ack=run('keytest-hold-direction ctrl-right 1200');assert 'KEYTEST direction=ctrl-right' in ack,ack
 time.sleep(1.45);held=state();assert field(selection(held),'end')>field(selection(held),'begin'),held
 count=field(held.split('SELECTION_UI ')[1],'direction_repeats')-repeat_before;assert 5<=count<=7,(count,held)
 stable=field(selection(held),'cursor');time.sleep(.35);released=state();assert field(selection(released),'cursor')==stable,released
 result['ctrl_arrow_repeat']={'ack':ack,'held':held,'released':released}
 run('press ctrl-f');run('lookup apple');run('press ctrl-f')
 definition=state().split('\nTEXT: ',1)[1].rsplit('\nDEFINITION ',1)[0];offset=definition.encode().index('苹果'.encode())
 for _ in range(100):
  s=state();at=field(selection(s),'cursor')
  if at==offset:break
  assert at<offset,(at,offset,s);run('press right')
 else:raise RuntimeError('Cannot reach 苹果')
 run('press ctrl-right');chosen=run('press ctrl-right');assert selected(chosen)=='苹果',chosen;snap('selected')
 # Enter returns after the progress sprite was pushed, before the deferred lookup.
 request=run('press enter');assert 'SELECTION_LOOKUP state=queued' in request and 'state=shown' in request and '查询中' in request,request
 completed=state();trace=request+completed;assert 'SELECTION_LOOKUP state=started' in trace and 'LOOKUP: 苹果' in trace and 'SOURCE /dictionaries/cedict.ifo' in completed,completed
 queued=field(request.split('state=queued ')[1],'ms');shown=field(request.split('state=shown ')[1],'ms');started=field(trace.split('state=started ')[1],'ms')
 assert queued<=shown<=started,(queued,shown,started)
 assert field(selection(completed),'active')==0,completed;snap('query-result')
 result['plain_enter_lookup']={'request':request,'completed':completed,'queued_ms':queued,'lcd_shown_ms':shown,'lookup_started_ms':started}
 run('lookup apple');run('press clear');run('type apple');time.sleep(.2)
 run('keytest-hold-direction down 1200');time.sleep(1.45);list_state=state();assert field(list_state,'selected')==4,list_state
 assert 'query=apple composition= ' in list_state,list_state
 run('press clear');run('keytest-hold-direction fn-down 1200');time.sleep(1.45);punct=state();assert 'query=. composition= ' in punct,punct
 result['list_repeat']=list_state;result['fn_literal_not_repeated']=punct
 run('lookup 苹果');run('press ctrl-f');first=state()
 for _ in range(5):s=run('press ctrl-right')
 assert selected(s)=='[Ping',s
 miss_request=run('press enter');assert '查询中' in miss_request,miss_request
 miss_done=state();assert '未找到所选词' in miss_done and field(selection(miss_done),'active')==1,miss_done
 assert selected(miss_done)=='[Ping',miss_done;result['missing_retains_selection']=miss_done
 run('press `');run('wiki 苹果');run('press ctrl-f')
 for _ in range(4):s=run('press ctrl-down')
 assert 'WIKI_PAGE page=1 ' in s and field(selection(s),'active')==1,s
 result['wiki_cross_page']=s;snap('wiki-cross-page')
 run('press ctrl-f');run('wiki 苹果');run('press ctrl-f');run('press ctrl-right');s=run('press ctrl-right');assert selected(s)=='苹果',s
 wiki_request=run('press enter');assert '查询中' in wiki_request,wiki_request
 wiki_result=state();assert 'WIKI_LOOKUP: 苹果' in wiki_result and 'SOURCE @wiki' in wiki_result,wiki_result
 result['wiki_plain_enter']={'request':wiki_request,'result':wiki_result}
 run('lookup apple');run('press ctrl-f')
 final=run('status');assert 'dropped=0' in final,final
 before=(root/'before-status.txt').read_text()
 for key in ['wifi_configured','volume']:assert field(final,key)==field(before,key),(key,final)
 result['final']=final;result['passed']=True
(out/'interaction-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('Passed: Ctrl+F replaces Ctrl+S, 1px blinking caret, scanner Ctrl/plain arrow repeat and release, Fn punctuation single press, Enter lookup, progress shown before lookup, wiki cross-page and configuration preservation.')
