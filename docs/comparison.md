# LightBench — порівняння ЛР3/ЛР4

Парні Wokwi-прогони13.11.2026, C031C6,48МГц,1152008N1. Виміряна ADC-independent ЛР3 ELF962ff5e4…; фінальна RTOS ELF2a55d386… . Fine-time порівняння використовує однакові GPIO/VCD endpoints. Внутрішній час обох продуктів має крок1мс.

| Метрика | Super loop | FreeRTOS1000Гц | Висновок |
|---|---:|---:|---|
| S3 реакція max,мс,debounce включено |41.092|34.281|Обидва виконують власний FR; RTOS максимум менший.|
| S3 ADC/IMU max interval,мс |27.661/27.661|20.050/20.050|Номінальна періодичність RTOS рівномірніша.|
| S3 CRC-знімки |304 valid|323 valid|Узгодженість збережена.|
| SDA LOW IMU gap,мс |3019.932|3023.578|ОС не усуває електричну відмову.|
| Flood RX/TX loss |4/34|17/149|RTOS гірший; TX2048→1024, вплив лише ОС не ізольований.|
| Flash,байти |27780|25756|Різниця також зумовлена форматувачем/UART, не лише ОС.|
| RAM із резервами,байти |10128|11496|+1368, для RTOS лишається792.|
| S2 неперервно,мс MCU |3606022|3606011|Обидва>=3600000,поодномувалідномуrun.|
| S2 реакція min/avg/max,мс |29.176/30.308/35.450|29.414/31.018/33.849|RTOS середня трохи гірша, максимум менший.|
| S2 RAM CRC32 |7f04a102|2d1281d8|80точок,64RAM,16overwritten,обидваvalid.|
| FR-12 |failed|failed|Flashfault; RAM CRC не є persistence.|

Гіпотези: (1) gapIMU<40мс під трисекундним SDA LOW — спростована; (2) додаткова затримка понад30мс менша на30% — підтверджена,S3зниження61.41%; (3) менші UARTвтрати — лише частково, floodгірший; (4) додатковаRAMвиміряна,+1368,CRCзбережено; (5) blackoutнасправномуносії — environment-limited,непідтверджено; (6) >=303узгодженихзнімки —323valid,підтверджено.

Окремі kernel diagnostics:30mutexі30binaryповторів,близько10мспроти50–51мс;40notificationі40ISRпробуджень;VCD71–75мксі75.5мкс зі scope-межами. Starvation1с дала0нижчих/Idle; FIFO100спроб8прийнято92втрачено; stackcheck2hookна5мс,check0continued;heap64malloc-hook. Tick100Гц у продуктовомуS3давmax45.960мс і161validCRC, обидва20-мссенсори збереглися.

Сумарний чистий usefulCPU/OSoverhead не ізольовано: GPIOtaskstats включаєISR/trace, baselineобгорткичасткові. TIM3методвідхилений; fineVCDіcoarseHALнепідміняються. Exactraw/ELF/input/toolhashes і межі: власний Lab04/Work/frozen/manifest.json, runs/*/{analysis.json,S2.result.json,S2.analysis.json}, results/comparison-short.json, results/tick-comparison.json.

У кінцевому OS-звіті перевантажувального sweep `sample_lost=220`: відкинуті копії Acquisition у продуктовій FIFO, окремо від UART-втрат17/149. Точний розподіл між ступенями не встановлено через асинхронні metrics/reset. У номінальних S1/S3/S2 sample_lost=0. Це виміряна межа перевантажувального режиму; вона не поширюється на номінальні S1/S3/S2.
