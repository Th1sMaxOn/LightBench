#include "text_format.h"
#include <stdint.h>
#include <stdbool.h>
/* Deliberately limited to the integer/string formats of the LB1 protocol.
 * snprintf return/truncation semantics are retained; unsupported formats fail. */
static void put(char *out,size_t size,size_t *used,char c) {
    if(*used+1<size) out[*used]=c;
    ++*used;
}
int lb_vsnprintf(char *out,size_t size,const char *fmt,va_list args) {
    size_t used=0;
    while(*fmt) {
        if(*fmt!='%') { put(out,size,&used,*fmt++); continue; }
        ++fmt; bool zero=false,wide=false; unsigned width=0;
        if(*fmt=='0') { zero=true; ++fmt; }
        while(*fmt>='0'&&*fmt<='9') width=width*10u+(unsigned)(*fmt++-'0');
        if(*fmt=='l') {wide=true; ++fmt;}
        char type=*fmt++;
        if(type=='s') {const char *s=va_arg(args,const char *); while(*s)put(out,size,&used,*s++);}
        else if(type=='%') put(out,size,&used,'%');
        else if(type=='u'||type=='x'||type=='d') {
            unsigned long value; bool negative=false;
            if(type=='d') {long v=wide?va_arg(args,long):va_arg(args,int); negative=v<0; value=negative?0ul-(unsigned long)v:(unsigned long)v;}
            else value=wide?va_arg(args,unsigned long):va_arg(args,unsigned int);
            char digits[3*sizeof(unsigned long)+1]; unsigned n=0,base=type=='x'?16:10;
            do {digits[n++]="0123456789abcdef"[value%base];value/=base;} while(value);
            if(negative)put(out,size,&used,'-');
            while(width>n+(negative?1u:0u)) {put(out,size,&used,zero?'0':' ');--width;}
            while(n)put(out,size,&used,digits[--n]);
        } else {if(size)out[0]=0; return -1;}
    }
    if(size)out[used<size?used:size-1]=0;
    return (int)used;
}
int lb_snprintf(char *out,size_t size,const char *fmt,...) {
    va_list args;va_start(args,fmt);int n=lb_vsnprintf(out,size,fmt,args);va_end(args);return n;
}
