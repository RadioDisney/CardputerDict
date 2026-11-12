import sys,time,re,json
from pathlib import Path
sys.path.insert(0,'/Users/huxunxun/WorkSpace/CardputerDict/tools')
from serial_link import open_device
from device_check import command,screenshot
from check_pronunciation import wait_idle
root=Path('/Users/huxunxun/Documents/ChatGPT/Cardpute/work/device-v040');root.mkdir(exist_ok=True)
result={}
with open_device('/dev/cu.usbmodem414301',timeout=.1) as p:
 result['initial']=command(p,'status');assert 'CardputerDict 0.4.0' in result['initial'] and 'wifi_configured=1' in result['initial']
 command(p,'lookup apple')
 result['fn_p']=command(p,'press pronounce');assert 'AUDIO start word=apple' in result['fn_p']
 result['fn_p_stop']=command(p,'press pronounce')
 assert 'state=idle' in command(p,'audio status')
 command(p,'lookup hello');result['wav_shortcut']=command(p,'press pronounce');assert 'format=wav' in result['wav_shortcut']
 result['wav_done']=wait_idle(p);assert 'frames=60096' in result['wav_done'] and 'peak=25265' in result['wav_done']
 result['wav8']=command(p,'speaklocal audio-test');assert 'format=wav rate=16000 channels=2' in result['wav8'],result['wav8']
 result['wav8_done']=wait_idle(p);assert 'frames=2400' in result['wav8_done'] and 'peak=5120' in result['wav8_done'],result['wav8_done']
 result['radio']=command(p,'speaklocal radio');assert 'path=/audio/cache/en/radio.mp3' in result['radio']
 result['radio_done']=wait_idle(p);assert 'frames=32303' in result['radio_done']
 result['status']=command(p,'status');assert 'sd=1 words=770611' in result['status']
 p.reset_input_buffer();p.write(b'restart\n');p.flush();deadline=time.monotonic()+15;logs=bytearray()
 while time.monotonic()<deadline:
  logs.extend(p.read(p.in_waiting or 1))
  if b'CardputerDict ready.' in logs:break
 else:raise RuntimeError('Restart failed '+repr(logs))
 result['restart']=command(p,'status');assert 'wifi_configured=1' in result['restart'] and 'wifi_connected=0' in result['restart'] and 'mode=cache' in result['restart']
 result['cached_after_restart']=command(p,'speaklocal language');assert 'AUDIO start word=language' in result['cached_after_restart'] and 'wifi_connected=0' in result['cached_after_restart']
 result['cached_after_restart_done']=wait_idle(p)
 assert 'frames=66288' in result['cached_after_restart_done']
 command(p,'lookup radio');time.sleep(2.6);screenshot(p,root/'final-ready.png')
 result['final']=command(p,'status');assert 'sd=1 words=770611' in result['final']
 result['passed']=True
 (root/'final-audio-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(result,ensure_ascii=False,indent=2))
