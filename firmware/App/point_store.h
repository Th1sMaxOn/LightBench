#ifndef LIGHTBENCH_POINT_STORE_H
#define LIGHTBENCH_POINT_STORE_H
#include "records.h"
#define STORE_PAGE_BYTES 2048u
#define STORE_FRAME_BYTES 32u
#define STORE_QUEUE 8u
/* Offsets are relative to the separately reserved 4 KiB. */
typedef struct {
    void *context;
    void (*read)(void *, unsigned, void *, unsigned);
    bool (*erase)(void *, unsigned);
    bool (*program)(void *, unsigned, const uint8_t[8]);
} PointFlash;
typedef struct {
    PointFlash flash;
    ReferencePoint queue[STORE_QUEUE];
    uint32_t committed, rejected, lost, last_sequence;
    unsigned slot, part, head, count;
    bool failed, erase_needed;
} PointStore;
void point_store_open(PointStore *, PointFlash, PointHistory *, uint32_t *series, uint32_t *sequence);
bool point_store_enqueue(PointStore *, ReferencePoint);
/* One real erase OR one 64-bit program operation per call. */
void point_store_step(PointStore *);
#endif
