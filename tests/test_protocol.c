#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BenchCommand feed(CommandReader *r,const char *s) {
    BenchCommand result=CMD_NONE;
    while(*s) { BenchCommand next=protocol_feed(r,*s++); if(next!=CMD_NONE) result=next; }
    return result;
}
int main(void) {
    CommandReader r={0};
    const char *lines[]={"run\n","stop\r","status\n","export\n","selftest\n","imuaddr 0x68\n","imuaddr 0x69\n"};
    for(unsigned i=0;i<7;++i) assert(feed(&r,lines[i])==(BenchCommand)(CMD_RUN+i));
    assert(feed(&r,"metrics\n")==CMD_METRICS);
    assert(feed(&r,"metrics reset\n")==CMD_METRICS_RESET);
    assert(feed(&r,"period 20\n")==CMD_PERIOD20);
    assert(feed(&r,"period 1\n")==CMD_PERIOD1);
    assert(feed(&r,"period 3\n")==CMD_UNKNOWN);
    assert(feed(&r,"\r\n")==CMD_NONE);
    assert(feed(&r,"run extra\n")==CMD_UNKNOWN);
    assert(feed(&r,"RUN\n")==CMD_UNKNOWN);
    for(unsigned i=0;i<80;++i) assert(protocol_feed(&r,'x')==CMD_NONE);
    assert(protocol_feed(&r,'\n')==CMD_BAD_LINE);
    assert(feed(&r,"status\n")==CMD_STATUS);
    assert(protocol_feed(&r,0)==CMD_NONE);
    assert(feed(&r,"run\n")==CMD_BAD_LINE);
    assert(protocol_feed(&r,255)==CMD_NONE);
    assert(protocol_feed(&r,'\n')==CMD_BAD_LINE);
    assert(feed(&r,"partial")==CMD_NONE); protocol_overflow(&r);
    assert(feed(&r,"\n")==CMD_BAD_LINE && feed(&r,"run\n")==CMD_RUN);
    puts("protocol: exact commands, CRLF, oversize/nonprintable/overflow recovery passed");
    return 0;
}
