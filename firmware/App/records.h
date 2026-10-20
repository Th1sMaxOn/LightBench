#ifndef LIGHTBENCH_RECORDS_H
#define LIGHTBENCH_RECORDS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define POINT_CAPACITY 64u
enum PointFlags {
    POINT_LIGHT_BAD = 1, POINT_POSE_BAD = 2, POINT_LIGHT_OLD = 4,
    POINT_POSE_OLD = 8, POINT_SKEW = 16, POINT_DYNAMIC = 32
};
typedef struct {
    uint32_t sequence, series, mark_ms, light_ms, pose_ms;
    uint16_t raw, angle_cdeg, flags, reserved;
} ReferencePoint;
typedef struct {
    ReferencePoint items[POINT_CAPACITY];
    uint32_t overwritten;
    uint16_t head, count;
} PointHistory;
typedef struct {
    ReferencePoint items[POINT_CAPACITY];
    uint32_t overwritten;
    uint16_t count;
} PointSnapshot;
void records_append(PointHistory *history, ReferencePoint point);
void records_snapshot(const PointHistory *history, PointSnapshot *snapshot);
uint32_t records_crc(const ReferencePoint *points, size_t count);
/* Unfinalized CRC state. Start at UINT32_MAX, XOR UINT32_MAX at the end. */
uint32_t records_crc_append(uint32_t crc, const ReferencePoint *point);
#endif
