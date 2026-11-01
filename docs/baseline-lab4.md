# Базова лінія для ЛР4

**Архівний аналіз UART, 01.11.2026.** За `docs/captures/lab3/uart-sweep.vcd.gz` єдині команди run і stop однозначно зіставлено з відповідями. Від останнього стоп-біта RX до початку відповіді TX: 0,615269 і 0,640394 мс; до кінця відповіді: 4,927275 і 5,254100 мс відповідно. Це вимірювання на лінії моделі, не окремий час виконання коду; команда stop могла очікувати в черзі. При декодуванні 0 помилок стоп-біта. Status має також періодичні відповіді і не піддається однозначному зіставленню. Нового quiet-drain досліду, утримання SDA LOW та запуску з неготовим датчиком не виконано.

**Актуалізація.** 30.10.2026: фактичні результати й незмінний ELF наведені в docs/lab3-report.md та Lab03/Work/systick-20261029/evidence-manifest-final.json. S1/S2/S3, NACK, sampling і UART sweep виконано; SDA LOW, фізичний power-loss та підтверджене збереження flash залишилися відкритими. Нижчі позначки prepared_not_run є історією підготовки.

Стан 28.10.2026: інструментарій і сценарії підготовлено, нові S1–S3 ще не виконано. Цей файл не підтверджує готовність ЛР3. RTOS-код не додавався.

## Відтворення

Стенд STM32C031C6 48 МГц, USART2 115200 8N1, LDR PA0, кнопка PA1, MPU6050 PB8/PB9, адреса 0x68. Базовий diagram.json зберігає схему ЛР2. diagram-lab3.json змінює лише підключення проб аналізатора: D0 FLASH PA6, D1 SDA, D2 UART_TX, D3 BUTTON, D4 ADC_MARK, D5 UART_RX PA3, D6 POINT PB0, D7 IMU_MARK PB1.

```sh
make build
make test
make sanitize
python3 tools/lab3_scenarios.py docs/metrics/lab3/scenarios
python3 tools/lab3_diagram.py
```

За наявності чинного WOKWI_CLI_TOKEN:

```sh
python3 tools/lab3_run.py docs/metrics/lab3/scenarios/S1.yaml docs/metrics/lab3/S1 --seconds 75 --vcd
python3 tools/lab3_run.py docs/metrics/lab3/scenarios/S2-hour.yaml docs/metrics/lab3/S2-hour --seconds 3620
python3 tools/lab3_run.py docs/metrics/lab3/scenarios/S3.yaml docs/metrics/lab3/S3 --seconds 85 --vcd
python3 tools/lab3_run.py docs/metrics/lab3/scenarios/fault-recovery.yaml docs/metrics/lab3/fault --seconds 15 --vcd
python3 tools/lab3_run.py docs/metrics/lab3/scenarios/sampling-sweep.yaml docs/metrics/lab3/sweep --seconds 65
python3 tools/lab3_metrics.py docs/metrics/lab3/S1/uart.txt docs/metrics/lab3/S1/results.json
```

Каталог результатів створюється виключно заново. Старі запуски не перезаписуються. origin.json містить ELF/HEX, diagram, scenario SHA256, parent commit, команду та фактичний exit code. CLI token до логу не потрапляє.

S1: 65 с без серії й подій, щонайменше 3250 періодичних запусків. S2: 80 точок через 45 с зі зміною світла, рідкі status, загалом ≥3605 с часу MCU. Перевірити, що фінальний uptime й elapsed_us справді охоплюють годину, а count узгоджений із періодом. Fake clock у host-тесті не виконує S2.

S3: 40 стабільних натискань у відтворюваних псевдовипадкових фазах, seed 328019, експорт та пачки status. Аналізувати окремо події під час передачі snapshot і під час справжнього flash busy. Якщо busy значно коротший за міжподієвий інтервал, потрібні додаткові натискання всередині busy з VCD-доказом. Сценарій сам по собі цього не доводить.

Fault-recovery через imuaddr 0x69 є NACK відсутньої адреси. Це не заміна утримання SDA чи фізичного від'єднання. Окремо відтворити SDA LOW, відпускання і відновлення; виміряти timeout та вплив на ADC.

Sampling sweep: period 20,10,5,2,1, по 10 с; метрики скидаються перед кожним ступенем. Перша межа, де M_ADC_INTERVAL/M_IMU_INTERVAL max перевищує два періоди або missed>0, є кандидатом точки зламу. Перевірити причину в ширині IMU/flash, а не змінювати FR заднім числом.

Для UART sweep надіслати status зі швидкостями 1,5,10,20,40,80 команд/с під час export. Співставити завершальний LF на UART_RX та останній байт відповідного status на UART_TX. За втрати рядків припинити однозначне парування і повідомити rx_lost/tx_lost. Теоретична межа 11520 байтів/с, нижня межа передачі N байтів 10N/115200 с.

Старт: записати reset і перші ADC/IMU_MARK, окремо перший придатний sample. Банер us починається після налаштування SystemClock, тому це не повний power-on час. Не замінювати reset новим чистим запуском із заново завантаженою flash.

Цілісність: отримати persist щонайменше для 130 точок, export CRC; виконати reset зі збереженням flash, перевірити 64 останні, sequence/series та CRC. Обрив живлення під час кожного з чотирьох doubleword й erase перевірити зі збереженим носієм; UART файл не є носієм пристрою. Host test_point_store перевіряє лише алгоритмічну модель. Якщо Wokwi не моделює flash/reset/power-loss, записати це обмеження й не називати тест пройденим.

## Метрики

METRIC name/n/min_us/avg_us/max_us містить тривалість задачі або інтервал у мікросекундних одиницях, але нова SysTick-збірка має лише крок 1000 мкс. Дрібніші інтервали і GPIO reaction визначаються за VCD, не з цих полів. missed рахує пропущені номінальні випуски, а не число повільних інтервалів. Retry IMU в стані fault не рахується як пропущені справні виміри.

reaction у прошивці починається від опитаного кандидата кнопки, має мс-квантування та включає 30 мс debounce. Фізичну реакцію вимірювати від останнього фронту стабільного BUTTON до зміни POINT. Порівнювати однакову подію та явно вказати відмінність початкового моменту.

busy_us складається з тривалостей, округлених SysTick до 1 мс; накопичена похибка може бути значною, тому відношення busy_us / elapsed_us не використовується як достовірний CPU load. Eligibility, spin та IRQ поза задачею також не враховані. Без WFI CPU виконує цикл майже весь час; VCD-ділянки використовуються лише для вимірювання позначених операцій.

| Метрика | Підтверджено зараз | S1 | S2 | S3 |
|---|---|---|---|---|
| Flash / RAM | Див. build-1.log, sections.txt, symbols.txt | Та сама збірка | Та сама | Та сама |
| Найбільший окремий .su кадр | 408 байтів line, не повний call chain | Не виміряно | Не виміряно | Не виміряно |
| MSP painting / heap watermark | Код додано, runtime значень немає | Немає | Немає | Немає |
| Час задач/циклу, джитер, missed, реакція | Instrumentation додано | Немає | Немає | Немає |
| Flash reset/power-loss на MCU | Алгоритм host пройшов | Немає | Немає | Немає |

Для ЛР4 повторити ці сценарії, baudrate, probe mapping, seed і визначення метрик. Гіпотези та вартість RTOS наведено в limitations.md. Без нових MCU-даних порівняння ЛР3/ЛР4 не має експериментальної бази.
