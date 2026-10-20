#include "runtime.h"
#include "board.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t now, probes, adc_reads, markers;
static bool pressed, adc_ok=true, bus_ok=true, rx_over, tx_over, led;
static size_t free_bytes=2047;
static char output[65536], input[1024];
static unsigned used, in_at, in_len;
static BusResult bus_read(void *ctx,uint8_t addr,uint8_t reg,uint8_t *data,size_t n) {
    (void)ctx; ++now;
    if(reg==0x75) ++probes;
    if(addr!=0x68 || !bus_ok) return BUS_NACK;
    memset(data,0,n);
    if(reg==0x75) { assert(n==1); data[0]=0x68; }
    else { assert(reg==0x3b && n==14); data[4]=0x40; }
    return BUS_OK;
}
static BusResult bus_write(void *ctx,uint8_t a,uint8_t r,const uint8_t *d,size_t n) {
    (void)ctx;(void)a;(void)r;(void)d;(void)n; return BUS_OK;
}
uint32_t HAL_GetTick(void) { return now; }
void board_init(void) {}
SensorBus board_sensor_bus(void) { return (SensorBus){NULL,bus_read,bus_write}; }
bool board_read_light(uint16_t *value) { ++adc_reads; ++now; *value=1234; return adc_ok; }
bool board_button(void) { return pressed; }
void board_led(bool on) { led=on; }
void board_point_marker(void) { ++markers; }
void board_imu_marker(bool on) { (void)on; }
size_t board_tx_free(void) { return free_bytes; }
int board_get_char(void) { return in_at<in_len ? (unsigned char)input[in_at++] : -1; }
bool board_rx_overflow(void) { bool result=rx_over; rx_over=false; return result; }
bool board_tx_overflow(void) { bool result=tx_over; tx_over=false; return result; }
int board_put_char(int ch) { if (used+1==sizeof(output)) { memmove(output,output+32768,used-32768); used-=32768; } output[used++]=(char)ch; output[used]=0; return ch; }
static void clear_output(void) { used=0; output[0]=0; }
static void command(const char *s) {
    assert(strlen(s)<sizeof(input)); strcpy(input,s); in_at=0; in_len=(unsigned)strlen(s);
    while(in_at<in_len) runtime_poll();
}
static void step(unsigned ms) { uint32_t end=now+ms; while(now<end) { ++now; runtime_poll(); } }
static void click(void) { pressed=true;step(40);pressed=false;step(40); }
int main(void) {
    runtime_init();
    assert(strstr(output,"project=LightBench") && strstr(output,"adc=1 imu=1 button=ready"));
    command("stop\nrun\nrun\n"); assert(led && strstr(output,"already_running"));
    clear_output(); click(); assert(markers==1 && strstr(output,"flags=0"));
    pressed=true; step(200); assert(markers==2); step(200); assert(markers==2);
    pressed=false;step(40);
    clear_output(); command("export\nexport\n");
    assert(strstr(output,"export_begin count=2") && strstr(output,"result=busy"));
    click(); step(100); assert(markers==3 && strstr(output,"export_end count=2"));
    command("imuaddr 0x69\n"); unsigned reads=adc_reads, probe_count=probes;
    step(900); assert(probes==probe_count && adc_reads>reads && led);
    step(110); assert(probes==probe_count+1);
    command("status\n"); assert(strstr(output,"imu=0 angle_cdeg=invalid"));
    command("imuaddr 0x68\n"); step(40); command("status\n");
    assert(strstr(output,"imu=1 angle_cdeg=0"));
    bus_ok=false; step(30); command("selftest\n"); assert(strstr(output,"selftest adc=1 imu=0"));
    bus_ok=true; step(1010);
    clear_output(); adc_ok=false; step(25);
    assert(!led && strstr(output,"reason=adc_fault"));
    command("run\n"); assert(strstr(output,"adc_unavailable"));
    adc_ok=true; step(25); command("run\nstop\nstop\n"); assert(strstr(output,"mean_raw=empty"));
    clear_output(); click(); assert(strstr(output,"reply=point result=not_running"));
    command("unknown\n"); assert(strstr(output,"unknown_command"));
    rx_over=true; command("run\n"); assert(strstr(output,"invalid_line"));
    command("status\n"); assert(strstr(output,"rx_lost=1"));
    tx_over=true; step(1); command("status\n"); assert(strstr(output,"tx_lost=1"));
    free_bytes=0;command("status\n"); free_bytes=2047;command("status\n");
    assert(strstr(output,"tx_lost=2"));
    command("export\n"); free_bytes=0;step(2); free_bytes=2047;step(10);
    clear_output(); command("run\n"); step(3600001);
    assert(!led && strstr(output,"reason=time_limit"));
    puts("runtime: concurrent polling, button, export, retry timing, ADC stop, recovery, errors and hour limit passed with fake ports");
    return 0;
}
