// SPDX-License-Identifier: GPL-2.0-only
#include "rtp_backend.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int64_t now,cancel_at;
    unsigned index,stops,gain,stream,stuck;
    int8_t captured[262144];size_t n;
    RtpSlot slots[4];
} Fake;
static int command(void *ctx,unsigned op,uintptr_t arg){
    Fake *f=ctx;
    if(op==RTP_STOP){f->stops++;f->stream=0;return 0;}
    if(op==RTP_GAIN){f->gain=arg;return 0;}
    if(op==RTP_DIRECT){uint32_t n=*(uint32_t *)arg;assert(n<=3996&&n%4==0);memcpy(f->captured,(char *)arg+4,n);f->n=n;return 0;}
    assert(op==RTP_STREAM);f->stream=1;for(int i=0;i<4;i++)f->slots[i].status=RTP_INVALID;return 0;
}
static int64_t now(void *ctx){return ((Fake *)ctx)->now;}
static int cancelled(void *ctx){Fake *f=ctx;return f->cancel_at>0&&f->now>=f->cancel_at;}
static void sleep_us(void *ctx,unsigned us){
    Fake *f=ctx;f->now+=us;
    if(!f->stream||f->stuck)return;
    RtpSlot *s=&f->slots[f->index];
    if(s->status==RTP_VALID){assert(s->length>0&&s->length<=1000&&s->length%4==0);memcpy(f->captured+f->n,s->data,s->length);f->n+=s->length;s->status=RTP_INVALID;f->index=(f->index+1)%4;}
    else if(s->status==RTP_FINISHED){for(int i=0;i<4;i++)f->slots[i].status=RTP_FINISHED;f->stream=0;}
}
static void test(size_t len,int cancel,int stuck,int expected){
    Fake f={.cancel_at=cancel?1000:0,.stuck=stuck};RtpTransport t={&f,command,now,sleep_us,cancelled,f.slots};
    int8_t *data=malloc(len);for(size_t i=0;i<len;i++)data[i]=(int8_t)((i%255)-127);
    int r=rtp_play(&t,data,len,1);assert(r==expected);assert(f.stops==2);assert(f.gain==128);
    if(!r){assert(f.n==((len+3)&~3u));assert(!memcmp(data,f.captured,len));for(size_t i=len;i<f.n;i++)assert(f.captured[i]==0);}
    if(cancel)assert(f.now<=1500);
    if(stuck)assert(f.now<(int64_t)(len*1000000ull/24000)+251000);
    free(data);printf("PASS len=%zu cancel=%d stalled=%d result=%d elapsed_us=%lld\n",len,cancel,stuck,r,(long long)f.now);
}
int main(void){
    const size_t lengths[]={1,3,4,432,2400,3995,3996,3997,4000,4001,16001,65533,139200,240001};
    for(size_t i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++)test(lengths[i],0,0,0);
    test(2400,1,0,-ECANCELED);test(16001,1,0,-ECANCELED);test(16001,0,1,-ETIMEDOUT);
    puts("All RTP transport boundary, tail, wraparound, cancellation and timeout tests passed.");
}
