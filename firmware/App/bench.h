#ifndef LIGHTBENCH_BENCH_H
#define LIGHTBENCH_BENCH_H
#include "pose.h"
#include "records.h"
#define SERIES_LIMIT_MS 3600000u
typedef struct { uint32_t time_ms; uint16_t raw; bool valid; } LightSample;
typedef struct { uint32_t time_ms; uint16_t angle_cdeg; bool valid; } PoseSample;
typedef struct {
    uint64_t raw_sum, angle_sum;
    uint32_t light_count, pose_count, angle_count;
    uint32_t light_failures, pose_failures, light_gaps, pose_gaps;
    uint32_t last_light_ms, last_pose_ms;
    uint16_t raw_min, raw_max;
} SeriesStats;
typedef enum { BENCH_OK, BENCH_RUNNING, BENCH_STOPPED, BENCH_ADC_BAD, BENCH_IDS_FULL } BenchResult;
typedef struct {
    LightSample light;
    PoseSample pose;
    SeriesStats stats;
    PointHistory history;
    uint32_t series, sequence, started_ms, stopped_ms;
    uint32_t button_changed_ms;
    bool running, button_candidate, button_stable;
} LightBench;
void bench_init(LightBench *bench);
BenchResult bench_run(LightBench *bench, uint32_t now);
BenchResult bench_stop(LightBench *bench, uint32_t now);
bool bench_expire(LightBench *bench, uint32_t now);
void bench_light(LightBench *bench, uint16_t raw, bool ok, uint32_t now);
void bench_pose(LightBench *bench, const int32_t mg[3], bool ok, uint32_t now);
ReferencePoint bench_observe(const LightBench *bench, uint32_t now);
BenchResult bench_mark(LightBench *bench, uint32_t now, ReferencePoint *point);
bool bench_button(LightBench *bench, bool pressed, uint32_t now);
#endif
