# Підключення LightBench

Nucleo-C031C6, LQFP48; єдина монтажна схема diagram.json. Усі вузли мають спільний GND, логіка 3.3 В.

| MCU | AF / блок | Nucleo | Пристрій / канал аналізатора | Рівні та причина |
|---|---|---|---|---|
| PB8 | AF6 I2C1_SCL | D15, CN5.10 / CN10.3 | MPU SCL / D0 | 4.7 кОм до 3V3, штатна пара I2C |
| PB9 | AF6 I2C1_SDA | D14, CN5.9 / CN10.5 | MPU SDA / D1 | 4.7 кОм до 3V3, open-drain |
| PA0 | ADC1_IN0 | A0, CN8.1 / CN7.28 | LDR AO | 0..3.3 В, analog без pull, окремий канал |
| PA1 | GPIO input | A1, CN8.2 / CN7.30 | Кнопка / D3 | Внутрішній pulldown, кнопка до 3V3, HIGH=pressed |
| PA2 | AF1 USART2_TX | VCP, CN7.37 | Serial RX / D2 | 3.3 В, штатний USART2 |
| PA3 | AF1 USART2_RX | VCP, CN10.10 | Serial TX | 3.3 В, окремий RX |
| PA4 | GPIO output | A2, CN8.3 / CN7.32 | ADC_MARK / D4 | HIGH на час HAL ADC start/read/stop |
| PA5 | GPIO output | D13, CN5.6 / CN10.11 | LED / D5 | Активна серія, бортове навантаження |
| PB0 | GPIO output | A3, CN8.4 / CN7.34 | POINT / D6 | Перемикається при кожній записаній точці |
| PB1 | GPIO output | CN10.24 | IMU_MARK / D7 | HIGH під час читання burst |
| 3V3, GND | Power | CN6.4 / CN6.6 | MPU VCC/GND, LDR VCC/GND | MPU AD0 до GND, INT/XDA/XCL та LDR DO не підключені |

Джерела: [DS13867 Rev4](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf), §4, таблиці pin/AF; [UM2953](https://www.st.com/resource/en/user_manual/um2953-stm32-nucleo64-board-mb1717-stmicroelectronics.pdf), рисунок 7, таблиця 11 та VCP §6.9. У старій таблиці 12 UM2953 підписи SDA/SCL переставлені; використано DS і таблицю 11. PA13/PA14 збережено для SWD.

## Шини та живлення

HSI48 → SYSCLK/HCLK/PCLK 48 МГц, AHB/APB без дільника. I2C TIMINGR=0x20303E5D, PRESC=2, SCLDEL=3, SDADEL=0, SCLH=62, SCLL=93. Програмовані high+low: 157×3/48 МГц=9.8125 мкс плюс фільтри/фронти; ціль 100 кГц. USART2 115200 8N1, oversampling16. ADC PCLK/2=24 МГц, sampling160.5+conversion12.5 cycles, теоретично 7.21 мкс без програмних накладних витрат. Реальні параметри моделі наведені у validation.

MPU AD0=0: 7-бітна адреса 0x68, wire write/read 0xD0/0xD1. WHO_AM_I 0x75=0x68. Ініціалізація 0x6B←01, 0x19←13, 0x1A←03, 0x1B←00, 0x1C←00. DLPF3, 1 кГц/20=50 Гц, ±2 g, 16384 LSB/g. I²C burst читає 14 байтів із 0x3B. 0x69 без AD0 HIGH дає контрольний NACK. Кнопка й LDR цифрового ID не мають.

Бюджет, не вимір: MPU reserve 5 мА, LDR-модуль 5 мА, LED 5 мА, кнопка з pulldown <0.1 мА, дві підтяжки ≤1.4 мА, разом <17 мА, прийнято 25 мА. Робочий типовий струм чіпа MPU accel+gyro 3.8 мА; резерв не є паспортним максимумом breakout. Для плати залишено проєктні 100 мА. За лімітом 3V3 500 мА з UM2953 §6.5.2 плановий запас 375 мА. Піки, точний breakout та струм плати на залізі не перевірені. Симулятор не перевіряє brownout/струм.

Підтяжка 4.7 кОм дає 0.70 мА при LOW. При C=100 пФ оцінка rise≈0.8473RC=0.40 мкс, нижче 1 мкс standard mode; фактичну ємність і фронт треба перевірити на фізичному стенді. LDR AO має бути ≤VDDA, 5-вольтовий AO неприпустимий. Силових навантажень немає.

Відхилені розкладки: LDR і кнопка на PA1 конфліктують між analog/input; SCK SPI1 на PA5 зайняв би LED; UART Arduino D0/D1 належать USART1, не нашому USART2. Внутрішнє сховище ЛР3 не потребує пінів. Якщо знадобиться SPI NOR, схему й pinout буде переглянуто до монтажу.
