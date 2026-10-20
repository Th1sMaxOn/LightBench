#include "bench.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int32_t up[3] = {0, 0, 1000};
static void seeded(LightBench *b, uint32_t now) {
    bench_init(b);
    bench_light(b, 2048, true, now);
    bench_pose(b, up, true, now);
    assert(bench_run(b, now) == BENCH_OK);
}
static void angles(void) {
    const int32_t inputs[][3] = {{0,0,1000},{1000,0,0},{0,0,-1000},{0,0,0},
        {0,0,850},{0,0,849},{0,0,1150},{0,0,1151},{INT32_MAX,0,0},{0,-1000,0}};
    const uint16_t want[] = {0,9000,18000,ANGLE_INVALID,0,ANGLE_INVALID,0,ANGLE_INVALID,ANGLE_INVALID,9000};
    for (unsigned i=0; i<sizeof(want)/sizeof(want[0]); ++i) assert(pose_angle(inputs[i]) == want[i]);
    for (int degree=0; degree<=180; ++degree) {
        double rad=degree*3.141592653589793/180.0;
        int32_t mg[3]={(int32_t)lround(sin(rad)*1000),0,(int32_t)lround(cos(rad)*1000)};
        assert(abs((int)pose_angle(mg)-degree*100) <= 5);
    }
}
static void temporal_pairs(void) {
    LightBench b; bench_init(&b);
    ReferencePoint p=bench_observe(&b,0);
    assert(p.flags==(POINT_LIGHT_BAD|POINT_POSE_BAD) && p.raw==UINT16_MAX && p.angle_cdeg==ANGLE_INVALID);
    seeded(&b,100);
    bench_light(&b,4095,true,120); bench_pose(&b,up,true,100);
    p=bench_observe(&b,140); assert(!p.flags && p.light_ms==120 && p.pose_ms==100);
    p=bench_observe(&b,141); assert(p.flags==POINT_POSE_OLD);
    bench_light(&b,0,true,121);
    p=bench_observe(&b,140); assert(p.flags==POINT_SKEW);
    bench_pose(&b,up,true,142);
    p=bench_observe(&b,150); assert(p.flags==POINT_SKEW);
    bench_light(&b,0,true,100); bench_pose(&b,up,true,120);
    p=bench_observe(&b,140); assert(!p.flags);
    p=bench_observe(&b,161); assert(p.flags==(POINT_LIGHT_OLD|POINT_POSE_OLD));
    int32_t dynamic[3]={0,0,0};
    bench_pose(&b,dynamic,true,161); bench_light(&b,30,true,161);
    assert(bench_observe(&b,161).flags==POINT_DYNAMIC);
    bench_pose(&b,NULL,false,161);
    assert(bench_observe(&b,161).flags==POINT_POSE_BAD);
    bench_light(&b,8192,true,161);
    assert(bench_observe(&b,161).flags==(POINT_LIGHT_BAD|POINT_POSE_BAD));
    seeded(&b,UINT32_MAX-20u);
    bench_pose(&b,up,true,UINT32_MAX-10u); bench_light(&b,40,true,9u);
    p=bench_observe(&b,29u); assert(!p.flags);
    assert(bench_observe(&b,30u).flags==POINT_POSE_OLD);
}
static void series_stats(void) {
    LightBench b; bench_init(&b);
    assert(bench_run(&b,0)==BENCH_ADC_BAD);
    seeded(&b,100);
    assert(bench_run(&b,100)==BENCH_RUNNING);
    assert(b.series==1 && b.stats.light_count==0);
    bench_light(&b,0,true,120); bench_light(&b,2048,true,140); bench_light(&b,4095,true,181);
    bench_pose(&b,up,true,120);
    int32_t side[3]={1000,0,0}, zero[3]={0};
    bench_pose(&b,side,true,140); bench_pose(&b,zero,true,160);
    bench_pose(&b,NULL,false,180); bench_pose(&b,up,true,201);
    assert(b.stats.raw_sum==6143 && b.stats.light_count==3);
    assert(b.stats.raw_min==0 && b.stats.raw_max==4095 && b.stats.light_gaps==1);
    assert(b.stats.pose_count==4 && b.stats.angle_count==3 && b.stats.angle_sum==9000);
    assert(b.stats.pose_failures==1 && b.stats.pose_gaps==1);
    ReferencePoint p; assert(bench_mark(&b,201,&p)==BENCH_OK);
    assert(bench_stop(&b,210)==BENCH_OK && b.stopped_ms==210);
    assert(bench_stop(&b,211)==BENCH_STOPPED);
    assert(bench_mark(&b,212,&p)==BENCH_STOPPED);
    assert(bench_run(&b,220)==BENCH_OK && b.series==2 && b.history.count==1);
    assert(!b.stats.light_count && !b.stats.angle_count && !b.stats.raw_sum);
    bench_light(&b,100,false,221);
    assert(!b.running && b.stats.light_failures==1 && !b.light.valid);
    assert(bench_run(&b,222)==BENCH_ADC_BAD);
    bench_light(&b,40,true,223); assert(bench_run(&b,224)==BENCH_OK);
    assert(!bench_expire(&b,224+SERIES_LIMIT_MS-1));
    assert(bench_expire(&b,224+SERIES_LIMIT_MS) && !b.running);
    assert(!bench_expire(&b,224+SERIES_LIMIT_MS+1));
    b.series=UINT32_MAX; assert(bench_run(&b,0)==BENCH_IDS_FULL);
    seeded(&b,0); b.sequence=UINT32_MAX;
    assert(bench_mark(&b,1,&p)==BENCH_IDS_FULL && !b.history.count);
}
static void button_and_records(void) {
    LightBench b; seeded(&b,0);
    assert(!bench_button(&b,true,10));
    assert(!bench_button(&b,false,15));
    assert(!bench_button(&b,true,20));
    assert(!bench_button(&b,true,49));
    assert(bench_button(&b,true,50));
    assert(!bench_button(&b,true,2000));
    assert(!bench_button(&b,false,2001));
    assert(!bench_button(&b,false,2031));
    assert(!bench_button(&b,true,2040));
    assert(bench_button(&b,true,2070));
    b.button_candidate=false; b.button_stable=false;
    assert(!bench_button(&b,true,UINT32_MAX-10));
    assert(!bench_button(&b,true,18)); assert(bench_button(&b,true,19));
    PointSnapshot snapshot;
    ReferencePoint p;
    for(unsigned i=1;i<=70;++i) assert(bench_mark(&b,i,&p)==BENCH_OK);
    records_snapshot(&b.history,&snapshot);
    assert(snapshot.count==64 && snapshot.overwritten==6);
    assert(snapshot.items[0].sequence==7 && snapshot.items[63].sequence==70);
    uint32_t crc=records_crc(snapshot.items,snapshot.count);
    for(unsigned i=0;i<70;++i) assert(bench_mark(&b,71+i,&p)==BENCH_OK);
    assert(snapshot.items[0].sequence==7 && records_crc(snapshot.items,snapshot.count)==crc);
    assert(records_crc(snapshot.items,snapshot.count)==crc);
    snapshot.items[0].raw^=1;
    assert(records_crc(snapshot.items,snapshot.count)!=crc);
    b.history.overwritten=UINT32_MAX; records_append(&b.history,p);
    assert(b.history.overwritten==UINT32_MAX);
    p=(ReferencePoint){1,2,3,4,5,6,7,8,0};
    assert(sizeof(p)==28 && records_crc(NULL,0)==0);
    /* Independent Python zlib.crc32(struct.pack('<IIIIIHHHH', ...)). */
    assert(records_crc(&p,1)==0xa51b2176u);
}
int main(void) {
    angles(); temporal_pairs(); series_stats(); button_and_records();
    puts("bench: angle sweep, temporal boundaries/wrap, series denominators/faults, debounce, immutable ring and CRC passed");
    return 0;
}
