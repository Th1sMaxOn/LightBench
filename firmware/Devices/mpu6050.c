#include "mpu6050.h"
#include <string.h>

/* InvenSense RM-MPU-6000A-00, register descriptions: retain 50 Hz / 2g /
 * 250dps, verify configuration before allowing a sample to reach LightBench. */
enum { RATE_REGISTER = 0x19, FRAME_REGISTER = 0x3b, POWER_REGISTER = 0x6b, ID_REGISTER = 0x75 };

void mpu6050_attach(Mpu6050 *device, Mpu6050Bus io, uint8_t address) {
    *device = (Mpu6050){ .io = io, .address = address, .configured = false };
}

static BusResult access_registers(Mpu6050 *d, const MpuRegisterAccess *request) {
    if (!d || !d->io.transfer || (d->address != 0x68 && d->address != 0x69)) return BUS_ERROR;
    return d->io.transfer(d->io.context, d->address, request);
}

BusResult mpu6050_init(Mpu6050 *device) {
    if (!device) return BUS_ERROR;
    device->configured = false;
    uint8_t identity = 0;
    MpuRegisterAccess request = { .reg = ID_REGISTER, .count = 1, .receive = &identity };
    BusResult result = access_registers(device, &request);
    if (result != BUS_OK) return result;
    if (identity != 0x68) return BUS_ERROR;
    const uint8_t wake = 1;
    request = (MpuRegisterAccess){ .reg = POWER_REGISTER, .count = 1, .transmit = &wake };
    result = access_registers(device, &request);
    if (result != BUS_OK) return result;
    const uint8_t settings[4] = { 19, 3, 0, 0 };
    request = (MpuRegisterAccess){ .reg = RATE_REGISTER, .count = sizeof(settings), .transmit = settings };
    result = access_registers(device, &request);
    if (result != BUS_OK) return result;
    uint8_t actual[4] = {0};
    request = (MpuRegisterAccess){ .reg = RATE_REGISTER, .count = sizeof(actual), .receive = actual };
    result = access_registers(device, &request);
    if (result != BUS_OK) return result;
    if (memcmp(settings, actual, sizeof(actual))) return BUS_ERROR;
    request = (MpuRegisterAccess){ .reg = POWER_REGISTER, .count = 1, .receive = &identity };
    result = access_registers(device, &request);
    if (result != BUS_OK) return result;
    if (identity != wake) return BUS_ERROR;
    device->configured = true;
    return BUS_OK;
}

void mpu6050_decode(const uint8_t bytes[14], ImuSample *sample) {
    int32_t fields[7];
    for (unsigned field = 0; field != 7; ++field) {
        const uint8_t *pair = &bytes[2 * field];
        fields[field] = (int32_t)((uint32_t)pair[0] * 256u + pair[1]);
        if (pair[0] & 0x80u) fields[field] -= 65536;
    }
    ImuSample decoded;
    for (unsigned axis = 0; axis != 3; ++axis) {
        decoded.accel_mg[axis] = fields[axis] * 1000 / 16384;
        decoded.gyro_mdps[axis] = fields[axis + 4] * 1000 / 131;
    }
    decoded.temperature_mc = 36530 + fields[3] * 1000 / 340;
    *sample = decoded;
}

BusResult mpu6050_read(Mpu6050 *device, ImuSample *sample) {
    if (!device || !sample || !device->configured) return BUS_ERROR;
    uint8_t frame[14];
    const MpuRegisterAccess request = { .reg = FRAME_REGISTER, .count = sizeof(frame), .receive = frame };
    BusResult result = access_registers(device, &request);
    if (result == BUS_OK) mpu6050_decode(frame, sample);
    else device->configured = false;
    return result;
}
