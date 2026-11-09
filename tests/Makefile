CC ?= cc
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD ?= build/$(PLATFORM)
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Werror
CPPFLAGS += -I../firmware/Devices -I../firmware/App -I../firmware/Platform -Ifakes
APP = ../firmware/App/bench.c ../firmware/App/pose.c ../firmware/App/records.c ../firmware/App/protocol.c ../firmware/App/point_store.c
DEVICE = ../firmware/Devices/mpu6050.c
RUNTIME = ../firmware/Platform/runtime.c
SUITES = test_bench test_protocol test_mpu test_runtime test_point_store test_imu_transport test_serial_tx test_light_input test_text_format test_serial_register test_runtime_rtos
HEADERS = $(wildcard ../firmware/App/*.h ../firmware/Devices/*.h ../firmware/Platform/*.h fakes/*.h)
.PHONY: test sanitize clean
test: $(SUITES:%=$(BUILD)/%)
	@set -e; for suite in $(SUITES); do ./$(BUILD)/$$suite; done
$(BUILD):
	mkdir -p "$@"
$(BUILD)/test_text_format: test_text_format.c ../firmware/Platform/text_format.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o "$@"
$(BUILD)/test_runtime: test_runtime.c $(APP) $(DEVICE) $(RUNTIME) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -lm -o "$@"
$(BUILD)/test_imu_transport: test_imu_transport.c ../firmware/Platform/imu_transport.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o "$@"
$(BUILD)/test_light_input: test_light_input.c ../firmware/Platform/light_input.c $(APP) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -lm -o "$@"
$(BUILD)/test_serial_tx: test_serial_tx.c ../firmware/Platform/serial_tx.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -o "$@"
$(BUILD)/test_runtime_rtos: test_runtime_rtos.c test_runtime.c $(APP) $(DEVICE) $(RUNTIME) ../firmware/Platform/text_format.c $(HEADERS) | $(BUILD)
	$(CC) -Ifakes/rtos $(CPPFLAGS) $(CFLAGS) -DLAB4 $(filter-out test_runtime.c,$(filter %.c,$^)) -lm -o "$@"
$(BUILD)/test_serial_register: test_serial_register.c ../firmware/Platform/serial_tx.c $(HEADERS) fakes/uart/main.h | $(BUILD)
	$(CC) -Ifakes/uart $(CPPFLAGS) $(CFLAGS) -DLAB4 $(filter %.c,$^) -o "$@"
$(BUILD)/%: %.c $(APP) $(DEVICE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -lm -o "$@"
sanitize:
	$(MAKE) test BUILD=build/$(PLATFORM)/sanitize CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
clean:
	rm -rf build
