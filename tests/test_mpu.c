#include "mpu6050.h"
#include "pose.h"
#include "fixtures/final-imu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { BusResult result; uint8_t id; unsigned reads,writes,fail_write; } Bus;
static const uint8_t registers[]={0x6b,0x19,0x1a,0x1b,0x1c};
static const uint8_t values[]={1,0x13,3,0,0};
static const uint8_t vector[14]={0x40,0,0xc0,0,0x20,0,0,0,0,0x83,0xff,0x7d,0,0};
static BusResult read_bus(void *ctx,uint8_t addr,uint8_t reg,uint8_t *out,size_t n) {
    Bus *b=ctx; assert(addr==0x68); ++b->reads;
    if(b->result!=BUS_OK) return b->result;
    if(reg==0x75) { assert(n==1); *out=b->id; }
    else { assert(reg==0x3b && n==14); memcpy(out,vector,n); }
    return BUS_OK;
}
static BusResult write_bus(void *ctx,uint8_t addr,uint8_t reg,const uint8_t *data,size_t n) {
    Bus *b=ctx; assert(addr==0x68 && n==1);
    assert(reg==registers[b->writes] && *data==values[b->writes]);
    ++b->writes; return b->writes==b->fail_write ? BUS_NACK : BUS_OK;
}
int main(void) {
    Bus b={.id=0x68}; Mpu6050 d={{&b,read_bus,write_bus},0x68};
    assert(mpu6050_init(&d)==BUS_OK && b.writes==5);
    ImuSample s; assert(mpu6050_read(&d,&s)==BUS_OK);
    assert(s.accel_mg[0]==1000 && s.accel_mg[1]==-1000 && s.accel_mg[2]==500);
    assert(s.gyro_mdps[0]==1000 && s.gyro_mdps[1]==-1000 && s.temperature_mc==36530);
    const uint8_t edges[14]={0x7f,0xff,0x80,0,0,0,0xff,0xff,0x7f,0xff,0x80,0,0,0};
    mpu6050_decode(edges,&s);
    assert(s.accel_mg[0]==1999 && s.accel_mg[1]==-2000);
    assert(s.gyro_mdps[0]==250129 && s.gyro_mdps[1]==-250137);
    for(unsigned i=BUS_NACK;i<=BUS_ERROR;++i) {
        b.result=(BusResult)i; unsigned writes=b.writes;
        assert(mpu6050_init(&d)==(BusResult)i && b.writes==writes);
        ImuSample before=s; assert(mpu6050_read(&d,&s)==(BusResult)i);
        assert(!memcmp(&before,&s,sizeof(s)));
    }
    b.result=BUS_OK; b.id=0x69; assert(mpu6050_init(&d)==BUS_ERROR);
    b.id=0x68;
    for(unsigned i=1;i<=5;++i) {
        b.writes=0; b.fail_write=i;
        assert(mpu6050_init(&d)==BUS_NACK && b.writes==i);
    }
    mpu6050_decode(captured_pose,&s);
    assert(s.accel_mg[0]==1000 && s.accel_mg[1]==0 && s.accel_mg[2]==0);
    assert(s.temperature_mc==24001 && pose_angle(s.accel_mg)==9000);
    puts("mpu: config sequence, signed scales, NACK/timeout/error/ID and output preservation passed; synthetic boundaries plus own Wokwi fixture");
    return 0;
}
