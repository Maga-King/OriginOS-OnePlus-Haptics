// SPDX-License-Identifier: GPL-2.0-only
// ABI reference: OnePlusOSS qcom-hv-haptics.c and Maga-King/OP13HyperOSFix
// upstream/oneplus13_hyper_haptics_0.8.8/app/src/main/cpp/rtp_root_helper.c.
#include "rtp_backend.h"
#include <errno.h>
#include <math.h>
#include <string.h>

static int check(RtpTransport *t,int64_t deadline) {
    if(t->cancelled(t->ctx))return -ECANCELED;
    if(t->now_us(t->ctx)>=deadline)return -ETIMEDOUT;
    return 0;
}
static int wait_slot(RtpTransport *t,RtpSlot *s,uint8_t status,int64_t deadline) {
    for(;;){
        int r=check(t,deadline);if(r)return r;
        if(__atomic_load_n(&s->status,__ATOMIC_ACQUIRE)==status)return 0;
        t->sleep_us(t->ctx,500);
    }
}
static void scale_copy(int8_t *dst,const int8_t *src,size_t n,float scale) {
    for(size_t i=0;i<n;i++)dst[i]=(int8_t)lrintf((float)src[i]*scale);
}
int rtp_play(RtpTransport *t,const int8_t *data,size_t n,float scale) {
    if(!t||!data||!n||n>24000u*120u||!isfinite(scale)||scale<0||scale>1)return -EINVAL;
    if(t->cancelled(t->ctx))return -ECANCELED;
    int r=t->command(t->ctx,RTP_STOP,0);if(r)return r;
    if(t->cancelled(t->ctx))return -ECANCELED;
    r=t->command(t->ctx,RTP_GAIN,128);if(r)return r;
    if(t->cancelled(t->ctx))return -ECANCELED;
    size_t padded=(n+3u)&~3u;
    int64_t duration_us=(int64_t)((n*1000000ull+23999)/24000);
    // 250ms includes driver startup and drain; never a multi-second open-ended wait.
    int64_t deadline=t->now_us(t->ctx)+duration_us+250000;
    if(padded<=3996){
        struct {uint32_t length;int8_t data[3996];} packet={0};
        packet.length=(uint32_t)padded;scale_copy(packet.data,data,n,scale);
        r=t->command(t->ctx,RTP_DIRECT,(uintptr_t)&packet);if(r)goto end;
        int64_t finish=t->now_us(t->ctx)+duration_us+3000;
        while(t->now_us(t->ctx)<finish){r=check(t,deadline);if(r)goto end;t->sleep_us(t->ctx,500);}
    }else{
        if(!t->slots){r=-ENODEV;goto end;}
        r=t->command(t->ctx,RTP_STREAM,0);if(r)goto end;
        unsigned index=0;size_t pos=0;
        while(pos<n){
            RtpSlot *s=&t->slots[index];r=wait_slot(t,s,RTP_INVALID,deadline);if(r)goto end;
            size_t len=n-pos;if(len>1000)len=1000;
            size_t pad=(len+3u)&~3u;scale_copy(s->data,data+pos,len,scale);
            memset(s->data+len,0,pad-len);s->length=(int16_t)pad;
            __atomic_store_n(&s->status,RTP_VALID,__ATOMIC_RELEASE);
            pos+=len;index=(index+1)%4;
        }
        RtpSlot *last=&t->slots[index];r=wait_slot(t,last,RTP_INVALID,deadline);if(r)goto end;
        last->length=0;__atomic_store_n(&last->status,RTP_FINISHED,__ATOMIC_RELEASE);
        for(;;){
            r=check(t,deadline);if(r)goto end;
            int done=1;
            for(int i=0;i<4;i++)if(__atomic_load_n(&t->slots[i].status,__ATOMIC_ACQUIRE)!=RTP_FINISHED)done=0;
            if(done)break;t->sleep_us(t->ctx,500);
        }
    }
end:
    // Stop also joins the kernel's erase worker before a subsequent effect begins.
    {int stop=t->command(t->ctx,RTP_STOP,0);if(!r)r=stop;}
    return r;
}
