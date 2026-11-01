import importlib.util,json,hashlib,sys
from pathlib import Path
repo=Path(__file__).resolve().parents[1]; src=Path(sys.argv[1]); root=Path(sys.argv[2]); root.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('cap',repo/'tools/inspect_capture.py');cap=importlib.util.module_from_spec(spec);spec.loader.exec_module(cap);ch,end=cap.load(src)
def decode(trace):
 times,values=trace;eligible=0;frames=[];bit=1e9/115200
 for i,(t,s) in enumerate(zip(times,values)):
  if t<eligible or s or i==0 or values[i-1]!=1:continue
  frames.append((t,sum(cap.value(trace,t+(1.5+b)*bit)<<b for b in range(8)),cap.value(trace,t+9.5*bit)));eligible=t+9.7*bit
 lines=[];cur=bytearray();first=None;bad=0
 for t,b,stop in frames:
  if first is None:first=t
  cur.append(b);bad+=not stop
  if b==10:lines.append({'start_ns':first,'end_ns':round(t+10*bit),'text':cur.decode(errors='replace').rstrip()});cur.clear();first=None
 return lines,sum(not f[2] for f in frames),len(frames)
rx,rb,rn=decode(ch['UART_RX']);tx,tb,tn=decode(ch['UART_TX']); data={'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'duration_s':end/1e9,'rx_bytes':rn,'rx_bad_stop':rb,'tx_bytes':tn,'tx_bad_stop':tb,'rx_lines':rx,'tx_lines':tx};(root/'uart-sweep-wire.json').write_text(json.dumps(data,indent=2)+'\n');print('RX',len(rx),'bad',rb,'TX',len(tx),'bad',tb);print('first RX',rx[:8]);print('first replies',[a for a in tx if a['text'].startswith('reply=')][:8])
