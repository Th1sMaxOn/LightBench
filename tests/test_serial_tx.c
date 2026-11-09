#include "board.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart2, other_uart;
static uint32_t primask;
static const uint8_t *pending;
static uint16_t pending_length;
static bool fail_start;
static bool in_interrupt;
static uint8_t wire[8192];
static size_t wire_length;

uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1; }
void __enable_irq(void) { primask = 0; }
void __DMB(void) {}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart, uint8_t *bytes, uint16_t length) {
    assert((primask || in_interrupt) && uart == &huart2 && !pending && length);
    if (fail_start) return HAL_ERROR;
    pending = bytes;
    pending_length = length;
    return HAL_OK;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart);
static void complete(void) {
    assert(pending && wire_length + pending_length <= sizeof(wire));
    memcpy(wire + wire_length, pending, pending_length);
    wire_length += pending_length;
    pending = NULL;
    in_interrupt = true;
    HAL_UART_TxCpltCallback(&huart2);
    in_interrupt = false;
}
int main(void) {
    uint8_t a[1800], b[500], c[2047];
    for (size_t i=0; i<sizeof(a); ++i) a[i]=(uint8_t)i;
    for (size_t i=0; i<sizeof(b); ++i) b[i]=(uint8_t)(i+7);
    memset(c, 0xa5, sizeof(c));
    assert(board_tx_free() == 2047);
    assert(board_tx_write(a, sizeof(a)) && pending_length == sizeof(a));
    assert(!board_tx_write(b, sizeof(b)) && board_tx_free() == 247);
    HAL_UART_TxCpltCallback(&other_uart);
    assert(board_tx_free() == 247);
    complete();
    assert(board_tx_write(b, sizeof(b)) && pending_length == 248);
    complete();
    assert(pending_length == 252);
    complete();
    assert(wire_length == sizeof(a)+sizeof(b));
    assert(!memcmp(wire,a,sizeof(a)) && !memcmp(wire+sizeof(a),b,sizeof(b)));
    fail_start = true;
    assert(board_tx_write(c,sizeof(c)) && !pending && !board_tx_free());
    assert(board_tx_overflow() && !board_tx_overflow());
    fail_start = false;
    primask = 1;
    board_tx_service();
    assert(primask == 1 && pending);
    primask = 0;
    while (pending) complete();
    assert(board_tx_free() == 2047 && wire_length == sizeof(a)+sizeof(b)+sizeof(c));
    assert(!memcmp(wire+sizeof(a)+sizeof(b),c,sizeof(c)));
    puts("serial TX tests passed");
}
