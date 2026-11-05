#ifndef PROJECT_BOARD_H
#define PROJECT_BOARD_H
#include "mpu6050.h"
#include "point_store.h"
#include <stdbool.h>
#include <stdint.h>
void board_init(void);
PointFlash board_point_flash(void);
Mpu6050Bus board_sensor_bus(void);
bool board_read_light(uint16_t *value);
bool board_button(void);
void board_point_marker(void);
void board_imu_marker(bool on);
size_t board_tx_free(void);
void board_led(bool on);
int board_get_char(void);
bool board_rx_overflow(void);
bool board_tx_write(const uint8_t *bytes, size_t length);
int board_put_char(int value);
void board_tx_service(void);
bool board_tx_overflow(void);
/* The production UART is fixed to USART2, 115200, 8N1. */
void board_uart_rx_byte(uint8_t byte);
void board_uart_rx_error(void);
bool board_uart_irq(void);
#endif
