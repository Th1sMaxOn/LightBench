#include "records.h"

void records_append(PointHistory *history, ReferencePoint point) {
    history->items[history->head] = point;
    history->head = (uint16_t)((history->head + 1u) % POINT_CAPACITY);
    if (history->count < POINT_CAPACITY) ++history->count;
    else if (history->overwritten != UINT32_MAX) ++history->overwritten;
}

static uint32_t crc_word(uint32_t crc, uint32_t word, unsigned bytes) {
    for (unsigned b = 0; b < bytes; ++b) {
        crc ^= word & 255u;
        word >>= 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc;
}
uint32_t records_crc(const ReferencePoint *points, size_t count) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < count; ++i) {
        const ReferencePoint *p = &points[i];
        crc = crc_word(crc, p->sequence, 4);
        crc = crc_word(crc, p->series, 4);
        crc = crc_word(crc, p->mark_ms, 4);
        crc = crc_word(crc, p->light_ms, 4);
        crc = crc_word(crc, p->pose_ms, 4);
        crc = crc_word(crc, p->raw, 2);
        crc = crc_word(crc, p->angle_cdeg, 2);
        crc = crc_word(crc, p->flags, 2);
        crc = crc_word(crc, p->reserved, 2);
    }
    return crc ^ UINT32_MAX;
}
void records_snapshot(const PointHistory *history, PointSnapshot *snapshot) {
    snapshot->count = history->count;
    snapshot->overwritten = history->overwritten;
    unsigned first = (history->head + POINT_CAPACITY - history->count) % POINT_CAPACITY;
    for (unsigned i = 0; i < history->count; ++i)
        snapshot->items[i] = history->items[(first + i) % POINT_CAPACITY];
    snapshot->crc = records_crc(snapshot->items, snapshot->count);
}
