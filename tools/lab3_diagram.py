"""Derive an explicitly separate timing probe wiring from this product's diagram."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
diagram=json.loads((root/'diagram.json').read_text())
logic=next(p for p in diagram['parts'] if p['id']=='logic')
logic['attrs']['channelNames']='FLASH,SDA,UART_TX,BUTTON,ADC_MARK,UART_RX,POINT,IMU_MARK'
for connection in diagram['connections']:
    if connection[1]=='logic:D0':
        connection[0]='nucleo:PA6'
    if connection[1]=='logic:D5':
        connection[0]='nucleo:PA3'
(root/'diagram-lab3.json').write_text(json.dumps(diagram,indent=2)+'\n')
print('diagram-lab3.json: FLASH PA6 and UART_RX PA3 replace SCL/LED probes only')
