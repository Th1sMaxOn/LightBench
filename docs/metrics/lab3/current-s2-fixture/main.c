#include "wokwi-api.h"
#include <stdint.h>
static pin_t output; static timer_t timer; static unsigned edges;
static void event(void *unused) {
 (void)unused;
 if(edges==160) return;
 pin_write(output,(edges&1u)?0:1);
 timer_start(timer,(edges++&1u)?44930000:70000,false);
}
void chip_init(void) {
 output=pin_init("OUT",OUTPUT_LOW);
 timer_config_t t={.callback=event};timer=timer_init(&t);
 timer_start(timer,10000000,false);
}
