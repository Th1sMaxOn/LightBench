# Компоненти LightBench

Пашко Максим Ігорович, ІО-32. Середовище: Ubuntu 24.04, Docker, STM32 HAL, Wokwi. Схема форми B має LDR ADC, MPU6050 I²C і кнопку GPIO. Деталі монтажу в [pinout](pinout.md).

| Вузол | Роль, режим, обмеження | Джерело |
|---|---|---|
| STM32C031C6 | M0+, 48 МГц, 32 КіБ flash/12 КіБ RAM; 4 КіБ зарезервовано під ЛР3 | [ST DS13867 Rev4](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf), с.1, §4 |
| MPU6050 | WHO_AM_I, wake/config, accel ±2g; 14-byte burst, ±250°/с для супутніх полів драйвера. Положення з тяжіння придатне лише у спокої | [TDK register map Rev4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map.pdf), регістри 25–28, 59–72, 107, 117; [Wokwi](https://docs.wokwi.com/parts/wokwi-mpu6050) |
| LDR-модуль | AO до ADC, код 0..4095. Wokwi задає lux як вхід моделі; прошивка не вимірює калібровані люкси | [Wokwi LDR](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor) |
| Кнопка | Два контакти, 3V3 і PA1 з pulldown; debounce 30 мс у App. Утримання не повторює точку | [Wokwi button](https://docs.wokwi.com/parts/wokwi-pushbutton) |
| UART/LED | USART2 115200 8N1, PA5 активної серії, IT TX/RX | [Wokwi Nucleo](https://docs.wokwi.com/parts/board-st-nucleo-c031c6) |

MPU signed big-endian accel ділиться на 16384 LSB/g; драйвер дає mg, кут обчислюється окремо. Наприклад 0x4000 по Z означає 1000 mg і θ=0°. Вектор 0/0/0 не означає 0°, він непридатний. Похибка квантування mg не є метрологічною точністю.

У дільнику моделі LDR більше світла зменшує AO. Напрям і значення перевіряються власним запуском; параметри rl10/gamma моделі не є калібруванням фізичного GM5528. Положення MPU й lux LDR змінюються незалежно.

Фізичний аналог кнопки є пасивним контактом. Конкретний виробник LDR/breakout не обраний, його піковий струм і допуски не підтверджені. [TDK datasheet](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf), §6, визначає VDD чіпа 2.375..3.46 В, typical 3.8 мА для accel+gyro. Параметри чіпа не замінюють datasheet модуля. Форма B підтверджує модель, не електричну придатність довільного фізичного виробу.

BH1750 відхилено, бо потрібний raw аналогового каналу; потенціометр не реагує на світло. Контакт нахилу не дає числового положення; дисплей не потрібний для CSV. Камера, SPI та PWM до ЛР2 цього продукту не входять.
