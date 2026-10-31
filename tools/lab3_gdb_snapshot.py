"""Read diagnostic RAM and journal pages over Wokwi's local GDB RSP server.

The halt changes execution timing. Never treat this capture as a timing scenario.
"""
import argparse
import json
from pathlib import Path
import socket
import struct

parser=argparse.ArgumentParser()
parser.add_argument('--port',type=int,required=True)
parser.add_argument('--schema',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
schema=json.loads(args.schema.read_text())
args.output.mkdir(parents=True,exist_ok=True)
packets=[]
result={'diagnostic_only':True,'schema':schema,'reads':{},'packets':packets}
connection=socket.create_connection(('localhost',args.port),timeout=5)
connection.settimeout(5)

def receive():
    while True:
        char=connection.recv(1)
        if not char: raise EOFError('RSP server closed')
        if char==b'$': break
    raw=bytearray()
    while True:
        char=connection.recv(1)
        if not char: raise EOFError('RSP packet interrupted')
        if char==b'#': break
        raw.extend(char)
    checksum=connection.recv(2)
    assert len(checksum)==2 and sum(raw)%256==int(checksum,16), 'RSP checksum mismatch'
    connection.sendall(b'+')
    reply=raw.decode('ascii')
    packets.append({'received':reply})
    return reply

def send(command,reply=True):
    data=command.encode('ascii')
    packets.append({'sent':command})
    connection.sendall(b'$'+data+b'#'+f'{sum(data)%256:02x}'.encode())
    return receive() if reply else None

try:
    connection.sendall(b'\x03')
    result['stop']=send('?')
    result['registers']=send('g')
    for label,item in schema['regions'].items():
        address,length=int(item['address'],16),item['bytes']
        raw=bytearray()
        for offset in range(0,length,128):
            count=min(128,length-offset)
            response=send(f'm{address+offset:x},{count:x}')
            assert len(response)==count*2, f'{label}: incomplete RSP read {response}'
            raw.extend(bytes.fromhex(response))
        (args.output/(label+'.bin')).write_bytes(raw)
        decoded={'address':item['address'],'bytes':length,'hex':raw.hex()}
        if 'fields' in item:
            assert len(item['fields'])*4==length
            decoded['fields']=dict(zip(item['fields'],struct.unpack('<'+'I'*(length//4),raw)))
        if label.startswith('page'):
            decoded.update(all_ff=all(x==255 for x in raw),all_zero=all(x==0 for x in raw))
        result['reads'][label]=decoded
except Exception as error:
    result['error']=repr(error)
    raise
finally:
    try: send('c',reply=False)
    finally:
        connection.close()
        (args.output/'snapshot.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:{x:y for x,y in v.items() if x!='hex'} for k,v in result['reads'].items()},indent=2))
