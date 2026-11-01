import importlib.util,json,hashlib,bisect,statistics
from pathlib import Path
p=Path(__file__).resolve().parent; repo=p.parents[2]/'Lab01/Work/ShelfWatch';sp=importlib.util.spec_from_file_location('i',repo/'tools/inspect_capture.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);c,end=m.load(p/'short-fixture.vcd')
def transitions(n):return [{'ms':t/1e6,'value':v} for t,v in zip(*c[n]) if t>0]
phase=transitions('FAULT_PHASE');ready=transitions('READY');sda=m.pulses(c['SDA']);low=[];ts,vs=c['SDA'];
for i,v in enumerate(vs[:-1]):
 if not v and ts[i+1]-ts[i]>1000000:low.append({'start_ms':ts[i]/1e6,'end_ms':ts[i+1]/1e6,'duration_ms':(ts[i+1]-ts[i])/1e6})
tx=m.transactions(c); candidates=[{'start_ms':t['start_ns']/1e6,'bytes':t['bytes'],'ack':t['ack']} for t in tx if t['bytes'] and t['bytes'][0] in [208,209]]
print('ready',ready,'phase',phase,'SDA long-low',low[:10]);print('first I2C',candidates[:12]);print('ADC pulses first',m.pulses(c['ADC_MARK'])[:3]);print('IMU first',m.pulses(c['IMU_MARK'])[:3]);x={'vcd_sha256':hashlib.sha256((p/'short-fixture.vcd').read_bytes()).hexdigest(),'duration_s':end/1e9,'ready':ready,'phase':phase,'sda_long_low':low,'first_i2c':candidates[:35],'adc_first_pulses_ns':m.pulses(c['ADC_MARK'])[:3],'imu_first_pulses_ns':m.pulses(c['IMU_MARK'])[:3]};(p/'analysis.json').write_text(json.dumps(x,indent=2)+'\n')
