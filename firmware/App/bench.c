#include "bench.h"
#include <string.h>

void bench_init(LightBench *bench) {
    memset(bench, 0, sizeof(*bench));
    bench->pose.angle_cdeg = ANGLE_INVALID;
}
BenchResult bench_run(LightBench *bench, uint32_t now) {
    if (bench->running) return BENCH_RUNNING;
    if (!bench->light.valid) return BENCH_ADC_BAD;
    if (bench->series == UINT32_MAX) return BENCH_IDS_FULL;
    memset(&bench->stats, 0, sizeof(bench->stats));
    bench->stats.raw_min = UINT16_MAX;
    ++bench->series;
    bench->started_ms = now;
    bench->running = true;
    return BENCH_OK;
}
BenchResult bench_stop(LightBench *bench, uint32_t now) {
    if (!bench->running) return BENCH_STOPPED;
    bench->running = false;
    bench->stopped_ms = now;
    return BENCH_OK;
}
bool bench_expire(LightBench *bench, uint32_t now) {
    if (!bench->running || (uint32_t)(now - bench->started_ms) < SERIES_LIMIT_MS) return false;
    (void)bench_stop(bench, now);
    return true;
}
void bench_light(LightBench *bench, uint16_t raw, bool ok, uint32_t now) {
    ok = ok && raw <= 4095u;
    if (bench->running) {
        SeriesStats *s = &bench->stats;
        if (ok) {
            if (s->light_count && (uint32_t)(now - s->last_light_ms) > 40u) ++s->light_gaps;
            s->last_light_ms = now;
            ++s->light_count;
            s->raw_sum += raw;
            if (raw < s->raw_min) s->raw_min = raw;
            if (raw > s->raw_max) s->raw_max = raw;
        } else {
            ++s->light_failures;
            (void)bench_stop(bench, now);
        }
    }
    bench->light = (LightSample){now, raw, ok};
}
void bench_pose(LightBench *bench, const int32_t mg[3], bool ok, uint32_t now) {
    uint16_t angle = ok ? pose_angle(mg) : ANGLE_INVALID;
    if (bench->running) {
        SeriesStats *s = &bench->stats;
        if (ok) {
            if (s->pose_count && (uint32_t)(now - s->last_pose_ms) > 40u) ++s->pose_gaps;
            s->last_pose_ms = now;
            ++s->pose_count;
            if (angle != ANGLE_INVALID) { ++s->angle_count; s->angle_sum += angle; }
        } else ++s->pose_failures;
    }
    bench->pose = (PoseSample){now, angle, ok};
}
ReferencePoint bench_observe(const LightBench *bench, uint32_t now) {
    uint32_t light_age = now - bench->light.time_ms, pose_age = now - bench->pose.time_ms;
    uint32_t skew = light_age > pose_age ? light_age - pose_age : pose_age - light_age;
    uint16_t flags = 0;
    if (!bench->light.valid) flags |= POINT_LIGHT_BAD;
    if (!bench->pose.valid) flags |= POINT_POSE_BAD;
    if (light_age > 40u) flags |= POINT_LIGHT_OLD;
    if (pose_age > 40u) flags |= POINT_POSE_OLD;
    if (skew > 20u) flags |= POINT_SKEW;
    if (bench->pose.valid && bench->pose.angle_cdeg == ANGLE_INVALID) flags |= POINT_DYNAMIC;
    return (ReferencePoint){bench->sequence, bench->series, now,
        bench->light.time_ms, bench->pose.time_ms,
        bench->light.valid ? bench->light.raw : UINT16_MAX,
        bench->pose.angle_cdeg, flags, 0};
}
BenchResult bench_mark(LightBench *bench, uint32_t now, ReferencePoint *point) {
    if (!bench->running) return BENCH_STOPPED;
    if (bench->sequence == UINT32_MAX) return BENCH_IDS_FULL;
    ++bench->sequence;
    *point = bench_observe(bench, now);
    records_append(&bench->history, *point);
    return BENCH_OK;
}
bool bench_button(LightBench *bench, bool pressed, uint32_t now) {
    if (pressed != bench->button_candidate) {
        bench->button_candidate = pressed;
        bench->button_changed_ms = now;
    }
    if (bench->button_candidate == bench->button_stable ||
        (uint32_t)(now - bench->button_changed_ms) < 30u) return false;
    bench->button_stable = bench->button_candidate;
    return bench->button_stable;
}
