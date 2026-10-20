#ifndef LIGHTBENCH_POSE_H
#define LIGHTBENCH_POSE_H
#include <stdbool.h>
#include <stdint.h>
#define ANGLE_INVALID UINT16_MAX
uint16_t pose_angle(const int32_t mg[3]);
#endif
