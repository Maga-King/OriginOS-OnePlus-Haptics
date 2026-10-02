// SPDX-License-Identifier: Apache-2.0
#include "playback_queue.h"
#include <cassert>
#include <chrono>
#include <cerrno>
#include <iostream>
#include <vector>
using namespace mio;
using namespace std::chrono_literals;
class Fake final:public PlaybackBackend {
public:
    std::atomic<int> active{0},started{0};std::atomic<float> amplitude{0};
    bool amplitudeControl() const override{return true;}
    int play(const Wave&,PlaybackState& state) override{
        assert(active.fetch_add(1)==0);started++;state.started();
        while(!state.cancel){amplitude=state.amplitude.load();std::this_thread::sleep_for(100us);}
        std::this_thread::sleep_for(1ms);assert(active.fetch_sub(1)==1);return -ECANCELED;
    }
};
class DelayedStart final:public PlaybackBackend {
public:
    std::atomic<bool> entered{false},release{false},cancelSeen{false};std::atomic<int> outputs{0};
    bool amplitudeControl() const override{return true;}
    int play(const Wave&,PlaybackState& state) override{
        entered=true;
        while(!release){if(state.cancel)cancelSeen=true;std::this_thread::sleep_for(100us);}
        if(state.cancel)return -ECANCELED;
        outputs++;state.started();return 0;
    }
};
template<class F>void until(F fn){auto end=std::chrono::steady_clock::now()+2s;while(!fn()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(100us);}}
int main(){
    auto b=std::make_shared<Fake>();PlaybackQueue q(b);std::atomic<int> callbacks{0};
    q.submit(Wave(100),[&](int r){assert(r==-ECANCELED);q.off();callbacks++;},true);
    until([&]{return b->started==1;});q.amplitude(.25f);until([&]{return b->amplitude==.25f;});
    q.off();assert(b->active==0);until([&]{return callbacks==1;});
    constexpr int n=1000;std::vector<std::atomic<int>> counts(n);for(auto& c:counts)c=0;
    for(int i=0;i<n;i++)q.submit(Wave(100),[&,i](int r){assert(r==-ECANCELED);counts[i]++;callbacks++;});
    q.off();until([&]{return callbacks==n+1;});for(auto& c:counts)assert(c==1);
    // A canceled callback may submit a replacement without stale cleanup killing it.
    int before=b->started;
    q.submit(Wave(100),[&](int){q.submit(Wave(100),[&](int){callbacks++;});callbacks++;});
    until([&]{return b->started>before;});q.off();
    until([&]{return callbacks>=n+2;});until([&]{return b->active==1;});q.off();
    until([&]{return callbacks==n+3;});
    // A callback blocked on its recipient must not delay the next physical job.
    std::atomic<bool> entered{false},release{false};
    before=b->started;q.submit(Wave(100),[&](int){entered=true;until([&]{return release.load();});});
    until([&]{return b->started>before;});q.off();until([&]{return entered.load();});
    before=b->started;q.submit(Wave(100));until([&]{return b->started>before;});q.off();release=true;
    // Old HE/standard completion cleanup carries its own generation ticket.
    PlaybackQueue::Ticket oldTicket,newTicket;
    assert(q.submit(Wave(100),{},false,true,&oldTicket)==0);
    assert(q.submit(Wave(100),{},false,true,&newTicket)==0);assert(oldTicket!=newTicket);
    q.cancel(oldTicket);assert(b->active==1);q.cancel(newTicket);assert(b->active==0);
    // Reproduce Qualcomm's documented off-before-worker-start race deterministically.
    auto delayed=std::make_shared<DelayedStart>();PlaybackQueue delayedQueue(delayed);
    delayedQueue.submit(Wave(100));until([&]{return delayed->entered.load();});
    std::thread stopper([&]{delayedQueue.off();});until([&]{return delayed->cancelSeen.load();});
    delayed->release=true;stopper.join();assert(delayed->outputs==0);
    // A start timeout must cancel the task; it may not fire after the caller failed.
    auto late=std::make_shared<DelayedStart>();PlaybackQueue lateQueue(late);
    assert(lateQueue.submit(Wave(100),{},false,true)==-ETIMEDOUT);
    until([&]{return late->cancelSeen.load();});late->release=true;lateQueue.off();assert(late->outputs==0);
    // Concurrent Binder callers cannot create a second hardware owner or lose callbacks.
    std::atomic<int> concurrentCallbacks{0};std::vector<std::thread> callers;
    for(int t=0;t<4;t++)callers.emplace_back([&]{for(int i=0;i<100;i++){q.submit(Wave(100),[&](int){concurrentCallbacks++;});if(i%7==0)q.off();}});
    for(auto& t:callers)t.join();q.off();until([&]{return concurrentCallbacks==400;});
    bool rejected=false;try{q.amplitude(0);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    std::cout<<"PASS 1000 replacements + 400 concurrent calls; exactly-once callbacks, reentry, blocked callback isolation, stale tickets, delayed-start cancel, start timeout, amplitude\n";
}
