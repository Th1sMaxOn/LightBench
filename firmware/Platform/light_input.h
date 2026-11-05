#ifndef LIGHTBENCH_LIGHT_INPUT_H
#define LIGHTBENCH_LIGHT_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *context;
    bool (*prepare)(void *context);
    bool (*start)(void *context);
    bool (*complete)(void *context, uint16_t *raw);
    bool (*stop)(void *context);
    void (*discard)(void *context);
    void (*marker)(void *context, bool active);
} LightInputOps;

typedef struct {
    LightInputOps ops;
    bool ready;
} LightInput;

void light_input_open(LightInput *input, LightInputOps ops);
bool light_input_sample(LightInput *input, uint16_t *raw);
#endif
