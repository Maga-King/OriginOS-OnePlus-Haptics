// SPDX-License-Identifier: Apache-2.0
#include "he_extension.h"
#include "he_model.h"
#include <android/binder_parcel.h>
#include <cstdio>
#include <stdexcept>
namespace mio {
namespace {
bool allocate(void* arg,int32_t n,int32_t** out){
    if(n<0||n>65536)return false;auto& p=*static_cast<std::vector<int32_t>*>(arg);
    p.resize(n);*out=p.data();return true;
}
void* create(void* p){return p;}void destroy(void*){}
binder_status_t unknown(AIBinder*,transaction_code_t,const AParcel*,AParcel*){return STATUS_UNKNOWN_TRANSACTION;}
AIBinder_Class* callbackClass(){static auto c=AIBinder_Class_define("vendor.aac.hardware.richtap.vibrator.IRichtapCallback",create,destroy,unknown);return c;}
void notify(const ndk::SpAIBinder& cb,int result){
    if(!cb.get())return;
    AParcel *in=nullptr,*out=nullptr;int r=AIBinder_prepareTransaction(cb.get(),&in);
    if(!r)r=AParcel_writeInt32(in,result);
    if(!r)r=AIBinder_transact(cb.get(),1,&in,&out,FLAG_ONEWAY);
    if(in)AParcel_delete(in);if(out)AParcel_delete(out);
    std::fprintf(stderr,"MIO HE callback value=%d status=%d\n",result,r);
}
}
binder_status_t performHe(VibratorFrontend& frontend,const std::string& root,transaction_code_t code,const AParcel* in,AParcel* out){
    int32_t args[4];std::vector<int32_t> packet;AIBinder* raw=nullptr;
    for(auto& arg:args){int r=AParcel_readInt32(in,&arg);if(r)return r;}
    int r=AParcel_readInt32Array(in,&packet,allocate);if(r)return r;
    r=AParcel_readStrongBinder(in,&raw);if(r)return r;ndk::SpAIBinder cb(raw);
    if(raw&&AIBinder_isRemote(raw)&&!AIBinder_associateClass(raw,callbackClass()))return STATUS_BAD_TYPE;
    int exception=0,duration=0;
    try{
        auto wave=HeModel(root).render(packet,args[0],args[1],args[2],args[3]);duration=WaveModel::duration(wave);
        int played=frontend.playVendor(std::move(wave),[cb](int result){notify(cb,result&&result!=-ECANCELED?-1:1);});
        if(played&&played!=-ECANCELED)exception=EX_ILLEGAL_STATE;
    }catch(const std::invalid_argument&){exception=EX_ILLEGAL_ARGUMENT;}
     catch(const std::out_of_range&){exception=EX_UNSUPPORTED_OPERATION;}
     catch(...){exception=EX_ILLEGAL_STATE;}
    std::fprintf(stderr,"MIO HE tx=%u format=%d ints=%zu loops=%d interval=%d amplitude=%d frequency=%d duration=%d exception=%d\n",code,packet.empty()?-1:packet[0],packet.size(),args[0]&0xffff,args[1],args[2],args[3],duration,exception);
    if(code!=10)return exception?STATUS_BAD_VALUE:STATUS_OK;
    ndk::ScopedAStatus status(exception?AStatus_fromExceptionCode(exception):AStatus_newOk());
    r=AParcel_writeStatusHeader(out,status.get());return r||exception?r:AParcel_writeInt32(out,duration);
}
}
