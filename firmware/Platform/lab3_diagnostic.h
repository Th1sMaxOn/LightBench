#ifndef LIGHTBENCH_LAB3_DIAGNOSTIC_H
#define LIGHTBENCH_LAB3_DIAGNOSTIC_H
#include <stdint.h>

/* These RAM snapshots are read through GDB after the diagnostic run is halted. */
typedef struct {
    uint32_t upper_lo, upper_hi, counter, status, counter_after;
    uint32_t result_lo, result_hi, hal_ms, irq_count;
} ClockSnapshot;
typedef struct {
    uint32_t calls, backwards, irq_count, irq_status, irq_counter;
    ClockSnapshot previous, before, after;
} ClockDiagnostic;
typedef struct {
    uint32_t operation, offset, unlock_result, hal_result, page_error, hal_error;
    uint32_t lock_result, sr_before, cr_before, sr_after, cr_after;
    uint32_t checked_words, non_ff_words, first_bad_offset, first_bad_word;
    uint32_t first_word, last_word, complete;
} FlashDiagnostic;
#ifdef LAB3_DIAGNOSTIC
extern volatile ClockDiagnostic clock_diagnostic;
extern volatile FlashDiagnostic flash_diagnostic;
#endif
#endif
