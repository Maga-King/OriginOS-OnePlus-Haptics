// SPDX-License-Identifier: Apache-2.0
#include "he_extension.h"
#include <android/binder_parcel.h>
#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
using namespace nyako;
class Dry final:public PlaybackBackend {
public:bool amplitudeControl()const override{return true;}
int play(const Wave& w,PlaybackState& state)override{state.started();for(int i=0;i<WaveModel::duration(w);i++){if(state.cancel)return -ECANCELED;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return 0;}
};
std::shared_ptr<VibratorFrontend> frontend;std::string root;std::atomic<int> calls{0};
void* create(void*p){return p;}void destroy(void*){}
int callback(AIBinder*,transaction_code_t code,const AParcel* in,AParcel*){if(code!=1)return STATUS_UNKNOWN_TRANSACTION;int32_t value;int r=AParcel_readInt32(in,&value);if(!r){assert(value==1);calls++;}return r;}
int transact(AIBinder*,transaction_code_t code,const AParcel* in,AParcel* out){return performHe(*frontend,root,code,in,out);}
void ok(int r){assert(r==0);}
void wait(int n){for(int i=0;i<2500&&calls<n;i++)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(calls==n);}
int main(int argc,char**argv){
    assert(argc==2);root=argv[1];frontend=ndk::SharedRefBase::make<VibratorFrontend>(root,std::make_shared<Dry>(),132,false);
    ndk::SpAIBinder ext(AIBinder_new(AIBinder_Class_define("vendor.aac.hardware.richtap.vibrator.IRichtapVibrator",create,destroy,transact),nullptr));
    ndk::SpAIBinder cb(AIBinder_new(AIBinder_Class_define("vendor.aac.hardware.richtap.vibrator.IRichtapCallback",create,destroy,callback),nullptr));
    std::vector<int32_t> packet{2,2,23305,7,65537,0,0,1,4096,18,1,250,100,50,300,4,0,0,-40,20,10,-40,290,20,-40,300,0,-40};
    int expected=0;
    for(int code:{2,4,10}){
        for(bool cancel:{false,true}){
            AParcel *in=nullptr,*out=nullptr;ok(AIBinder_prepareTransaction(ext.get(),&in));
            for(int n:{1,0,255,0})ok(AParcel_writeInt32(in,n));ok(AParcel_writeInt32Array(in,packet.data(),packet.size()));ok(AParcel_writeStrongBinder(in,cb.get()));
            ok(AIBinder_transact(ext.get(),code,&in,&out,code==10?0:FLAG_ONEWAY));
            if(code==10){AStatus* status;ok(AParcel_readStatusHeader(out,&status));assert(AStatus_isOk(status));int32_t ms;ok(AParcel_readInt32(out,&ms));assert(ms==550);AStatus_delete(status);}
            if(in)AParcel_delete(in);if(out)AParcel_delete(out);
            if(cancel){std::this_thread::sleep_for(std::chrono::milliseconds(15));assert(frontend->on(30,nullptr).isOk());}
            wait(++expected);
        }
    }
    frontend->off();std::this_thread::sleep_for(std::chrono::milliseconds(600));assert(calls==6);
    std::cout<<"PASS HE Binder2/4/10 captured Copilot packet: 6 requests,6 callbacks; natural completion and standard preemption, no late duplicates\n";
}
