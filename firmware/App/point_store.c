#include "point_store.h"
#include <string.h>
#define SLOTS 128u
#define PAGE_SLOTS 64u
#define COMMIT 0xb34cu
static void put(uint8_t *p, uint32_t x, unsigned n) {
    for (unsigned i=0;i<n;++i) { p[i]=(uint8_t)x; x>>=8; }
}
static uint32_t get(const uint8_t *p, unsigned n) {
    uint32_t x=0;
    for (unsigned i=0;i<n;++i) x|=(uint32_t)p[i]<<(8*i);
    return x;
}
static void encode(uint8_t bytes[32], const ReferencePoint *p) {
    put(bytes,p->sequence,4); put(bytes+4,p->series,4);
    put(bytes+8,p->mark_ms,4); put(bytes+12,p->light_ms,4); put(bytes+16,p->pose_ms,4);
    put(bytes+20,p->raw,2); put(bytes+22,p->angle_cdeg,2);
    put(bytes+24,p->flags,2); put(bytes+26,COMMIT,2);
    put(bytes+28,records_crc(p,1),4);
}
static bool decode(const uint8_t b[32], ReferencePoint *p) {
    *p=(ReferencePoint){get(b,4),get(b+4,4),get(b+8,4),get(b+12,4),get(b+16,4),
        (uint16_t)get(b+20,2),(uint16_t)get(b+22,2),(uint16_t)get(b+24,2),0};
    return get(b+26,2)==COMMIT && p->sequence && p->series && get(b+28,4)==records_crc(p,1);
}
static bool erased(const uint8_t b[32]) {
    for (unsigned i=0;i<32;++i) if(b[i]!=255) return false;
    return true;
}
void point_store_open(PointStore *s, PointFlash f, PointHistory *h, uint32_t *series, uint32_t *sequence) {
    memset(s,0,sizeof(*s)); s->flash=f; memset(h,0,sizeof(*h));
    *series=0; *sequence=0;
    uint8_t b[32]; ReferencePoint p;
    unsigned latest=0; bool found=false;
    for (unsigned i=0;i<SLOTS;++i) {
        f.read(f.context,i*32,b,32);
        if (erased(b)) continue;
        if (!decode(b,&p)) { ++s->rejected; continue; }
        if (p.series>*series) *series=p.series;
        if (p.sequence>*sequence) { *sequence=p.sequence; latest=i; found=true; }
    }
    /* Physical order is chronological within the two-page circular journal. */
    for (unsigned n=1;n<=SLOTS;++n) {
        unsigned i=found ? (latest+n)%SLOTS : n-1;
        f.read(f.context,i*32,b,32);
        if (decode(b,&p)) records_append(h,p);
    }
    s->last_sequence=*sequence;
    s->slot=found ? (latest+1)%SLOTS : 0;
    /* Never program an interrupted slot again: non-erased C031 words may set PROGERR. */
    while (s->slot%PAGE_SLOTS) {
        f.read(f.context,s->slot*32,b,32);
        if (erased(b)) break;
        s->slot=(s->slot+1)%SLOTS;
    }
    s->erase_needed=s->slot%PAGE_SLOTS==0;
}
bool point_store_enqueue(PointStore *s, ReferencePoint p) {
    if (s->failed || s->count==STORE_QUEUE || p.reserved || !p.sequence || !p.series) {
        ++s->lost; return false;
    }
    s->queue[(s->head+s->count)%STORE_QUEUE]=p; ++s->count; return true;
}
void point_store_step(PointStore *s) {
    if (!s->count || s->failed) return;
    if (s->erase_needed) {
        if (!s->flash.erase(s->flash.context,s->slot/PAGE_SLOTS)) s->failed=true;
        else s->erase_needed=false;
        return;
    }
    uint8_t frame[32], verify[8];
    encode(frame,&s->queue[s->head]);
    unsigned offset=s->slot*32+s->part*8;
    if (!s->flash.program(s->flash.context,offset,frame+s->part*8)) { s->failed=true; return; }
    s->flash.read(s->flash.context,offset,verify,8);
    if (memcmp(verify,frame+s->part*8,8)) { s->failed=true; return; }
    if (++s->part==4) {
        s->last_sequence=s->queue[s->head].sequence;
        ++s->committed; s->head=(s->head+1)%STORE_QUEUE; --s->count; s->part=0;
        s->slot=(s->slot+1)%SLOTS;
        s->erase_needed=s->slot%PAGE_SLOTS==0;
    }
}
