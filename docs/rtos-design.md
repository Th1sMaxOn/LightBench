# LightBench — виміряна організація FreeRTOS

Nucleo-C031C6, 48 МГц, Cortex-M0+. Офіційне ядро FreeRTOS V10.5.1 (походження в Middlewares/FreeRTOS/VERSION.json), порт GCC/ARM_CM0, MIT. Без FPU/DWT/BASEPRI; критичні секції ARMv6-M використовують PRIMASK. HAL працює з TIM14 1 мс, ОС — із SysTick 1000 Гц. Для порівняння створено окрему збірку 100 Гц.

| Задача | Пріоритет | Стек, слів / байтів | Робота |
|---|---:|---:|---|
| owner | 2 | 384 / 1536 | Один власник LightBench, history, snapshot, PointStore та TX. UART і кнопка перевіряються раз на tick, потім vTaskDelay(1). |
| adc | 3 | 128 / 512 | xTaskDelayUntil, 20 мс; незалежний ADC, копія Acquisition у FIFO. |
| imu | 3 | 192 / 768 | xTaskDelayUntil, 20 мс; один власник I²C; fault/retry не частіше 1000 мс. |
| Idle | 0 | 80 / 320 | Службова задача, без підтвердженої економії енергії. |

ADC/IMU → FIFO8×28 байтів → owner → RAM/CSV/CRC/покроковий журнал. Owner → FIFO8 керувальних байтів → IMU. Черги переносять копії, порядок команд збережено; переповнення явно обліковуються. Пріоритет сенсорів захищає FR-02/FR-11; відгук owner перевіряє FR-06/FR-13. Mutex для одноосібно керованих history та I²C не потрібний.

Задачі, TCB і черги створюються статично до scheduler; продуктовий heap ОС 0. Увімкнено preemption, time slicing, trace, runtime stats uint64, stack check2 та assert. Вимкнено software timers, recursive/counting semaphores, dynamic allocation, newlib reentrancy та стандартне форматування статистики. Diagnostic ELF окремо використовує heap_4, mutex/binary semaphore, notification/FromISR та malloc hook; це не повний продукт.

USART2 IRQ3 обслуговує фіксований 115200 8N1 transport, RX256/TX1024 поверх HAL-init. Звичайний продуктовий ISR не викликає API ОС: записує байт/error і перший irq_at. Owner опитує кільце. IRQ→owner latency включає очікування owner; окремий diagnostic вимірює контрольоване пробудження. TIM3 спільний high-resolution counter виключено через немонотонний результат контрольного тесту; внутрішні us=HALms×1000, роздільність1мс. Основний fine-time метод — незалежні GPIO/VCD. PC6/PC7 кодують поточну задачу; diagnostic PB3 — notification/ISR signal, PB4 — тіло tick.

Виміряні корекції: нижчий пріоритет IMU спричиняв timeout; owner1280 overflow у S3 →1536; після зупинки delayed-list за UART notifications перейдено на bounded polling, точна першопричина не встановлена. Перший кандидат29696/28672 Flash не вмістився; статичні об'єкти, обмежений LB1-форматувач і короткий UART IRQ дали робочий продукт. MCU й журнал4КіБ збережено.

Фінальний ELF SHA-256 `2a55d38645690ffe0324aa112e727fba6e55c5ad78bad0fcdf9d2c9e638ad36a`, BuildID `afd7317-1b2b3e0afe71`: Flash25756/28672, RAM11496/12288 включно зі стеками, вільно792. Стеки вже в .bss, не додавати їх повторно. S3:80/80подій,323CRCvalid,maxреакції34.281мс. Парні S2 13.11.2026: baseline3606022мс MCU,RTOS3606011мс,80подій/64RAM/16overwritten в обох, CRCvalid. FR-12 failed: committed0,flashfault,reset/power-loss на справному носії не доведено. Висновок і порівняння — [comparison.md](comparison.md); firmware identities, входи й аналіз пов’язані через Lab04/Work/frozen/manifest.json.
