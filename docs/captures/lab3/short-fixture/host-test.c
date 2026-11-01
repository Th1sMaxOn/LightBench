#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#define WOKWI_API_H
#define INPUT 0
#define INPUT_PULLUP 2
#define OUTPUT_LOW 16
typedef int32_t pin_t; typedef uint32_t timer_t;
typedef struct {uint32_t address;pin_t sda,scl;bool (*connect)(void*,uint32_t,bool);uint8_t (*read)(void*);bool (*write)(void*,uint8_t);void *user_data;} i2c_config_t;
typedef struct {void(*callback)(void*);void*user_data;} timer_config_t;
static uint32_t last_mode=99, last_delay; static i2c_config_t bus; static timer_config_t tc;
static pin_t pin_init(const char *n,uint32_t m){(void)m;return !strcmp(n,"FAULT")?1:!strcmp(n,"READY")?2:!strcmp(n,"PHASE")?3:!strcmp(n,"SDA")?4:5;}
static void pin_mode(pin_t p,uint32_t m){assert(p==1);last_mode=m;} static void pin_write(pin_t p,uint32_t v){(void)p;(void)v;}
static int i2c_init(const i2c_config_t *c){bus=*c;return 1;}static timer_t timer_init(const timer_config_t *c){tc=*c;return 1;}static void timer_start(timer_t t,uint32_t n,bool repeat){(void)t;assert(!repeat);last_delay=n;}
#include "main.c"
int main(void){chip_init();assert(last_delay==2000000);assert(!bus.connect(bus.user_data,0x68,false));tc.callback(tc.user_data);assert(last_delay==8000000);assert(bus.connect(bus.user_data,0x68,false));assert(bus.write(bus.user_data,0x75));assert(bus.connect(bus.user_data,0x68,true));assert(bus.read(bus.user_data)==0x68);assert(bus.connect(bus.user_data,0x68,false));assert(bus.write(bus.user_data,0x3b));assert(bus.connect(bus.user_data,0x68,true));for(int i=0;i<4;i++)assert(bus.read(bus.user_data)==0);assert(bus.read(bus.user_data)==0x40);tc.callback(tc.user_data);assert(last_mode==OUTPUT_LOW&&last_delay==3000000);tc.callback(tc.user_data);assert(last_mode==INPUT);return 0;}
