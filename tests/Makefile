CC ?= cc
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD ?= build/$(PLATFORM)
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -Werror
CPPFLAGS += -I../firmware/Devices -I../firmware/App -I../firmware/Platform -Ifakes
APP = ../firmware/App/bench.c ../firmware/App/pose.c ../firmware/App/records.c ../firmware/App/protocol.c ../firmware/App/point_store.c
DEVICE = ../firmware/Devices/mpu6050.c
RUNTIME = ../firmware/Platform/runtime.c
SUITES = test_bench test_protocol test_mpu test_runtime test_point_store
HEADERS = $(wildcard ../firmware/App/*.h ../firmware/Devices/*.h ../firmware/Platform/*.h fakes/*.h)
.PHONY: test sanitize clean
test: $(SUITES:%=$(BUILD)/%)
	@set -e; for suite in $(SUITES); do ./$(BUILD)/$$suite; done
$(BUILD):
	mkdir -p "$@"
$(BUILD)/test_runtime: test_runtime.c $(APP) $(DEVICE) $(RUNTIME) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -lm -o "$@"
$(BUILD)/%: %.c $(APP) $(DEVICE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -lm -o "$@"
sanitize:
	$(MAKE) test BUILD=build/$(PLATFORM)/sanitize CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
clean:
	rm -rf build
