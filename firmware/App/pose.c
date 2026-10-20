#include "pose.h"
#include <math.h>

uint16_t pose_angle(const int32_t mg[3]) {
    /* Bound before squaring: malformed host input must not overflow int64. */
    for (unsigned i = 0; i < 3; ++i)
        if (mg[i] < -1150 || mg[i] > 1150) return ANGLE_INVALID;
    int64_t squared = (int64_t)mg[0] * mg[0] +
                      (int64_t)mg[1] * mg[1] + (int64_t)mg[2] * mg[2];
    if (squared < 722500 || squared > 1322500) return ANGLE_INVALID;
    /* The validated squared norm fits uint32. Avoid 64-bit-to-float helpers. */
    float cosine = mg[2] / sqrtf((float)(uint32_t)squared);
    if (cosine > 1.0f) cosine = 1.0f;
    if (cosine < -1.0f) cosine = -1.0f;
    return (uint16_t)lroundf(acosf(cosine) * 5729.5779513f);
}
