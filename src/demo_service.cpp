// SPDX-License-Identifier: GPL-2.0-only
// Personal-device demo. No donor service is loaded into the system HAL process.
#include "vibrator_frontend.h"
#include "phone_backend.h"
#include "he_extension.h"
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
using namespace nyako;
namespace {
template<class T>T symbol(const char* name){auto p=reinterpret_cast<T>(dlsym(RTLD_DEFAULT,name));if(!p)throw std::runtime_error(name);return p;}
std::shared_ptr<VibratorFrontend> frontend;
std::shared_ptr<PhoneBackend> physical;
std::string waveRoot;
int header(AParcel* out,int exception=0){
    ndk::ScopedAStatus s(exception?AStatus_fromExceptionCode(exception):AStatus_newOk());
    return AParcel_writeStatusHeader(out,s.get());
}
void* create(void* p){return p;}
void destroy(void*){}
int extension(AIBinder*,transaction_code_t code,const AParcel* in,AParcel* out){
    int32_t a=0,b=0;int r=0;
    switch(code){
    case 10001:{ // Nyako rejected-feedback side channel: id, strength, owner.
        if(AIBinder_getCallingUid()!=1000)return header(out,EX_SECURITY);
        int64_t owner=0;r=AParcel_readInt32(in,&a);
        if(!r)r=AParcel_readInt32(in,&b);if(!r)r=AParcel_readInt64(in,&owner);if(r)return r;
        int duration=0,result=-EINVAL;
        try{
            Wave wave;WaveModel::append(wave,WaveModel(waveRoot).effect(a),WaveModel::strength(b));
            duration=WaveModel::duration(wave);
            result=physical->overlay(std::move(wave),static_cast<uint64_t>(owner));
        }catch(...){result=-EINVAL;}
        std::fprintf(stderr,"NYAKO parallel effect=%d strength=%d owner=%lld duration=%d result=%d\n",a,b,static_cast<long long>(owner),duration,result);
        r=header(out);return r?r:AParcel_writeInt32(out,result?result:duration);
    }
    case 10002:{
        if(AIBinder_getCallingUid()!=1000)return header(out,EX_SECURITY);
        int64_t owner=0;r=AParcel_readInt64(in,&owner);if(r)return r;
        physical->cancelOverlay(static_cast<uint64_t>(owner));return header(out);
    }
    case 1: // init(int,int), oneway: initialization has no hardware side effects.
        r=AParcel_readInt32(in,&a);return r?r:AParcel_readInt32(in,&b);
    case 5: frontend->off();return STATUS_OK; // oneway
    case 3:{ // stop(callback), oneway. Completion for the stopped effect is queued by off().
        AIBinder* cb=nullptr;r=AParcel_readStrongBinder(in,&cb);
        if(cb)AIBinder_decStrong(cb);if(!r)frontend->off();return r;
    }
    case 6:
        r=header(out);return r?r:AParcel_writeStrongBinder(out,frontend->asBinder().get());
    case 9:{
        r=AParcel_readInt32(in,&a);if(r)return r;bool unsupported=false;
        try{WaveModel(waveRoot).effect(a);}catch(...){unsupported=true;}
        std::fprintf(stderr,"NYAKO support_query effect=%d supported=%d\n",a,!unsupported);
        r=header(out);return r?r:AParcel_writeBool(out,unsupported);
    }
    case 12:
        r=AParcel_readInt32(in,&a);if(!r)r=AParcel_readInt32(in,&b);if(r)return r;
        std::fprintf(stderr,"NYAKO info_query vibrator=%d feature=%d\n",a,b);
        // Return the opaque stock motor ID; do not report the temporary 1016
        // donor synthesis profile as physical hardware. No calibration/dual claims.
        r=header(out);return r?r:AParcel_writeInt32(out,b==3?9999:0);
    case 16777215:r=header(out);return r?r:AParcel_writeInt32(out,3);
    case 16777214:
        r=header(out);return r?r:ndk::AParcel_writeString(out,std::string("ea8742d6993e1a82917da38b9938e537aa7fcb54"));
    case 2:case 4:case 10:return performHe(*frontend,waveRoot,code,in,out);
    case 8:
        // These are oneway transactions: exceptions cannot reach the caller.
        // Reject with no output and make the missing feature explicit in logs.
        std::fprintf(stderr,"NYAKO demo unsupported oneway extension=%u\n",code);
        return STATUS_UNKNOWN_TRANSACTION;
    case 7:case 11:case 13:case 14:case 15:case 16:case 17:
        std::fprintf(stderr,"NYAKO demo unsupported extension=%u\n",code);
        return header(out,EX_UNSUPPORTED_OPERATION);
    default:return STATUS_UNKNOWN_TRANSACTION;
    }
}
void mark(AIBinder* b){symbol<void(*)(AIBinder*)>("AIBinder_markVintfStability")(b);}
int inspect(){
    AIBinder* b=symbol<AIBinder*(*)(const char*)>("AServiceManager_checkService")("android.hardware.vibrator.IVibrator/default");
    if(!b)return 1;
    auto api=av::IVibrator::fromBinder(ndk::SpAIBinder(b));if(!api)return 2;
    std::vector<av::Effect> effects;int32_t caps=0;
    if(!api->getSupportedEffects(&effects).isOk()||!api->getCapabilities(&caps).isOk())return 3;
    bool crisp=false,soft=false;for(auto e:effects){crisp|=static_cast<int>(e)==53;soft|=static_cast<int>(e)==10053;}
    std::printf("caps=%d private_53=%d private_10053=%d\n",caps,crisp,soft);
    return crisp&&soft?0:4;
}
}
int main(int argc,char** argv){
    std::setvbuf(stderr,nullptr,_IOLBF,0);
    try{
        if(argc==2&&!std::strcmp(argv[1],"--check-service"))return inspect();
        if(argc!=4||std::strcmp(argv[1],"--serve")){std::fprintf(stderr,"usage: nyako-vibrator --serve WAVE_ROOT READY_FILE | --check-service\n");return 2;}
        waveRoot=argv[2];
        auto backend=std::make_shared<PhoneBackend>(true,true);physical=backend;
        std::fprintf(stderr,"NYAKO demo0.3.0-demo1 PCM concurrency ready; frequency=%g source=%s; transport_scale=1.00 strength_range=0.20..1.00; donor previews experimental\n",backend->measuredHz(),backend->liveCalibration()?"live":"recorded-stock");
        frontend=ndk::SharedRefBase::make<VibratorFrontend>(waveRoot,backend,backend->measuredHz(),backend->liveCalibration());
        ndk::SpAIBinder ext(AIBinder_new(AIBinder_Class_define("vendor.aac.hardware.richtap.vibrator.IRichtapVibrator",create,destroy,extension),nullptr));
        auto binder=frontend->asBinder();mark(binder.get());mark(ext.get());
        if(AIBinder_setExtension(binder.get(),ext.get()))throw std::runtime_error("cannot attach extension");
        symbol<bool(*)(uint32_t)>("ABinderProcess_setThreadPoolMaxThreadCount")(4);
        int r=symbol<int(*)(AIBinder*,const char*)>("AServiceManager_addService")(binder.get(),"android.hardware.vibrator.IVibrator/default");
        if(r)throw std::runtime_error("cannot register IVibrator/default");
        {std::ofstream f(argv[3]);f<<getpid()<<'\n';if(!f)throw std::runtime_error("cannot write ready marker");}
        std::fprintf(stderr,"NYAKO default Binder registered; demo0.3.0-demo1\n");
        symbol<void(*)()>("ABinderProcess_joinThreadPool")();return 1;
    }catch(const std::exception& e){std::fprintf(stderr,"NYAKO demo stopped: %s\n",e.what());return 1;}
}
