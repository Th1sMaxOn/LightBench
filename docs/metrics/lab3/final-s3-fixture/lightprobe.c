#include "wokwi-api.h"
static pin_t out; static timer_t timer; static unsigned phase;
static void next(void *u) { (void)u; phase++; pin_dac_write(out,phase==1 ? 1.65f : 3.3f); if(phase==1) timer_start(timer,4000000,false); }
void chip_init(void) {out=pin_init("OUT",ANALOG); pin_dac_write(out,0.0f); timer_config_t config={.callback=next}; timer=timer_init(&config); timer_start(timer,4000000,false);}
