#include "text_format.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
int main(void) {
 char a[200],b[200];
 for(unsigned cap=0;cap<200;++cap) {
  for(unsigned value=0;value<10000;value+=137) {
   memset(a,0x55,sizeof(a));memset(b,0x55,sizeof(b));
   int x=snprintf(a,cap,"%s:%u:%lu:%08lx:%d\r\n","LB1",value,(unsigned long)UINT_MAX,(unsigned long)value,INT_MIN);
   int y=lb_snprintf(b,cap,"%s:%u:%lu:%08lx:%d\r\n","LB1",value,(unsigned long)UINT_MAX,(unsigned long)value,INT_MIN);
   assert(x==y);assert(memcmp(a,b,cap)==0);
  }
 }
 puts("LB1 formatter: 14600 bounded libc comparisons passed");
}
