#include "light_input.h"

void light_input_open(LightInput *input, LightInputOps ops) {
    input->ops = ops;
    input->ready = ops.prepare(ops.context);
    if (!input->ready) ops.discard(ops.context);
}

bool light_input_sample(LightInput *input, uint16_t *raw) {
    if (!input || !raw) return false;
    LightInputOps *ops = &input->ops;
    if (!input->ready) {
        input->ready = ops->prepare(ops->context);
        if (!input->ready) {
            ops->discard(ops->context);
            return false;
        }
    }
    uint16_t observed = 0;
    ops->marker(ops->context, true);
    bool started = ops->start(ops->context);
    bool converted = started && ops->complete(ops->context, &observed);
    bool stopped = ops->stop(ops->context);
    ops->marker(ops->context, false);
    if (!(converted && stopped && observed <= 4095u)) {
        ops->discard(ops->context);
        input->ready = false;
        return false;
    }
    *raw = observed;
    return true;
}
