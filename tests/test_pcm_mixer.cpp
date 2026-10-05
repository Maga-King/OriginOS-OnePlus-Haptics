// SPDX-License-Identifier: Apache-2.0
#include "pcm_mixer.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <future>
#include <mutex>
using namespace nyako;
struct FakeStream {
    RtpSlot slots[4]{};unsigned reader=0;bool streaming=false,stall=false;
    std::mutex mutex;std::vector<int8_t> samples;
    int starts=0,stops=0;int64_t next=0;
    static int64_t now(void*){
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    static int command(void* p,unsigned op,uintptr_t){
        auto& f=*static_cast<FakeStream*>(p);
        if(op==RTP_STOP){f.streaming=false;f.stops++;}
        if(op==RTP_STREAM){
            for(auto& s:f.slots){s.length=0;__atomic_store_n(&s.status,RTP_INVALID,__ATOMIC_RELEASE);}
            f.reader=0;f.streaming=true;f.starts++;f.next=now(p);
        }
        return 0;
    }
    static void sleep(void* p,unsigned us){
        auto& f=*static_cast<FakeStream*>(p);
        if(f.streaming&&!f.stall&&now(p)>=f.next){
            auto& s=f.slots[f.reader];
            if(__atomic_load_n(&s.status,__ATOMIC_ACQUIRE)==RTP_VALID){
                assert(s.length==240);
                {std::lock_guard<std::mutex> l(f.mutex);f.samples.insert(f.samples.end(),s.data,s.data+s.length);}
                __atomic_store_n(&s.status,RTP_INVALID,__ATOMIC_RELEASE);
                // Qualcomm's actual order: INVALID first, then length=0.
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                assert(__atomic_load_n(&s.status,__ATOMIC_ACQUIRE)==RTP_INVALID);
                __atomic_store_n(&s.length,0,__ATOMIC_RELEASE);
                f.reader=(f.reader+1)%4;f.next=now(p)+10000;
            }else if(__atomic_load_n(&s.status,__ATOMIC_ACQUIRE)==RTP_FINISHED){
                for(auto& slot:f.slots)__atomic_store_n(&slot.status,RTP_FINISHED,__ATOMIC_RELEASE);
                f.streaming=false;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
    static int cancelled(void*){return 0;}
    RtpTransport transport(){return {this,command,now,sleep,cancelled,slots};}
    bool has(int8_t value){std::lock_guard<std::mutex> l(mutex);return std::find(samples.begin(),samples.end(),value)!=samples.end();}
};
int main(){
    {
        FakeStream f;int directCalls=0;
        PcmMixer mixer(f.transport(),[&](const Wave& wave,PlaybackState& state){
            directCalls++;assert(wave.size()==264);state.started();return 0;
        });
        PlaybackState state;assert(mixer.play(Wave(264,20),state)==0);
        assert(directCalls==1&&f.starts==0); // A key click must not enter stream prefill.
    }
    {
        FakeStream f;PcmMixer mixer(f.transport());PlaybackState state;
        assert(mixer.play(Wave(24*50,20),state)==0);
        std::lock_guard<std::mutex> l(f.mutex);
        assert(f.samples.size()==24*50);for(auto v:f.samples)assert(v==20);
    }
    {
        FakeStream f;PcmMixer mixer(f.transport());PlaybackState state;
        auto normal=std::async(std::launch::async,[&]{return mixer.play(Wave(24*180,20),state);});
        assert(state.waitStarted()==0);
        assert(mixer.overlay(Wave(24*30,40),17)==0);
        mixer.cancelOverlay(18); // A different caller cannot cancel this feedback.
        assert(normal.get()==0);assert(f.has(60));assert(f.has(20));
        assert(f.starts==1); // Both lanes used the same physical stream.
    }
    {
        FakeStream f;PcmMixer mixer(f.transport());PlaybackState state;
        auto normal=std::async(std::launch::async,[&]{return mixer.play(Wave(24*200,20),state);});
        assert(state.waitStarted()==0);
        assert(mixer.overlay(Wave(24*100,40),21)==0);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));state.cancel=true;
        assert(normal.get()==-ECANCELED);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        assert(f.has(40)); // Normal off did not erase the short lane.
    }
    {
        FakeStream f;PcmMixer mixer(f.transport());PlaybackState state;
        auto normal=std::async(std::launch::async,[&]{return mixer.play(Wave(24*200,20),state);});
        assert(state.waitStarted()==0);
        assert(mixer.overlay(Wave(24*100,40),31)==0);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));mixer.cancelOverlay(31);
        assert(normal.get()==0);assert(f.has(60));
        std::lock_guard<std::mutex> l(f.mutex);assert(f.samples.back()==20);
    }
    {
        FakeStream f;f.stall=true;PcmMixer mixer(f.transport());PlaybackState state;
        assert(mixer.play(Wave(24*300,20),state)==-ETIMEDOUT);
    }
    std::cout<<"PASS unchanged PCM, concurrent feedback, independent cancellation, stalled driver\n";
}
