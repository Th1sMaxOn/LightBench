#include "board.h"
#include "stm32c0xx_hal.h"
#ifdef LAB4
#include "measurement.h"
#endif

/* One owner, 7-bit I2C1 at 100 kHz; HAL retains initialization only.
 * Wokwi accepts an early second TXDR write while the previous byte is still
 * on the wire. Reserve 100 us per TX byte: nine observed 10 us SCL periods
 * plus one period of margin. This compatibility guard does not replace
 * RXNE/STOPF or the shared deadline. No concurrent HAL transfer is allowed. */
enum { QUIET_US = 100, DEADLINE_MS = 10 };
enum Stage { IDLE, TRANSMIT, WRITE_STOP, TURNAROUND, RECEIVE, READ_STOP, COMPLETE };

static void dismiss(I2C_TypeDef *i2c) {
    i2c->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF |
               I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
}

static BusResult transfer(void *context, uint8_t address, const MpuRegisterAccess *q) {
    I2C_TypeDef *i2c = context;
    if (!q || !q->count || q->count > 14 || address > 0x7f ||
        (!!q->receive == !!q->transmit)) return BUS_ERROR;
    const unsigned tx_length = q->transmit ? q->count + 1u : 1u;
    const uint32_t guard = QUIET_US * (SystemCoreClock / 1000000u);
    uint32_t started = HAL_GetTick();
    uint32_t previous = SysTick->VAL;
    uint32_t cycles = 0, eligible = guard;

    unsigned sent = 0, received = 0;
    enum Stage stage = IDLE;
    BusResult outcome = BUS_OK;
    dismiss(i2c);
    while (stage != COMPLETE) {
        uint32_t ms = HAL_GetTick();
        uint32_t counter = SysTick->VAL;
        cycles += previous >= counter ? previous-counter : previous+SysTick->LOAD+1u-counter;
        previous = counter;
        uint32_t flags = i2c->ISR;
        if (flags & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)) { outcome = BUS_ERROR; break; }
        if (flags & I2C_ISR_NACKF) { outcome = BUS_NACK; break; }
        if ((uint32_t)(ms-started) >= DEADLINE_MS) { outcome = BUS_TIMEOUT; break; }
        switch (stage) {
        case IDLE:
        case TURNAROUND:
            if (cycles < eligible || (flags & I2C_ISR_BUSY)) break;
            dismiss(i2c);
            i2c->CR2 = ((uint32_t)address << 1) | I2C_CR2_AUTOEND | I2C_CR2_START |
                ((uint32_t)(stage == IDLE ? tx_length : q->count) << 16) |
                (stage == TURNAROUND ? I2C_CR2_RD_WRN : 0u);
            eligible = cycles + guard; /* Address byte must leave the wire too. */
            stage = stage == IDLE ? TRANSMIT : RECEIVE;
            break;
        case TRANSMIT:
            if (cycles < eligible || !(flags & I2C_ISR_TXIS)) break;
            i2c->TXDR = sent ? q->transmit[sent-1u] : q->reg;
            ++sent;
            eligible = cycles + guard;
            if (sent == tx_length) stage = WRITE_STOP;
            break;
        case WRITE_STOP:
            if (cycles < eligible || !(flags & I2C_ISR_STOPF)) break;
            dismiss(i2c);
            i2c->CR2 = 0;
            eligible = cycles + guard;
            stage = q->receive ? TURNAROUND : COMPLETE;
            break;
        case RECEIVE:
            if (!(flags & I2C_ISR_RXNE)) break;
            q->receive[received++] = (uint8_t)i2c->RXDR;
            if (received == q->count) stage = READ_STOP;
            break;
        case READ_STOP:
            if (!(flags & I2C_ISR_STOPF)) break;
            stage = COMPLETE;
            break;
        case COMPLETE: break;
        }
    }
    if (outcome != BUS_OK) {
        if (!(i2c->ISR & I2C_ISR_ARLO)) i2c->CR2 |= I2C_CR2_STOP;
        i2c->CR1 &= ~I2C_CR1_PE;
        i2c->CR2 = 0;
        i2c->CR1 |= I2C_CR1_PE;
    }
    dismiss(i2c);
    i2c->CR2 = 0;
    return outcome;
}

Mpu6050Bus board_sensor_bus(void) {
    return (Mpu6050Bus){ .context = I2C1, .transfer = transfer };
}
