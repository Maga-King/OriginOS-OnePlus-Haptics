// SPDX-License-Identifier: Apache-2.0
#include "pcm_mixer.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdio>
namespace nyako {
PcmMixer::PcmMixer(RtpTransport t):t_(t),thread_(&PcmMixer::run,this){}
PcmMixer::~PcmMixer(){
    {std::lock_guard<std::mutex> l(mutex_);closing_=true;}
    cv_.notify_all();thread_.join();
}
int PcmMixer::play(const Wave& wave,PlaybackState& state){
    auto job=std::make_shared<Job>();job->wave=wave;job->state=&state;
    std::unique_lock<std::mutex> l(mutex_);
    if(closing_)return -ECANCELED;
    // PlaybackQueue serializes the normal lane.
    if(base_)return -EBUSY;
    base_=job;cv_.notify_all();cv_.wait(l,[&]{return job->done;});
    return job->result;
}
int PcmMixer::overlay(Wave wave,uint64_t owner){
    if(!owner||wave.empty()||wave.size()>120u*24)return -EINVAL;
    auto job=std::make_shared<Job>();job->wave=std::move(wave);job->owner=owner;
    std::lock_guard<std::mutex> l(mutex_);
    if(closing_)return -ECANCELED;
    // The newest short feedback replaces only the short lane. No backlog.
    overlay_=std::move(job);cv_.notify_all();return 0;
}
void PcmMixer::cancelOverlay(uint64_t owner){
    std::lock_guard<std::mutex> l(mutex_);
    if(overlay_&&overlay_->owner==owner)overlay_.reset();
}
bool PcmMixer::render(int8_t* output,uint64_t frame){
    std::lock_guard<std::mutex> l(mutex_);
    if(closing_)return false;
    if(base_&&base_->state->cancel){
        base_->result=-ECANCELED;base_->end=frame-1;
        draining_.push_back(std::move(base_));
    }
    if(!base_&&!overlay_)return false;
    float gain=base_?base_->state->amplitude.load():1.f;
    // Match the stock FF_GAIN floor/range, independently for the normal lane.
    gain=(0x1333+std::lround(0x6ccc*gain))/32767.f;
    for(size_t i=0;i<frameSamples;i++){
        int b=0,o=0;
        if(base_&&base_->pos<base_->wave.size())b=std::lround(base_->wave[base_->pos++]*gain);
        if(overlay_&&overlay_->pos<overlay_->wave.size())o=overlay_->wave[overlay_->pos++];
        // Keep feedback intact; duck only overlapping normal samples when
        // their sum would clip. Alone, approved PCM remains byte-for-byte.
        int sum=b+o;
        if(sum>127||sum<-128){
            float headroom=std::max(0,127-std::abs(o));
            b=std::lround(b*headroom/127.f);sum=b+o;
        }
        output[i]=static_cast<int8_t>(std::clamp(sum,-128,127));
    }
    if(base_){
        base_->state->started();
        if(base_->pos==base_->wave.size()){base_->end=frame;draining_.push_back(std::move(base_));}
    }
    if(overlay_&&overlay_->pos==overlay_->wave.size())overlay_.reset();
    return true;
}
void PcmMixer::retire(uint64_t frame){
    std::lock_guard<std::mutex> l(mutex_);
    for(auto i=draining_.begin();i!=draining_.end();){
        if((*i)->end<=frame){(*i)->done=true;i=draining_.erase(i);}else ++i;
    }
    cv_.notify_all();
}
void PcmMixer::fail(int result){
    std::lock_guard<std::mutex> l(mutex_);
    if(base_){base_->state->started(result);base_->result=result;base_->done=true;base_.reset();}
    for(auto& j:draining_){j->state->started(result);j->result=result;j->done=true;}
    draining_.clear();overlay_.reset();cv_.notify_all();
}
void PcmMixer::run(){
    for(;;){
        {std::unique_lock<std::mutex> l(mutex_);cv_.wait(l,[&]{return closing_||base_||overlay_;});if(closing_)break;}
        int r=t_.command(t_.ctx,RTP_STOP,0);
        if(!r)r=t_.command(t_.ctx,RTP_GAIN,128);
        if(!r)r=t_.command(t_.ctx,RTP_STREAM,0);
        if(r){fail(r);continue;}
        uint64_t sequence=0,retired=0;unsigned index=0;
        std::array<uint64_t,4> slots{};
        int64_t started=t_.now_us(t_.ctx),progress=started;
        for(;;){
            {std::lock_guard<std::mutex> l(mutex_);if(closing_){r=-ECANCELED;break;}}
            int64_t now=t_.now_us(t_.ctx);
            // At most two frames ahead of the sample clock. Ring space alone
            // is not enough: the kernel may already have copied data into FIFO.
            if(sequence>=2&&now<started+static_cast<int64_t>(sequence-1)*10000){t_.sleep_us(t_.ctx,500);continue;}
            auto* s=&t_.slots[index];
            if(__atomic_load_n(&s->status,__ATOMIC_ACQUIRE)!=RTP_INVALID){
                if(now-progress>250000){r=-ETIMEDOUT;break;}
                t_.sleep_us(t_.ctx,500);continue;
            }
            progress=now;
            // A consumed slot plus the sample clock protects callback lifetime
            // from early kernel FIFO copies. Add one frame of drain margin.
            uint64_t clock=now>started+10000?static_cast<uint64_t>((now-started-10000)/10000):0;
            retired=std::max(retired,slots[index]);retire(std::min(retired,clock));
            if(!render(s->data,sequence+1)){
                // Keep the owner alive while queued samples physically drain.
                if(now<started+static_cast<int64_t>(sequence)*10000+10000){t_.sleep_us(t_.ctx,500);continue;}
                retire(sequence);break;
            }
            s->length=frameSamples;slots[index]=++sequence;
            __atomic_store_n(&s->status,RTP_VALID,__ATOMIC_RELEASE);
            index=(index+1)%4;
        }
        int stop=t_.command(t_.ctx,RTP_STOP,0);if(!r)r=stop;
        if(r){std::fprintf(stderr,"NYAKO mixer error=%d\n",r);fail(r);}
    }
    t_.command(t_.ctx,RTP_STOP,0);fail(-ECANCELED);
}
}
