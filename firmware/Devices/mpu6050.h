#ifndef LIGHTBENCH_MPU6050_H
#define LIGHTBENCH_MPU6050_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { BUS_OK, BUS_NACK, BUS_TIMEOUT, BUS_ERROR } BusResult;
/* Exactly one data pointer is set; one request is one register operation. */
typedef struct {
    uint8_t reg, count;
    uint8_t *receive;
    const uint8_t *transmit;
} MpuRegisterAccess;
typedef struct {
    void *context;
    BusResult (*transfer)(void *, uint8_t, const MpuRegisterAccess *);
} Mpu6050Bus;
typedef struct { Mpu6050Bus io; uint8_t address; bool configured; } Mpu6050;
typedef struct { int32_t accel_mg[3], gyro_mdps[3], temperature_mc; } ImuSample;
void mpu6050_attach(Mpu6050 *device, Mpu6050Bus io, uint8_t address);
BusResult mpu6050_init(Mpu6050 *device);
BusResult mpu6050_read(Mpu6050 *device, ImuSample *sample);
void mpu6050_decode(const uint8_t bytes[14], ImuSample *sample);
#endif
