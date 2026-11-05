#include "board.h"
#include "main.h"
#include "light_input.h"

extern UART_HandleTypeDef huart2;
static ADC_HandleTypeDef adc;
static LightInput light_input;
static bool prepare_light(void *context);
static bool start_light(void *context);
static bool finish_light(void *context, uint16_t *raw);
static bool stop_light(void *context);
static void discard_light(void *context);
static void mark_light(void *context, bool active);
static volatile uint16_t rx_head, rx_tail;
static volatile bool rx_overflow;
static uint8_t rx_ring[256];
void board_uart_rx_byte(uint8_t byte) {
    uint16_t next = (uint16_t)((rx_head + 1u) & 255u);
    if (next == rx_tail) rx_overflow = true;
    else { rx_ring[rx_head] = byte; rx_head = next; }
}
void board_uart_rx_error(void) { rx_overflow = true; }
#if !defined(LAB4) || defined(LAB4_DIAGNOSTIC)
static uint8_t rx_byte;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
    if (uart != &huart2) return;
    board_uart_rx_byte(rx_byte);
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
    if (uart == &huart2) { rx_overflow = true; (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1); }
}
#endif
int board_get_char(void) {
    if (rx_head == rx_tail) return -1;
    uint8_t value = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & 255u);
    return value;
}
bool board_rx_overflow(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq(); bool value = rx_overflow; rx_overflow = false;
    if (!mask) __enable_irq();
    return value;
}
bool board_button(void) { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET; }
void board_point_marker(void) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); }
void board_imu_marker(bool on) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
void board_led(bool on) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
bool board_read_light(uint16_t *value) {
    return light_input_sample(&light_input, value);
}
void HAL_ADC_MspInit(ADC_HandleTypeDef *handle) {
    if (handle->Instance == ADC1) __HAL_RCC_ADC_CLK_ENABLE();
}
void board_init(void) {
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_1; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLDOWN; HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_6; gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Pull = GPIO_NOPULL; HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1; HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0; gpio.Mode = GPIO_MODE_ANALOG; HAL_GPIO_Init(GPIOA, &gpio);
    LightInputOps light_ops = { &adc, prepare_light, start_light, finish_light,
                                stop_light, discard_light, mark_light };
    light_input_open(&light_input, light_ops);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0); HAL_NVIC_EnableIRQ(USART2_IRQn);
#if defined(LAB4) && !defined(LAB4_DIAGNOSTIC)
    huart2.Instance->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    huart2.Instance->CR3 |= USART_CR3_EIE;
    huart2.Instance->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_PEIE;
#else
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
#endif
}
static bool prepare_light(void *context) {
    ADC_HandleTypeDef *device = context;
    *device = (ADC_HandleTypeDef){0};
    device->Instance = ADC1;
    device->Init = (ADC_InitTypeDef){
        .ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2,
        .Resolution = ADC_RESOLUTION_12B,
        .DataAlign = ADC_DATAALIGN_RIGHT,
        .ScanConvMode = ADC_SCAN_SEQ_FIXED,
        .EOCSelection = ADC_EOC_SINGLE_CONV,
        .NbrOfConversion = 1,
        .ExternalTrigConv = ADC_SOFTWARE_START,
        .ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE,
        .Overrun = ADC_OVR_DATA_OVERWRITTEN,
        .SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5,
        .SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5,
        .TriggerFrequencyMode = ADC_TRIGGER_FREQ_LOW
    };
    if (HAL_ADC_Init(device) != HAL_OK) return false;
    ADC_ChannelConfTypeDef channel = {
        .Channel = ADC_CHANNEL_0, .Rank = ADC_RANK_CHANNEL_NUMBER,
        .SamplingTime = ADC_SAMPLINGTIME_COMMON_1
    };
    if (HAL_ADC_ConfigChannel(device, &channel) != HAL_OK) return false;
    return HAL_ADCEx_Calibration_Start(device) == HAL_OK;
}
static bool start_light(void *context) {
    return HAL_ADC_Start(context) == HAL_OK;
}
static bool finish_light(void *context, uint16_t *raw) {
    ADC_HandleTypeDef *device = context;
    if (HAL_ADC_PollForConversion(device, 2) != HAL_OK) return false;
    *raw = (uint16_t)HAL_ADC_GetValue(device);
    return true;
}
static bool stop_light(void *context) {
    return HAL_ADC_Stop(context) == HAL_OK;
}
static void discard_light(void *context) {
    (void)HAL_ADC_DeInit(context);
}
static void mark_light(void *context, bool active) {
    (void)context;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, active ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
