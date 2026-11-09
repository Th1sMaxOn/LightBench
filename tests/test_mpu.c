#include "mpu6050.h"
#include "pose.h"
#include "fixtures/final-imu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint8_t registers[128];
    unsigned operations, fail_at;
    BusResult failure;
    bool ignore_settings;
} RegisterModel;
static const uint8_t vector[14]={0x40,0,0xc0,0,0x20,0,0,0,0,0x83,0xff,0x7d,0,0};
static BusResult access(void *ctx, uint8_t address, const MpuRegisterAccess *q) {
    RegisterModel *m=ctx;
    assert(address==0x68 || address==0x69);
    assert(q->count && q->reg+q->count<=sizeof(m->registers));
    assert(!!q->receive != !!q->transmit);
    ++m->operations;
    if (m->operations==m->fail_at) {
        if(q->receive) q->receive[0]=0xaa; /* Failed reads may partially fill scratch. */
        return m->failure;
    }
    if(q->receive) memcpy(q->receive,m->registers+q->reg,q->count);
    else if (!(m->ignore_settings && q->reg==0x19))
        memcpy(m->registers+q->reg,q->transmit,q->count);
    return BUS_OK;
}
static void reset(RegisterModel *m, Mpu6050 *d) {
    memset(m,0,sizeof(*m)); m->registers[0x75]=0x68;
    memcpy(m->registers+0x3b,vector,sizeof(vector));
    mpu6050_attach(d,(Mpu6050Bus){m,access},0x68);
}
int main(void) {
    RegisterModel model; Mpu6050 d; reset(&model,&d);
    ImuSample s;
    assert(mpu6050_read(&d,&s)==BUS_ERROR && model.operations==0);
    assert(mpu6050_init(&d)==BUS_OK && d.configured);
    assert(model.registers[0x6b]==1 && model.registers[0x19]==19);
    assert(model.registers[0x1a]==3 && model.registers[0x1b]==0 && model.registers[0x1c]==0);
    unsigned init_operations=model.operations;
    assert(mpu6050_read(&d,&s)==BUS_OK);
    assert(s.accel_mg[0]==1000 && s.accel_mg[1]==-1000 && s.accel_mg[2]==500);
    assert(s.gyro_mdps[0]==1000 && s.gyro_mdps[1]==-1000 && s.temperature_mc==36530);
    const uint8_t edges[14]={0x7f,0xff,0x80,0,0,0,0xff,0xff,0x7f,0xff,0x80,0,0,0};
    mpu6050_decode(edges,&s);
    assert(s.accel_mg[0]==1999 && s.accel_mg[1]==-2000);
    assert(s.gyro_mdps[0]==250129 && s.gyro_mdps[1]==-250137);
    for(unsigned failure=BUS_NACK;failure<=BUS_ERROR;++failure) {
        for(unsigned operation=1;operation<=init_operations;++operation) {
            reset(&model,&d); model.fail_at=operation; model.failure=(BusResult)failure;
            assert(mpu6050_init(&d)==(BusResult)failure && !d.configured);
            unsigned calls=model.operations;
            assert(mpu6050_read(&d,&s)==BUS_ERROR && model.operations==calls);
            model.fail_at=0; assert(mpu6050_init(&d)==BUS_OK);
        }
        reset(&model,&d); assert(mpu6050_init(&d)==BUS_OK);
        model.fail_at=model.operations+1; model.failure=(BusResult)failure;
        ImuSample before=s;
        assert(mpu6050_read(&d,&s)==(BusResult)failure && !d.configured);
        assert(!memcmp(&before,&s,sizeof(s)));
    }
    reset(&model,&d); model.registers[0x75]=0x69;
    assert(mpu6050_init(&d)==BUS_ERROR && !d.configured);
    reset(&model,&d); model.ignore_settings=true;
    assert(mpu6050_init(&d)==BUS_ERROR && !d.configured);
    reset(&model,&d); d.address=0x69; assert(mpu6050_init(&d)==BUS_OK);
    reset(&model,&d); d.address=0x80; assert(mpu6050_init(&d)==BUS_ERROR && !model.operations);
    reset(&model,&d); d.io.transfer=NULL; assert(mpu6050_init(&d)==BUS_ERROR);
    assert(mpu6050_init(NULL)==BUS_ERROR && mpu6050_read(NULL,&s)==BUS_ERROR);
    mpu6050_decode(captured_pose,&s);
    assert(s.accel_mg[0]==1000 && s.accel_mg[1]==0 && s.accel_mg[2]==0);
    assert(s.temperature_mc==24001 && pose_angle(s.accel_mg)==9000);
    puts("mpu: units/extrema, verified configuration, every I/O failure, atomic sample and re-init passed");
    return 0;
}
