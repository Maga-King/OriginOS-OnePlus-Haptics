// SPDX-License-Identifier: Apache-2.0
#include "vibrator_frontend.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace nyako {
static Status unsupported(){return Status::fromExceptionCode(EX_UNSUPPORTED_OPERATION);}
static Status invalid(){return Status::fromExceptionCode(EX_ILLEGAL_ARGUMENT);}
static Status failure(){return Status::fromExceptionCode(EX_ILLEGAL_STATE);}
VibratorFrontend::VibratorFrontend(std::string root,std::shared_ptr<PlaybackBackend> backend,float hz,bool live):model_(std::move(root)),queue_(std::move(backend)),measuredHz_(hz),liveCalibration_(live){
    if(!std::isfinite(hz)||hz<=0)throw std::invalid_argument("invalid measured resonance");
}
Status VibratorFrontend::start(Wave w,const Callback& cb,bool adjustable){
    std::unique_lock<std::recursive_mutex> command;
    if(commandMutex_)command=std::unique_lock<std::recursive_mutex>(*commandMutex_);
    try{if(beforeStandard_)beforeStandard_();int result=queue_.submit(std::move(w),[cb](int result){
        if(result&&result!=-ECANCELED)std::fprintf(stderr,"NYAKO playback failed: %d\n",result);
        if(cb){auto status=cb->onComplete();if(!status.isOk())std::fprintf(stderr,"NYAKO callback failed: %s\n",status.getDescription().c_str());}
    },adjustable,true);
    return result&&result!=-ECANCELED?Status::fromServiceSpecificError(-result):Status::ok();}catch(...){return failure();}
}
int VibratorFrontend::playRaw(Wave input,float gain,PlaybackQueue::Ticket* ticket){
    try{Wave w;WaveModel::append(w,input,gain);return queue_.submit(std::move(w),{},false,true,ticket);}
    catch(...){return -EINVAL;}
}
int VibratorFrontend::playVendor(Wave wave,PlaybackQueue::Complete complete){
    std::unique_lock<std::recursive_mutex> command;
    if(commandMutex_)command=std::unique_lock<std::recursive_mutex>(*commandMutex_);
    return queue_.submit(std::move(wave),std::move(complete),false,true);
}
Status VibratorFrontend::getCapabilities(int32_t* out){
    *out=CAP_ON_CALLBACK|CAP_PERFORM_CALLBACK|CAP_COMPOSE_EFFECTS;
    if(liveCalibration_)*out|=CAP_GET_RESONANT_FREQUENCY;
    if(queue_.amplitudeControl())*out|=CAP_AMPLITUDE_CONTROL;
    return Status::ok();
}
Status VibratorFrontend::off(){
    std::unique_lock<std::recursive_mutex> command;
    if(commandMutex_)command=std::unique_lock<std::recursive_mutex>(*commandMutex_);
    if(beforeStandard_)beforeStandard_();queue_.off();return Status::ok();
}
Status VibratorFrontend::on(int32_t ms,const Callback& cb){
    if(ms<=0||ms>WaveModel::maxMs)return invalid();
    try{return start(WaveModel::ramp({{1,measuredHz_,1,measuredHz_,ms}},.34f),cb,true);}catch(...){return failure();}
}
Status VibratorFrontend::perform(av::Effect effect,av::EffectStrength strength,const Callback& cb,int32_t* duration){
    *duration=0;float gain;
    try{gain=WaveModel::strength(static_cast<int>(strength));}catch(...){return invalid();}
    try{Wave w;WaveModel::append(w,model_.effect(static_cast<int>(effect)),gain);
        int ms=WaveModel::duration(w);auto status=start(std::move(w),cb);
        if(status.isOk())*duration=ms;
#ifdef NYAKO_DEMO
        std::fprintf(stderr,"NYAKO perform effect=%d strength=%d gain=%.5f duration=%d status=%s\n",static_cast<int>(effect),static_cast<int>(strength),gain,*duration,status.isOk()?"OK":"FAILED");
#endif
        return status;
    }catch(const std::out_of_range&){std::fprintf(stderr,"NYAKO unmapped effect=%d strength=%d gain=%.5f\n",static_cast<int>(effect),static_cast<int>(strength),gain);return unsupported();}catch(...){return failure();}
}
Status VibratorFrontend::getSupportedEffects(std::vector<av::Effect>* out){
    out->clear();for(int id:model_.supportedEffects())out->push_back(static_cast<av::Effect>(id));
    return Status::ok();
}
Status VibratorFrontend::setAmplitude(float a){
    if(!std::isfinite(a)||a<=0||a>1)return invalid();
    if(!queue_.amplitudeControl())return unsupported();queue_.amplitude(a);
#ifdef NYAKO_DEMO
    std::fprintf(stderr,"NYAKO setAmplitude value=%.5f\n",a);
#endif
    return Status::ok();
}
Status VibratorFrontend::setExternalControl(bool){return unsupported();}
Status VibratorFrontend::getCompositionDelayMax(int32_t* out){*out=1000;return Status::ok();}
Status VibratorFrontend::getCompositionSizeMax(int32_t* out){*out=256;return Status::ok();}
Status VibratorFrontend::getSupportedPrimitives(std::vector<av::CompositePrimitive>* out){
    out->clear();for(int i=0;i<9;i++)out->push_back(static_cast<av::CompositePrimitive>(i));return Status::ok();
}
Status VibratorFrontend::getPrimitiveDuration(av::CompositePrimitive p,int32_t* out){
    *out=0;try{*out=WaveModel::duration(model_.primitive(static_cast<int>(p)));return Status::ok();}
    catch(const std::out_of_range&){return unsupported();}catch(...){return failure();}
}
Status VibratorFrontend::compose(const std::vector<av::CompositeEffect>& effects,const Callback& cb){
    if(effects.empty()||effects.size()>256)return invalid();Wave w;
    try{for(const auto& e:effects){
        if(e.delayMs<0||e.delayMs>1000||!std::isfinite(e.scale)||e.scale<0||e.scale>1)return invalid();
        WaveModel::append(w,Wave(static_cast<size_t>(e.delayMs)*24));
        WaveModel::append(w,model_.primitive(static_cast<int>(e.primitive)),e.scale);
    }
    return start(std::move(w),cb);}catch(const std::out_of_range&){return unsupported();}
    catch(const std::invalid_argument&){return invalid();}catch(...){return failure();}
}
// These interfaces exist, but capability bits stay off until their underlying
// hardware/audio/calibration paths are implemented and measured on this phone.
// Returning success here (as the donor does) would hide missing functionality.
Status VibratorFrontend::getSupportedAlwaysOnEffects(std::vector<av::Effect>* out){out->clear();return unsupported();}
Status VibratorFrontend::alwaysOnEnable(int32_t,av::Effect,av::EffectStrength){return unsupported();}
Status VibratorFrontend::alwaysOnDisable(int32_t){return unsupported();}
Status VibratorFrontend::getResonantFrequency(float* out){if(!liveCalibration_)return unsupported();*out=measuredHz_;return Status::ok();}
Status VibratorFrontend::getQFactor(float*){return unsupported();}
Status VibratorFrontend::getFrequencyResolution(float*){return unsupported();}
Status VibratorFrontend::getFrequencyMinimum(float*){return unsupported();}
Status VibratorFrontend::getBandwidthAmplitudeMap(std::vector<float>* out){out->clear();return unsupported();}
Status VibratorFrontend::getPwlePrimitiveDurationMax(int32_t*){return unsupported();}
Status VibratorFrontend::getPwleCompositionSizeMax(int32_t*){return unsupported();}
Status VibratorFrontend::getSupportedBraking(std::vector<av::Braking>* out){out->clear();return unsupported();}
Status VibratorFrontend::composePwle(const std::vector<av::PrimitivePwle>&,const Callback&){return unsupported();}
}
