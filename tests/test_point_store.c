#include "point_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[4096];
static int fail_after=-1;
static bool silently_ignore;
static unsigned operations;
static void read_flash(void *ctx,unsigned at,void *p,unsigned n) {
    (void)ctx; assert(at+n<=sizeof(flash)); memcpy(p,flash+at,n);
}
static bool fail(void) { ++operations; return fail_after>=0 && fail_after--==0; }
static bool erase_flash(void *ctx,unsigned page) {
    (void)ctx; assert(page<2); if(fail()) return false;
    memset(flash+page*2048,255,2048); return true;
}
static bool program_flash(void *ctx,unsigned at,const uint8_t bytes[8]) {
    (void)ctx; assert(at%8==0 && at+8<=sizeof(flash));
    if(fail()) return false;
    if(silently_ignore) return true;
    for(unsigned i=0;i<8;++i) { assert(flash[at+i]==255); flash[at+i]&=bytes[i]; }
    return true;
}
static PointFlash port(void) { return (PointFlash){NULL,read_flash,erase_flash,program_flash}; }
static ReferencePoint point(unsigned n) {
    return (ReferencePoint){n,1+(n-1)/7,123*n,123*n-7,123*n-3,(uint16_t)(n%4096),9000,(uint16_t)(n%64),0};
}
static void drain(PointStore *s) { while(s->count && !s->failed) point_store_step(s); }
static void append(PointStore *s,unsigned n) { assert(point_store_enqueue(s,point(n))); drain(s); assert(!s->failed); }
static void open_store(PointStore *s,PointHistory *h,uint32_t *series,uint32_t *seq) { point_store_open(s,port(),h,series,seq); }
int main(void) {
    PointStore s; PointHistory h; uint32_t series,seq;
    memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq);
    assert(!h.count && !seq && !series);
    for(unsigned n=1;n<=320;++n) {
        append(&s,n);
        PointStore recovered; PointHistory history;
        open_store(&recovered,&history,&series,&seq);
        assert(seq==n && series==point(n).series && history.count==(n<64?n:64));
        PointSnapshot snap; records_snapshot(&history,&snap);
        for(unsigned i=0;i<snap.count;++i) {
            ReferencePoint expected=point(n-snap.count+1+i);
            assert(!memcmp(&snap.items[i],&expected,sizeof(expected)));
        }
    }
    /* Reboot after every operation, with a full other page. */
    for(unsigned cut=0;cut<=5;++cut) {
        memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq);
        for(unsigned n=1;n<=128;++n) append(&s,n);
        assert(point_store_enqueue(&s,point(129)));
        for(unsigned j=0;j<cut;++j) point_store_step(&s);
        open_store(&s,&h,&series,&seq);
        assert(seq==(cut==5?129u:128u));
        assert(h.count==64);
        append(&s,seq+1);
        open_store(&s,&h,&series,&seq); assert(h.count==64);
    }
    /* Every byte of an interrupted first record, including a partial commit. */
    for(unsigned cut=1;cut<32;++cut) {
        memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq); append(&s,1);
        uint8_t complete[32]; memcpy(complete,flash,32);
        memset(flash,255,sizeof(flash)); memcpy(flash,complete,cut);
        open_store(&s,&h,&series,&seq); assert(!h.count && s.rejected==1);
        append(&s,1); open_store(&s,&h,&series,&seq); assert(seq==1 && h.count==1);
    }
    memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq); append(&s,1);
    for(unsigned byte=0;byte<32;++byte) {
        flash[byte]^=1; open_store(&s,&h,&series,&seq); assert(!h.count && s.rejected==1); flash[byte]^=1;
    }
    for(unsigned failure=0;failure<5;++failure) {
        memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq);
        fail_after=(int)failure; assert(point_store_enqueue(&s,point(1)));drain(&s);
        assert(s.failed && !s.committed); assert(!point_store_enqueue(&s,point(2)));
        fail_after=-1; open_store(&s,&h,&series,&seq); assert(!h.count);
    }
    /* Partial erase of the oldest page must leave the other page's 64 intact. */
    for(unsigned erased_bytes=0;erased_bytes<=2048;erased_bytes+=8) {
        memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq);
        for(unsigned n=1;n<=128;++n) append(&s,n);
        memset(flash,255,erased_bytes);
        open_store(&s,&h,&series,&seq); assert(seq==128 && h.count==64);
        PointSnapshot snap; records_snapshot(&h,&snap);
        for(unsigned n=0;n<64;++n) assert(snap.items[n].sequence==65+n);
        append(&s,129); open_store(&s,&h,&series,&seq); assert(seq==129 && h.count==64);
    }
    memset(flash,255,sizeof(flash)); open_store(&s,&h,&series,&seq);
    for(unsigned i=1;i<=STORE_QUEUE;++i) assert(point_store_enqueue(&s,point(i)));
    assert(!point_store_enqueue(&s,point(9)) && s.lost==1); drain(&s); assert(s.committed==8);
    silently_ignore=true; assert(point_store_enqueue(&s,point(9))); drain(&s); assert(s.failed);
    puts("point_store: 320 resets, 6 rollover cuts, 31 byte cuts, 32 CRC corruptions, 257 partial erases, 5 I/O failures, queue and readback passed");
    return 0;
}
