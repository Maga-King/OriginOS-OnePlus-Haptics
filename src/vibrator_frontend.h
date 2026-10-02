// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "playback_queue.h"
#include <aidl/android/hardware/vibrator/BnVibrator.h>
namespace mio {
namespace av=aidl::android::hardware::vibrator;
using Status=ndk::ScopedAStatus;
using Callback=std::shared_ptr<av::IVibratorCallback>;
class VibratorFrontend final:public av::BnVibrator {
public:
    VibratorFrontend(std::string stockRoot,std::shared_ptr<PlaybackBackend> backend,float measuredHz,bool liveCalibration=true);
    Status getCapabilities(int32_t*) override;
    Status off() override;
    Status on(int32_t,const Callback&) override;
    Status perform(av::Effect,av::EffectStrength,const Callback&,int32_t*) override;
    Status getSupportedEffects(std::vector<av::Effect>*) override;
    Status setAmplitude(float) override;
    Status setExternalControl(bool) override;
    Status getCompositionDelayMax(int32_t*) override;
    Status getCompositionSizeMax(int32_t*) override;
    Status getSupportedPrimitives(std::vector<av::CompositePrimitive>*) override;
    Status getPrimitiveDuration(av::CompositePrimitive,int32_t*) override;
    Status compose(const std::vector<av::CompositeEffect>&,const Callback&) override;
    Status getSupportedAlwaysOnEffects(std::vector<av::Effect>*) override;
    Status alwaysOnEnable(int32_t,av::Effect,av::EffectStrength) override;
    Status alwaysOnDisable(int32_t) override;
    Status getResonantFrequency(float*) override;
    Status getQFactor(float*) override;
    Status getFrequencyResolution(float*) override;
    Status getFrequencyMinimum(float*) override;
    Status getBandwidthAmplitudeMap(std::vector<float>*) override;
    Status getPwlePrimitiveDurationMax(int32_t*) override;
    Status getPwleCompositionSizeMax(int32_t*) override;
    Status getSupportedBraking(std::vector<av::Braking>*) override;
    Status composePwle(const std::vector<av::PrimitivePwle>&,const Callback&) override;
    void setBeforeStandard(std::function<void()> hook){beforeStandard_=std::move(hook);}
    void setCommandMutex(std::recursive_mutex* mutex){commandMutex_=mutex;}
    int playRaw(Wave,float,PlaybackQueue::Ticket*);
    int playVendor(Wave,PlaybackQueue::Complete);
    void cancelRaw(PlaybackQueue::Ticket ticket){if(ticket)queue_.cancel(ticket);}
    void cancelAll(){queue_.off();}
    void dispatch(PlaybackQueue::Complete cb,int result){queue_.dispatch(std::move(cb),result);}
private:
    Status start(Wave,const Callback&,bool adjustable=false);
    WaveModel model_;
    PlaybackQueue queue_;
    const float measuredHz_;
    const bool liveCalibration_;
    std::function<void()> beforeStandard_;
    std::recursive_mutex* commandMutex_=nullptr;
};
}
