#include "board.h"
#include "stm32c0xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

I2C_TypeDef test_i2c;
SysTick_Type test_systick;
uint32_t SystemCoreClock=48000000;
static uint32_t us, start_ms, flags_to_inject;
static unsigned phases, remaining, tx_count, rx_count, previous_tx, phase_start;
static uint8_t wire[16];
static bool suppress_stop, stalled, address_nack;

/* Deliberately stale TXIS: reject TXDR writes less than one 90-us wire byte
 * apart. The former unpaced HAL adapter fails this model. This is a host
 * regression model, not evidence that the physical peripheral behaves so. */
uint32_t HAL_GetTick(void) {
    us+=10;
    test_systick.LOAD=47999;
    test_systick.VAL=47999-(us%1000)*48;
    test_i2c.ISR &= ~test_i2c.ICR;
    test_i2c.ICR=0;
    if (test_i2c.CR2&I2C_CR2_START) {
        ++phases; remaining=(test_i2c.CR2>>16)&255; phase_start=us;
        assert((test_i2c.CR2&255)==0xd0 || (test_i2c.CR2&255)==0xd2);
        test_i2c.CR2 &= ~I2C_CR2_START;
        test_i2c.ISR=I2C_ISR_BUSY;
        if(address_nack) test_i2c.ISR|=I2C_ISR_NACKF;
        else if(!stalled) test_i2c.ISR|=test_i2c.CR2&I2C_CR2_RD_WRN?I2C_ISR_RXNE:I2C_ISR_TXIS;
    }
    if (test_i2c.TXDR!=0x100) {
        assert(remaining && tx_count<sizeof(wire));
        assert(us-phase_start>=90);
        if(tx_count) assert(us-previous_tx>=90);
        previous_tx=us;wire[tx_count++]=(uint8_t)test_i2c.TXDR;test_i2c.TXDR=0x100;
        if(!(--remaining)) {
            test_i2c.ISR= suppress_stop ? I2C_ISR_BUSY : I2C_ISR_STOPF;
        }
    }
    if ((test_i2c.ISR&I2C_ISR_RXNE) && !address_nack && remaining) {
        test_i2c.RXDR=0x40+rx_count++;
        if(!(--remaining)) {
            test_i2c.ISR=I2C_ISR_RXNE|(suppress_stop?0:I2C_ISR_STOPF);
        }
    } else if(!remaining && rx_count) test_i2c.ISR &= ~I2C_ISR_RXNE;
    if (phases) test_i2c.ISR|=flags_to_inject;
    return start_ms+us/1000;
}
static void reset(void) {
    memset(&test_i2c,0,sizeof(test_i2c));test_i2c.CR1=I2C_CR1_PE;test_i2c.TXDR=0x100;
    us=phases=remaining=tx_count=rx_count=previous_tx=flags_to_inject=start_ms=0;
    suppress_stop=stalled=address_nack=false;
}
int main(void) {
    uint8_t data[14]={19,3,0,0};Mpu6050Bus bus=board_sensor_bus();
    MpuRegisterAccess q={.reg=0x19,.count=4,.transmit=data};
    reset();assert(bus.transfer(bus.context,0x68,&q)==BUS_OK);
    assert(phases==1 && tx_count==5 && wire[0]==0x19 && !memcmp(wire+1,data,4));
    reset();q=(MpuRegisterAccess){.reg=0x3b,.count=14,.receive=data};
    assert(bus.transfer(bus.context,0x69,&q)==BUS_OK);
    assert(phases==2 && tx_count==1 && wire[0]==0x3b && rx_count==14);
    for(unsigned n=0;n<14;++n) assert(data[n]==0x40+n);
    reset();address_nack=true;assert(bus.transfer(bus.context,0x68,&q)==BUS_NACK);
    assert(test_i2c.CR2==0 && (test_i2c.CR1&I2C_CR1_PE));
    uint32_t errors[]={I2C_ISR_BERR,I2C_ISR_ARLO,I2C_ISR_OVR,I2C_ISR_NACKF|I2C_ISR_BERR};
    for(unsigned n=0;n<sizeof(errors)/sizeof(errors[0]);++n) {
        reset();flags_to_inject=errors[n];assert(bus.transfer(bus.context,0x68,&q)==BUS_ERROR);
    }
    reset();stalled=true;assert(bus.transfer(bus.context,0x68,&q)==BUS_TIMEOUT && us<=11000);
    reset();suppress_stop=true;assert(bus.transfer(bus.context,0x68,&q)==BUS_TIMEOUT && us<=11000);
    reset();test_i2c.ISR=I2C_ISR_BUSY;assert(bus.transfer(bus.context,0x68,&q)==BUS_TIMEOUT && phases==0);
    reset();start_ms=UINT32_MAX-4;stalled=true;
    assert(bus.transfer(bus.context,0x68,&q)==BUS_TIMEOUT && us<=11000);
    reset();assert(bus.transfer(bus.context,0x80,&q)==BUS_ERROR);
    assert(bus.transfer(bus.context,0x68,NULL)==BUS_ERROR);
    q.transmit=data;assert(bus.transfer(bus.context,0x68,&q)==BUS_ERROR);
    q.transmit=NULL;q.count=0;assert(bus.transfer(bus.context,0x68,&q)==BUS_ERROR && us==0);
    reset();q.count=14;assert(bus.transfer(bus.context,0x68,&q)==BUS_OK);
    puts("transport: stale-ready pacing, packet bytes, RX, NACK/errors, STOP/busy deadlines and tick wrap passed");
}
