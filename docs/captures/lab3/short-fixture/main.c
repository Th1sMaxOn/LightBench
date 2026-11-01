#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {pin_t fault, ready, phase; timer_t timer; uint8_t reg; bool first_write; unsigned stage;} state_t;
static state_t state;
static bool connect_device(void *u, uint32_t address, bool read) {state_t *s=u; (void)address; if (!read) s->first_write=true; return s->stage>=1;}
static bool write_device(void *u, uint8_t b) {state_t *s=u; if(s->first_write){s->reg=b;s->first_write=false;}else{s->reg++;}return true;}
static uint8_t read_device(void *u) {state_t *s=u; uint8_t r=s->reg++; if(r==0x75)return 0x68; if(r==0x3f)return 0x40; return 0;}
static void next_phase(void *u) {state_t *s=u; ++s->stage; if(s->stage==1){pin_write(s->ready,1);timer_start(s->timer,8000000,false);} else if(s->stage==2){pin_mode(s->fault,OUTPUT_LOW);pin_write(s->phase,1);timer_start(s->timer,3000000,false);} else if(s->stage==3){pin_mode(s->fault,INPUT);pin_write(s->phase,0);}}
void chip_init(void) {state_t *s=&state;s->fault=pin_init("FAULT",INPUT);s->ready=pin_init("READY",OUTPUT_LOW);s->phase=pin_init("PHASE",OUTPUT_LOW);pin_t sda=pin_init("SDA",INPUT_PULLUP),scl=pin_init("SCL",INPUT_PULLUP);const i2c_config_t i={.address=0x68,.sda=sda,.scl=scl,.connect=connect_device,.read=read_device,.write=write_device,.user_data=s};i2c_init(&i);const timer_config_t t={.callback=next_phase,.user_data=s};s->timer=timer_init(&t);timer_start(s->timer,2000000,false);}
