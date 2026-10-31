#include "runtime.h"
#include "board.h"
#include "main.h"
#include "bench.h"
#include "protocol.h"
#include "measurement.h"
#include "build_identity.h"
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
static PointStore store;
typedef struct { uint64_t sum; uint32_t n, min, max; } Timing;
enum { M_LOOP, M_ADC, M_IMU, M_RETRY, M_RX, M_BUTTON, M_STORE, M_EXPORT,
       M_STATUS, M_ADC_INTERVAL, M_IMU_INTERVAL, M_REACTION, M_COUNT };
static Timing timing[M_COUNT];
static uint32_t busy_us, epoch_us, adc_previous, imu_previous, missed_adc, missed_imu;
static unsigned report_at, period_ms=20;
static void metric(unsigned id, uint32_t elapsed) {
    Timing *s=&timing[id];
    if(!s->n || elapsed<s->min) s->min=elapsed;
    if(elapsed>s->max) s->max=elapsed;
    s->sum+=elapsed; ++s->n;
}
static uint32_t measured(unsigned id, uint32_t start) {
    uint32_t elapsed=measurement_us()-start;
    metric(id,elapsed); return elapsed;
}
static void interval(unsigned id, uint32_t *previous) {
    uint32_t now=measurement_us();
    if(*previous) metric(id,now-*previous);
    *previous=now;
}
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
    (void)line("status ms=%lu series=%lu running=%u adc=%u raw=%u light_ms=%lu imu=%u angle_cdeg=%s pose_ms=%lu flags=%u points=%u overwritten=%lu rx_lost=%lu tx_lost=%lu store=flash pending=%u committed=%lu rejected=%lu lost=%lu failed=%u\r\n",
        (unsigned long)now, (unsigned long)bench.series, bench.running,
        bench.light.valid, bench.light.raw, (unsigned long)bench.light.time_ms,
        bench.pose.valid, angle_text(bench.pose.angle_cdeg, angle),
        (unsigned long)bench.pose.time_ms, p.flags, bench.history.count,
        (unsigned long)bench.history.overwritten, (unsigned long)rx_losses, (unsigned long)tx_losses,
        store.count,(unsigned long)store.last_sequence,(unsigned long)store.rejected,(unsigned long)store.lost,store.failed);
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
    uint32_t start=measurement_us();
    interval(M_ADC_INTERVAL,&adc_previous);
    uint16_t raw = 0;
    bool was_running = bench.running;
    bool ok = board_read_light(&raw);
    bench_light(&bench, raw, ok, measurement_ms());
    if (was_running && !bench.running) summary("adc_fault");
    (void)measured(M_ADC,start);
}
static void sample_pose(void) {
    uint32_t start=measurement_us();
    interval(M_IMU_INTERVAL,&imu_previous);
    ImuSample sample = {0};
    board_imu_marker(true);
    BusResult result = mpu6050_read(&imu, &sample);
    board_imu_marker(false);
    bench_pose(&bench, sample.accel_mg, result == BUS_OK, measurement_ms());
    if (result != BUS_OK) {
        imu_ready = false;
        last_retry = measurement_ms();
        (void)line("fault=imu bus=%u ms=%lu\r\n", result, (unsigned long)last_retry);
    }
    (void)measured(M_IMU,start);
}
static void execute(BenchCommand cmd, uint32_t now) {
    switch (cmd) {
    case CMD_METRICS: report_at=0; break;
    case CMD_METRICS_RESET:
        memset(timing,0,sizeof(timing)); busy_us=0; epoch_us=measurement_us();
        adc_previous=imu_previous=0; missed_adc=missed_imu=0;
        (void)line("metrics_reset us=%lu\r\n",(unsigned long)epoch_us); break;
    case CMD_PERIOD20: case CMD_PERIOD10: case CMD_PERIOD5: case CMD_PERIOD2: case CMD_PERIOD1: {
        static const unsigned periods[]={20,10,5,2,1};
        period_ms=periods[cmd-CMD_PERIOD20];
        last_adc=last_imu=now;
        (void)line("period_ms=%u\r\n",period_ms); break;
    }
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
        last_retry = measurement_ms();
        if (imu_ready) sample_pose();
        else bench_pose(&bench, NULL, false, measurement_ms());
        sample_adc();
        (void)line("selftest adc=%u imu=%u button=ready bus=%u ms=%lu\r\n",
            bench.light.valid, imu_ready, result, (unsigned long)measurement_ms());
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
    point_store_open(&store,board_point_flash(),&bench.history,&bench.series,&bench.sequence);
    memset(timing,0,sizeof(timing)); report_at=M_COUNT+1;
    busy_us=0; epoch_us=measurement_us();
    adc_previous=imu_previous=missed_adc=missed_imu=0; period_ms=20;
    imu = (Mpu6050){board_sensor_bus(), 0x68};
    imu_ready = mpu6050_init(&imu) == BUS_OK;
    last_retry = measurement_ms();
    sample_adc();
    if (imu_ready) sample_pose();
    last_adc = last_imu = last_status = measurement_ms();
    (void)line("project=LightBench build=%s adc=%u imu=%u button=ready store=flash uart=115200_8N1 clock=SysTick_1ms recovered=%u rejected=%lu us=%lu\r\n",
        BUILD_ID, bench.light.valid, imu_ready,bench.history.count,(unsigned long)store.rejected,(unsigned long)measurement_us());
    board_led(false);
}
void runtime_poll(void) {
    uint32_t loop_start=measurement_us(), start;
    uint32_t now = measurement_ms();
    if (bench_expire(&bench, now)) summary("time_limit");
    if (board_rx_overflow()) { ++rx_losses; protocol_overflow(&reader); }
    start=measurement_us(); bool received=false;
    for (unsigned budget = 0; budget < 64; ++budget) {
        int ch = board_get_char();
        if (ch < 0) break;
        received=true;
        BenchCommand cmd = protocol_feed(&reader, ch);
        if (cmd != CMD_NONE) execute(cmd, measurement_ms());
    }
    if(received) busy_us+=measured(M_RX,start);
    now = measurement_ms();
    if ((uint32_t)(now - last_adc) >= period_ms) {
        unsigned due=(now-last_adc)/period_ms; missed_adc+=due-1;
        last_adc+=due*period_ms; start=measurement_us(); sample_adc(); busy_us+=measurement_us()-start;
    }
    now = measurement_ms();
    if (!imu_ready && (uint32_t)(now - last_retry) >= 1000u) {
        start=measurement_us();
        last_retry = now;
        BusResult result = mpu6050_init(&imu);
        imu_ready = result == BUS_OK;
        (void)line("probe=imu result=%u ms=%lu\r\n", result, (unsigned long)measurement_ms());
        busy_us+=measured(M_RETRY,start);
    }
    now = measurement_ms();
    if ((uint32_t)(now - last_imu) >= period_ms) {
        unsigned due=(now-last_imu)/period_ms;
        if(imu_ready) missed_imu+=due-1;
        last_imu+=due*period_ms;
        if(imu_ready) { start=measurement_us(); sample_pose(); busy_us+=measurement_us()-start; }
    }
    now = measurement_ms();
    start=measurement_us();
    bool button=board_button(), changed=button!=bench.button_candidate;
    if (bench_button(&bench, button, now)) {
        metric(M_REACTION,(now-bench.button_changed_ms)*1000u);
        ReferencePoint p;
        BenchResult result = bench_mark(&bench, now, &p);
        if (result == BENCH_OK) {
            (void)point_store_enqueue(&store,p);
            board_point_marker();
            (void)line("point seq=%lu series=%lu ms=%lu edge_ms=%lu raw=%u angle_cdeg=%u flags=%u light_ms=%lu pose_ms=%lu\r\n",
                (unsigned long)p.sequence, (unsigned long)p.series, (unsigned long)p.mark_ms,
                (unsigned long)bench.button_changed_ms, p.raw, p.angle_cdeg, p.flags,
                (unsigned long)p.light_ms, (unsigned long)p.pose_ms);
        } else (void)line("reply=point result=%s\r\n", result_name(result));
        changed=true;
    }
    if(changed) busy_us+=measured(M_BUTTON,start);
    if (board_tx_overflow()) ++tx_losses;
    if ((uint32_t)(now - last_status) >= 1000u) { last_status = now; start=measurement_us(); status(now); busy_us+=measured(M_STATUS,start); }
    board_led(bench.running);
    if(exporting && board_tx_free()>=768u) { start=measurement_us(); export_step(); busy_us+=measured(M_EXPORT,start); }
    if(store.count && !store.failed) {
        uint32_t committed=store.committed;
        start=measurement_us(); point_store_step(&store); busy_us+=measured(M_STORE,start);
        if(store.committed!=committed) (void)line("persist seq=%lu us=%lu pending=%u\r\n",(unsigned long)store.last_sequence,(unsigned long)measurement_us(),store.count);
        if(store.failed) (void)line("fault=flash seq=%lu pending=%u\r\n",(unsigned long)store.last_sequence,store.count);
    }
    if(report_at<=M_COUNT && board_tx_free()>=768u) {
        static const char *names[]={"loop","adc","imu","retry","rx","button","store","export","status","adc_interval","imu_interval","reaction"};
        if(report_at<M_COUNT) {
            Timing s=timing[report_at];
            (void)line("METRIC name=%s n=%lu min_us=%lu avg_us=%lu max_us=%lu\r\n",names[report_at],(unsigned long)s.n,(unsigned long)s.min,(unsigned long)(s.n?s.sum/s.n:0),(unsigned long)s.max);
        } else {
            uint32_t elapsed=measurement_us()-epoch_us;
            (void)line("MEASURE us=%lu elapsed_us=%lu busy_us=%lu missed_adc=%lu missed_imu=%lu stack=%lu heap=%lu period_ms=%u rx_lost=%lu tx_lost=%lu hal_ms=%lu resolution_us=1000\r\n",
                (unsigned long)measurement_us(),(unsigned long)elapsed,(unsigned long)busy_us,
                (unsigned long)missed_adc,(unsigned long)missed_imu,(unsigned long)measurement_stack(),(unsigned long)measurement_heap(),period_ms,(unsigned long)rx_losses,(unsigned long)tx_losses,(unsigned long)HAL_GetTick());
        }
        ++report_at;
    }
    (void)measured(M_LOOP,loop_start);
}
