# Походження основи

Core, HAL/CMSIS, startup та базова конфігурація походять із дозволеного SP-Boilerplate та [wokwi/stm32-hello-wokwi](https://github.com/wokwi/stm32-hello-wokwi). Ліцензії залежностей збережені в репозиторії. [Vendor manifest](vendor-manifest.json) фіксує точні хеші Drivers.

Порт плати й MPU6050 адаптовано з власної попередньої реалізації. App/bench, pose, records, protocol та відповідні тести описують LightBench. Спільна vendor-основа не є прикладною логікою продукту. Схема містить LDR, IMU, кнопку й окремі діагностичні маркери.
