#include "light_input.h"
#include "bench.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef enum { NONE, PREPARE, START, COMPLETE, STOP } Fault;
typedef struct {
    Fault fault;
    uint16_t value;
    char calls[80];
    size_t count;
} LightFake;
static void note(LightFake *f, char c) { f->calls[f->count++] = c; f->calls[f->count] = 0; }
static bool prepare(void *p) { LightFake *f=p; note(f,'P'); return f->fault != PREPARE; }
static bool start(void *p) { LightFake *f=p; note(f,'S'); return f->fault != START; }
static bool complete(void *p, uint16_t *raw) {
    LightFake *f=p; note(f,'C');
    if (f->fault == COMPLETE) return false;
    *raw=f->value; return true;
}
static bool stop(void *p) { LightFake *f=p; note(f,'T'); return f->fault != STOP; }
static void discard(void *p) { note(p,'D'); }
static void marker(void *p, bool on) { note(p,on ? '+' : '-'); }
static LightInputOps operations(LightFake *fake) {
    return (LightInputOps){fake,prepare,start,complete,stop,discard,marker};
}
static void clear(LightFake *f) { f->count=0; f->calls[0]=0; }

static void test_conversion_failure_and_next_probe(void) {
    const Fault faults[] = {PREPARE, START, COMPLETE, STOP};
    for (unsigned i=0; i<sizeof(faults)/sizeof(faults[0]); ++i) {
        LightFake f={.value=1234}; LightInput input;
        light_input_open(&input,operations(&f));
        assert(input.ready && !strcmp(f.calls,"P"));
        clear(&f); f.fault=faults[i];
        if (f.fault==PREPARE) input.ready=false;
        uint16_t value=999;
        assert(!light_input_sample(&input,&value));
        assert(value==999 && !input.ready);
        if (f.fault==PREPARE) assert(!strcmp(f.calls,"PD"));
        if (f.fault==START) assert(!strcmp(f.calls,"+ST-D"));
        if (f.fault==COMPLETE || f.fault==STOP) assert(!strcmp(f.calls,"+SCT-D"));
        f.fault=NONE; clear(&f);
        assert(light_input_sample(&input,&value));
        assert(input.ready && value==1234 && !strcmp(f.calls,"P+SCT-"));
    }
}
static void test_range_and_startup(void) {
    LightFake f={.fault=PREPARE,.value=4095}; LightInput input;
    light_input_open(&input,operations(&f));
    assert(!input.ready && !strcmp(f.calls,"PD"));
    f.fault=NONE; clear(&f);
    uint16_t value=44;
    assert(light_input_sample(&input,&value) && value==4095);
    f.value=4096; clear(&f);
    assert(!light_input_sample(&input,&value) && value==4095 && !input.ready);
    assert(!strcmp(f.calls,"+SCT-D"));
    assert(!light_input_sample(&input,NULL));
}
static void test_light_fault_ends_series_without_destroying_pose(void) {
    LightFake f={.value=500}; LightInput input;
    LightBench bench; int32_t gravity[3]={0,0,1000}; uint16_t raw=0;
    bench_init(&bench); light_input_open(&input,operations(&f));
    bool ok=light_input_sample(&input,&raw); bench_light(&bench,raw,ok,10);
    bench_pose(&bench,gravity,true,12);
    assert(bench_run(&bench,20)==BENCH_OK);
    f.fault=COMPLETE;
    ok=light_input_sample(&input,&raw); bench_light(&bench,raw,ok,40);
    assert(!bench.running && !bench.light.valid && bench.stats.light_failures==1);
    assert(bench.pose.valid && bench.pose.time_ms==12);
    bench_pose(&bench,gravity,true,60);
    assert(bench.pose.valid && bench.pose.time_ms==60);
    assert(bench_run(&bench,61)==BENCH_ADC_BAD);
    f.fault=NONE; f.value=321;
    ok=light_input_sample(&input,&raw); bench_light(&bench,raw,ok,80);
    assert(bench.light.valid && bench.light.raw==321);
    assert(bench_run(&bench,81)==BENCH_OK);
}
int main(void) {
    test_conversion_failure_and_next_probe();
    test_range_and_startup();
    test_light_fault_ends_series_without_destroying_pose();
    puts("light input failure, recovery and independent streams passed");
    return 0;
}
