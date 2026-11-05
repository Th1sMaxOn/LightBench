#include "board.h"
#include "main.h"

/* Single foreground producer; the UART completion interrupt consumes bytes. */
#ifdef LAB4
#define TX_CAPACITY 1024
#else
#define TX_CAPACITY 2048
#endif
enum { TX_SIZE = TX_CAPACITY, TX_MASK = TX_SIZE - 1 };
extern UART_HandleTypeDef huart2;
static uint8_t buffer[TX_SIZE];
static volatile uint16_t head, tail;
#if !defined(LAB4) || defined(LAB4_DIAGNOSTIC)
static volatile uint16_t in_flight;
#endif
static volatile bool start_fault;

static void start_locked(void) {
#if defined(LAB4) && !defined(LAB4_DIAGNOSTIC)
    if (head != tail) huart2.Instance->CR1 |= USART_CR1_TXEIE_TXFNFIE;
#else
    if (in_flight || head == tail) return;
    uint16_t length = head > tail ? (uint16_t)(head - tail) : (uint16_t)(TX_SIZE - tail);
    if (HAL_UART_Transmit_IT(&huart2, &buffer[tail], length) == HAL_OK)
        in_flight = length;
    else
        start_fault = true;
#endif
}

void board_tx_service(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    start_locked();
    if (!mask) __enable_irq();
}

size_t board_tx_free(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    size_t available = (tail - head - 1u) & TX_MASK;
    if (!mask) __enable_irq();
    return available;
}

bool board_tx_write(const uint8_t *bytes, size_t length) {
    if (length > board_tx_free()) return false;
    /* Unpublished bytes are inaccessible to the interrupt until head moves. */
    uint16_t next = head;
    for (size_t i = 0; i < length; ++i) {
        buffer[next] = bytes[i];
        next = (uint16_t)((next + 1u) & TX_MASK);
    }
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    head = next;
    start_locked();
    if (!mask) __enable_irq();
    return true;
}

int board_put_char(int value) {
    uint8_t byte = (uint8_t)value;
    if (board_tx_write(&byte, 1)) return value;
    start_fault = true;
    return -1;
}

#if defined(LAB4) && !defined(LAB4_DIAGNOSTIC)
bool board_uart_irq(void) {
    USART_TypeDef *uart = huart2.Instance;
    uint32_t flags = uart->ISR;
    bool received = false;
    uint32_t errors = flags & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE);
    if (errors) {
        uart->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
        board_uart_rx_error(); received = true;
    }
    if ((flags & USART_ISR_RXNE_RXFNE) && (uart->CR1 & USART_CR1_RXNEIE_RXFNEIE)) {
        board_uart_rx_byte((uint8_t)uart->RDR); received = true;
    }
    if ((flags & USART_ISR_TXE_TXFNF) && (uart->CR1 & USART_CR1_TXEIE_TXFNFIE)) {
        if (tail != head) {
            uart->TDR = buffer[tail];
            tail = (uint16_t)((tail + 1u) & TX_MASK);
        } else uart->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
    }
    return received;
}
#else
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart) {
    if (uart != &huart2 || !in_flight) return;
    tail = (uint16_t)((tail + in_flight) & TX_MASK);
    in_flight = 0;
    start_locked();
}
#endif

bool board_tx_overflow(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    bool fault = start_fault;
    start_fault = false;
    if (!mask) __enable_irq();
    return fault;
}
