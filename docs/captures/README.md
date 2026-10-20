# Власні записи LightBench

Фактичний запуск Wokwi 27.10.2026, build ed8ba61-d5b4dbed. [Manifest](manifest.json) містить SHA256 прошивки, джерел і файлів. Дати Git не є датами вимірювання.

- final.vcd.gz є повним lossless VCD фінального запуску, 29.25814355 с модельного часу.
- final-uart.txt є сирим UART, final-session-uart.txt його точний фінальний сеанс.
- measurements.json та i2c-transactions.json отримані власним аналізатором переходів.
- Кожне вікно VCD має origin з offset і hash повного джерела. SR перетворено при 20 МГц, крок 50 нс; первинні переходи мають шкалу 1 нс.
- PVS задає підписи та декодери PulseView. Скриншоти у ../figures зроблено з цих записів. Для UART використано також окреме збільшене вікно uart-fragment.

| Канал | Модель / вимір |
|---|---|
| SCL/SDA | 100 кГц, median SCL period 10 мкс |
| WHO_AM_I | pointer 0x75, read 0x68; read phase 195 мкс, pointer+read 624.275 мкс |
| Burst | 14 bytes із 0x3B; read phase 1365 мкс, pointer+read 1899.375 мкс |
| IMU_MARK | median ширина 2014.7 мкс, period 19993.575 мкс |
| ADC_MARK | median ширина 50.725 мкс, period 19996.825 мкс, max period 27504.6 мкс |
| BUTTON/POINT | 75 груп і 75 точок; max 39.167 мс до POINT |
| UART_TX | median bit 8.7 мкс проти розрахункових 8.6806 мкс, близько 0.22% |
| NACK | 0xD2, ACK bit=1, 105 мкс; повтор через 1 с |

ADC_MARK охоплює HAL start/read/stop, а не лише час семплювання. Теорія ADC при 24 МГц: (160.5+12.5)/24 = 7.21 мкс. IMU max period 1577.638 мс належить навмисному NACK/recovery. Це одна gap, не штатна періодика.

## Ручне читання I²C

У вікні WHO_AM_I SDA переходить HIGH→LOW при HIGH SCL, це START. Наступні вісім значень SDA на фронтах SCL утворюють 11010000, тобто 0xD0. Сім старших бітів 1101000 дають адресу 0x68, молодший біт 0 означає write. Дев'ятий біт LOW означає ACK. Далі йде 01110101, регістр 0x75, та ACK. Після STOP і наступного START адресний байт 11010001 означає read; повертається 01101000, тобто WHO_AM_I=0x68. Master завершує останній байт NACK і STOP. Два окремі цикли pointer/read відповідають адаптеру з AUTOEND.

Для негативного тесту 11010010 означає 0x69 write. Дев'ятий біт HIGH є NACK. Після нього видно додатковий START без повного байта. Ця аномалія збережена у первинному записі; успішний burst так не виглядає.

## Повторення аналізу

```sh
python3 tools/analyze_export.py docs/captures/final-session-uart.txt
gzip -dc docs/captures/final.vcd.gz > /tmp/lightbench-final.vcd
python3 tools/inspect_capture.py /tmp/lightbench-final.vcd /tmp/lightbench-review
sigrok-cli -i docs/captures/i2c-whoami.sr -P i2c:scl=SCL:sda=SDA -A i2c=addr-data
sigrok-cli -i docs/captures/uart-startup.sr -P uart:tx=UART_TX:baudrate=115200 -B uart=tx
```

inspect_capture створює нові вікна у вказаному каталозі; збережені докази не перезаписуються цими командами. Під час пауз симулятора модельний час стоїть. Вхід lux та орієнтація змінювалися незалежно, тому записи не доводять залежності світла від кута.
