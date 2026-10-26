# Збірка LightBench у Linux

Проєкт збирається з кореня командою `make build`. Docker запускає ARM toolchain Ubuntu 24.04 і створює ELF/HEX/BIN у firmware/build. `make test-linux` виконує ті самі host-тести в Linux-контейнері. Перевірено локальний Docker; окрема Ubuntu VM під час цього виконання була недоступна через SSH і не використана як джерело результатів.

Перед передаванням на інший комп’ютер скопіюйте весь репозиторій або створіть локальний Git bundle. Запустіть `make clean`, `make build`, `make test-linux`. Не переносіть старий каталог build як доказ нової збірки. Відкрийте корінь у VS Code й виконайте `Wokwi: Start Simulator` із власною чинною ліцензією.
