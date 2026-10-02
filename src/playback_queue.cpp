// SPDX-License-Identifier: Apache-2.0
#include "playback_queue.h"
#include <cerrno>
#include <cmath>
#include <chrono>
#include <stdexcept>
namespace mio {
void PlaybackState::started(int result){
    {std::lock_guard<std::mutex> l(readyMutex_);if(ready_)return;result_=result;ready_=true;}
    readyCv_.notify_all();
}
int PlaybackState::waitStarted(){
    std::unique_lock<std::mutex> l(readyMutex_);
    if(!readyCv_.wait_for(l,std::chrono::milliseconds(200),[&]{return ready_;}))return -ETIMEDOUT;
    return result_;
}
PlaybackQueue::PlaybackQueue(std::shared_ptr<PlaybackBackend> b):backend_(std::move(b)) {
    if(!backend_)throw std::invalid_argument("missing playback backend");
    thread_=std::thread(&PlaybackQueue::run,this);
    try{callbackThread_=std::thread(&PlaybackQueue::runCallbacks,this);}
    catch(...){ {std::lock_guard<std::mutex> l(mutex_);closing_=true;}cv_.notify_all();thread_.join();throw;}
}
PlaybackQueue::~PlaybackQueue(){
    std::shared_ptr<Job> dropped;
    {std::lock_guard<std::mutex> l(mutex_);closing_=true;dropped=std::move(pending_);if(current_)current_->state.cancel=true;}
    cv_.notify_all();thread_.join();notify(dropped,-ECANCELED);
    {std::lock_guard<std::mutex> l(callbackMutex_);callbacksClosing_=true;}
    callbackCv_.notify_all();callbackThread_.join();
}
void PlaybackQueue::notify(const std::shared_ptr<Job>& j,int result){
    if(!j)return;j->state.started(result);if(!j->complete)return;
    {std::lock_guard<std::mutex> l(callbackMutex_);completions_.emplace_back(j,result);}
    callbackCv_.notify_one();
}
void PlaybackQueue::dispatch(Complete cb,int result){
    auto job=std::make_shared<Job>();job->complete=std::move(cb);notify(job,result);
}
void PlaybackQueue::runCallbacks(){
    for(;;){
        std::pair<std::shared_ptr<Job>,int> item;
        {std::unique_lock<std::mutex> l(callbackMutex_);callbackCv_.wait(l,[&]{return callbacksClosing_||!completions_.empty();});
            if(completions_.empty()&&callbacksClosing_)break;
            item=std::move(completions_.front());completions_.pop_front();
        }
        // Slow/dead/reentrant recipients never own the output thread or lock.
        try{item.first->complete(item.second);}catch(...){}
    }
}
int PlaybackQueue::submit(Wave w,Complete complete,bool adjustable,bool waitForStart,Ticket* ticket){
    if(w.size()>WaveModel::maxMs*24u)throw std::invalid_argument("wave too long");
    auto next=std::make_shared<Job>();next->wave=std::move(w);next->complete=std::move(complete);next->adjustable=adjustable;
    std::shared_ptr<Job> dropped;
    {std::lock_guard<std::mutex> l(mutex_);
        if(closing_)throw std::runtime_error("playback closed");
        next->ticket=++sequence_;if(ticket)*ticket=next->ticket;
        if(current_)current_->state.cancel=true;
        dropped=std::move(pending_);pending_=next;
    }
    cv_.notify_all();notify(dropped,-ECANCELED);
    if(!waitForStart)return 0;
    int result=next->state.waitStarted();if(result)next->state.cancel=true;return result;
}
void PlaybackQueue::off(){
    cancel(0);
}
void PlaybackQueue::cancel(Ticket ticket){
    std::shared_ptr<Job> dropped,old;
    {std::unique_lock<std::mutex> l(mutex_);
        if(pending_&&(!ticket||pending_->ticket==ticket))dropped=std::move(pending_);
        if(current_&&(!ticket||current_->ticket==ticket))old=current_;
        if(old)old->state.cancel=true;
        cv_.notify_all();
        // Wait only for the job observed here, not one submitted concurrently.
        cv_.wait(l,[&]{return current_!=old||!old;});
    }
    notify(dropped,-ECANCELED);
}
void PlaybackQueue::amplitude(float value){
    if(!std::isfinite(value)||value<=0||value>1)throw std::invalid_argument("amplitude outside (0,1]");
    std::lock_guard<std::mutex> l(mutex_);
    // setAmplitude applies to on(), not prebaked/HE/compose effects.
    if(pending_&&pending_->adjustable)pending_->state.amplitude=value;
    if(current_&&current_->adjustable)current_->state.amplitude=value;
}
void PlaybackQueue::run(){
    for(;;){
        std::shared_ptr<Job> job;
        {std::unique_lock<std::mutex> l(mutex_);cv_.wait(l,[&]{return closing_||pending_;});
            if(closing_)break;
            job=std::move(pending_);current_=job;
        }
        int result=0;
        try{if(!job->wave.empty())result=backend_->play(job->wave,job->state);}catch(...){result=-EIO;}
        lastResult_=result;
        {std::lock_guard<std::mutex> l(mutex_);current_.reset();}
        cv_.notify_all();
        // State is cleared before callback: callback may safely call off/submit.
        notify(job,result);
    }
}
}
