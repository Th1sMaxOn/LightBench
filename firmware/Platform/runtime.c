#include "runtime.h"
#include "board.h"
#include "main.h"
#include "bench.h"
#include "protocol.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef BUILD_ID
#define BUILD_ID "development"
#endif
static LightBench bench;
static PointSnapshot snapshot;
static CommandReader reader;
static Mpu6050 imu;
static bool imu_ready, exporting;
static unsigned export_row, export_stage;
static uint32_t export_crc;
static uint32_t last_adc, last_imu, last_retry, last_status;
static uint32_t rx_losses, tx_losses;

static bool line(const char *format, ...) {
    char bytes[384];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(bytes, sizeof(bytes), format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(bytes) || board_tx_free() < (size_t)n) {
        ++tx_losses;
        return false;
    }
    for (int i = 0; i < n; ++i) (void)board_put_char(bytes[i]);
    return true;
}
static const char *result_name(BenchResult result) {
    static const char *names[] = {"ok", "already_running", "not_running", "adc_unavailable", "ids_exhausted"};
    return names[result];
}
static const char *angle_text(uint16_t angle, char text[16]) {
    if (angle == ANGLE_INVALID) return "invalid";
    (void)snprintf(text, 16, "%u", (unsigned)angle);
    return text;
}
static void status(uint32_t now) {
    char angle[16];
    ReferencePoint p = bench_observe(&bench, now);
    (void)line("status ms=%lu series=%lu running=%u adc=%u raw=%u light_ms=%lu imu=%u angle_cdeg=%s pose_ms=%lu flags=%u points=%u overwritten=%lu rx_lost=%lu tx_lost=%lu store=ram_volatile\r\n",
        (unsigned long)now, (unsigned long)bench.series, bench.running,
        bench.light.valid, bench.light.raw, (unsigned long)bench.light.time_ms,
        bench.pose.valid, angle_text(bench.pose.angle_cdeg, angle),
        (unsigned long)bench.pose.time_ms, p.flags, bench.history.count,
        (unsigned long)bench.history.overwritten, (unsigned long)rx_losses, (unsigned long)tx_losses);
}
static void summary(const char *reason) {
    SeriesStats *s = &bench.stats;
    char angle[16], raw[16];
    const char *raw_mean = "empty";
    if (s->light_count) {
        (void)snprintf(raw, sizeof(raw), "%lu", (unsigned long)(s->raw_sum / s->light_count));
        raw_mean = raw;
    }
    (void)line("summary series=%lu reason=%s adc_n=%lu pose_n=%lu angle_n=%lu mean_raw=%s min_raw=%u max_raw=%u mean_cdeg=%s\r\n",
        (unsigned long)bench.series, reason, (unsigned long)s->light_count,
        (unsigned long)s->pose_count, (unsigned long)s->angle_count,
        raw_mean, s->light_count ? s->raw_min : 0u, s->light_count ? s->raw_max : 0u,
        angle_text(s->angle_count ? (uint16_t)(s->angle_sum / s->angle_count) : ANGLE_INVALID, angle));
    (void)line("quality adc_fail=%lu imu_fail=%lu adc_gaps=%lu pose_gaps=%lu\r\n",
        (unsigned long)s->light_failures, (unsigned long)s->pose_failures,
        (unsigned long)s->light_gaps, (unsigned long)s->pose_gaps);
}
static void sample_adc(void) {
    uint16_t raw = 0;
    bool was_running = bench.running;
    bool ok = board_read_light(&raw);
    bench_light(&bench, raw, ok, HAL_GetTick());
    if (was_running && !bench.running) summary("adc_fault");
}
static void sample_pose(void) {
    ImuSample sample = {0};
    board_imu_marker(true);
    BusResult result = mpu6050_read(&imu, &sample);
    board_imu_marker(false);
    bench_pose(&bench, sample.accel_mg, result == BUS_OK, HAL_GetTick());
    if (result != BUS_OK) {
        imu_ready = false;
        last_retry = HAL_GetTick();
        (void)line("fault=imu bus=%u ms=%lu\r\n", result, (unsigned long)last_retry);
    }
}
static void execute(BenchCommand cmd, uint32_t now) {
    switch (cmd) {
    case CMD_RUN:
        (void)line("reply=run result=%s ms=%lu\r\n", result_name(bench_run(&bench, now)), (unsigned long)now);
        break;
    case CMD_STOP: {
        BenchResult result = bench_stop(&bench, now);
        (void)line("reply=stop result=%s ms=%lu\r\n", result_name(result), (unsigned long)now);
        if (result == BENCH_OK) summary("command");
        break;
    }
    case CMD_STATUS: status(now); break;
    case CMD_EXPORT:
        if (exporting) (void)line("reply=export result=busy\r\n");
        else {
            records_snapshot(&bench.history, &snapshot);
            export_crc = UINT32_MAX;
            exporting = true; export_row = 0; export_stage = 0;
        }
        break;
    case CMD_SELFTEST: {
        BusResult result = mpu6050_init(&imu);
        imu_ready = result == BUS_OK;
        last_retry = HAL_GetTick();
        if (imu_ready) sample_pose();
        else bench_pose(&bench, NULL, false, HAL_GetTick());
        sample_adc();
        (void)line("selftest adc=%u imu=%u button=ready bus=%u ms=%lu\r\n",
            bench.light.valid, imu_ready, result, (unsigned long)HAL_GetTick());
        break;
    }
    case CMD_ADDR68: case CMD_ADDR69:
        imu.address = cmd == CMD_ADDR68 ? 0x68 : 0x69;
        imu_ready = false;
        last_retry = now - 1000u;
        bench_pose(&bench, NULL, false, now);
        (void)line("reply=imuaddr address=%u\r\n", imu.address);
        break;
    case CMD_UNKNOWN: case CMD_BAD_LINE:
        (void)line("reply=error reason=%s\r\n", cmd == CMD_UNKNOWN ? "unknown_command" : "invalid_line");
        break;
    default: break;
    }
}
static void export_step(void) {
    /* Leave room for a status/point response. No sampling waits on TX. */
    if (!exporting || board_tx_free() < 768u) return;
    if (export_stage == 0) {
        (void)line("export_begin count=%u overwritten=%lu format=LB1_le_28 crc=IEEE\r\nseq,series,mark_ms,light_ms,pose_ms,raw,angle_cdeg,flags,reserved\r\n",
            snapshot.count, (unsigned long)snapshot.overwritten);
        export_stage = 1;
    } else if (export_row < snapshot.count) {
        const ReferencePoint *p = &snapshot.items[export_row++];
        export_crc = records_crc_append(export_crc, p);
        (void)line("%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%u\r\n",
            (unsigned long)p->sequence, (unsigned long)p->series, (unsigned long)p->mark_ms,
            (unsigned long)p->light_ms, (unsigned long)p->pose_ms,
            p->raw, p->angle_cdeg, p->flags, p->reserved);
    } else {
        (void)line("export_end count=%u crc32=%08lx\r\n", snapshot.count, (unsigned long)(export_crc ^ UINT32_MAX));
        exporting = false;
    }
}
void runtime_init(void) {
    bench_init(&bench);
    memset(&reader, 0, sizeof(reader));
    snapshot.count = 0;
    exporting = false; rx_losses = tx_losses = 0;
    board_init();
    imu = (Mpu6050){board_sensor_bus(), 0x68};
    imu_ready = mpu6050_init(&imu) == BUS_OK;
    last_retry = HAL_GetTick();
    sample_adc();
    if (imu_ready) sample_pose();
    last_adc = last_imu = last_status = HAL_GetTick();
    (void)line("project=LightBench build=%s adc=%u imu=%u button=ready store=ram_volatile uart=115200_8N1\r\n",
        BUILD_ID, bench.light.valid, imu_ready);
    board_led(false);
}
void runtime_poll(void) {
    uint32_t now = HAL_GetTick();
    if (bench_expire(&bench, now)) summary("time_limit");
    if (board_rx_overflow()) { ++rx_losses; protocol_overflow(&reader); }
    for (unsigned budget = 0; budget < 64; ++budget) {
        int ch = board_get_char();
        if (ch < 0) break;
        BenchCommand cmd = protocol_feed(&reader, ch);
        if (cmd != CMD_NONE) execute(cmd, HAL_GetTick());
    }
    now = HAL_GetTick();
    if ((uint32_t)(now - last_adc) >= 20u) { last_adc = now; sample_adc(); }
    now = HAL_GetTick();
    if (!imu_ready && (uint32_t)(now - last_retry) >= 1000u) {
        last_retry = now;
        BusResult result = mpu6050_init(&imu);
        imu_ready = result == BUS_OK;
        (void)line("probe=imu result=%u ms=%lu\r\n", result, (unsigned long)HAL_GetTick());
    }
    now = HAL_GetTick();
    if (imu_ready && (uint32_t)(now - last_imu) >= 20u) { last_imu = now; sample_pose(); }
    now = HAL_GetTick();
    if (bench_button(&bench, board_button(), now)) {
        ReferencePoint p;
        BenchResult result = bench_mark(&bench, now, &p);
        if (result == BENCH_OK) {
            board_point_marker();
            (void)line("point seq=%lu series=%lu ms=%lu edge_ms=%lu raw=%u angle_cdeg=%u flags=%u light_ms=%lu pose_ms=%lu\r\n",
                (unsigned long)p.sequence, (unsigned long)p.series, (unsigned long)p.mark_ms,
                (unsigned long)bench.button_changed_ms, p.raw, p.angle_cdeg, p.flags,
                (unsigned long)p.light_ms, (unsigned long)p.pose_ms);
        } else (void)line("reply=point result=%s\r\n", result_name(result));
    }
    if (board_tx_overflow()) ++tx_losses;
    if ((uint32_t)(now - last_status) >= 1000u) { last_status = now; status(now); }
    board_led(bench.running);
    export_step();
}
