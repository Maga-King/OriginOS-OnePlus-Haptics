// SPDX-License-Identifier: Apache-2.0
#include "vibrator_frontend.h"
#include "phone_backend.h"
#include <aidl/android/hardware/vibrator/BpVibrator.h>
#include <aidl/android/hardware/vibrator/BnVibratorCallback.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
using namespace mio;
using namespace std::chrono_literals;
class DryBackend final:public PlaybackBackend {
public:
    std::atomic<int> jobs{0};std::atomic<float> amplitude{1};
    bool amplitudeControl() const override{return true;}
    int play(const Wave& w,PlaybackState& state) override{
        jobs++;state.started();auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(WaveModel::duration(w));
        while(std::chrono::steady_clock::now()<end){if(state.cancel)return -ECANCELED;amplitude=state.amplitude.load();std::this_thread::sleep_for(500us);}return 0;
    }
};
class CallbackCounter final:public av::BnVibratorCallback {
public:std::atomic<int> calls{0};Status onComplete() override{calls++;return Status::ok();}
};
template<class F>void until(F f){auto end=std::chrono::steady_clock::now()+3s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(1ms);}}
static void ok(Status s){if(!s.isOk())std::cerr<<s.getDescription()<<std::endl;assert(s.isOk());}
static void unsupported(Status s){assert(s.getExceptionCode()==EX_UNSUPPORTED_OPERATION);}
static void invalid(Status s){assert(s.getExceptionCode()==EX_ILLEGAL_ARGUMENT);}
int main(int argc,char** argv){
    bool physical=argc>1&&std::string(argv[1])=="--physical";
    auto dry=std::make_shared<DryBackend>();std::shared_ptr<PhoneBackend> phone;
    if(physical)phone=std::make_shared<PhoneBackend>();
    std::shared_ptr<PlaybackBackend> backend=physical?std::static_pointer_cast<PlaybackBackend>(phone):dry;
    std::string waveRoot=argc>2&&std::string(argv[1])=="--waves"?argv[2]:"/odm/etc/vibrator/9999";
    auto impl=ndk::SharedRefBase::make<VibratorFrontend>(waveRoot,backend,physical?phone->measuredHz():132);
    // Force generated Bp->Parcel->Bn marshalling even inside the isolated process.
    auto api=ndk::SharedRefBase::make<av::BpVibrator>(impl->asBinder());
    auto cb=ndk::SharedRefBase::make<CallbackCounter>();
    int32_t caps=0,ms=0,v=0;float hz=0;std::string hash;
    ok(api->getCapabilities(&caps));assert(caps==167);
    ok(api->getInterfaceVersion(&v));assert(v==2);ok(api->getInterfaceHash(&hash));assert(hash=="ea8742d6993e1a82917da38b9938e537aa7fcb54");
    ok(api->getResonantFrequency(&hz));assert(hz==132);
    if(physical){
        ok(api->perform(av::Effect::CLICK,av::EffectStrength::LIGHT,cb,&ms));until([&]{return cb->calls==1;});
        std::this_thread::sleep_for(100ms);
        ok(api->perform(static_cast<av::Effect>(10000),av::EffectStrength::LIGHT,cb,&ms));until([&]{return cb->calls==2;});
        ok(api->on(250,cb));std::this_thread::sleep_for(45ms);ok(api->setAmplitude(.25f));std::this_thread::sleep_for(45ms);ok(api->setAmplitude(.65f));std::this_thread::sleep_for(45ms);ok(api->off());
        until([&]{return cb->calls==3;});assert(phone->gainWrites()>=5);
        std::cout<<"PASS bounded physical Binder frontend: crisp/soft, amplitude writes="<<phone->gainWrites()<<", cancellation callbacks=3\n";
        return 0;
    }
    std::vector<av::Effect> effects;ok(api->getSupportedEffects(&effects));assert(effects.size()==765);
    for(auto e:effects){if(static_cast<int>(e)>5&&static_cast<int>(e)!=21)continue;ok(api->perform(e,av::EffectStrength::MEDIUM,cb,&ms));assert(ms>0&&ms<150);ok(api->off());}
    until([&]{return cb->calls==7;});
    ok(api->perform(static_cast<av::Effect>(10000),static_cast<av::EffectStrength>(59),cb,&ms));assert(ms==29);ok(api->off());until([&]{return cb->calls==8;});
    // Binder does not marshal return values when an exception is returned.
    unsupported(api->perform(static_cast<av::Effect>(123456),av::EffectStrength::LIGHT,cb,&ms));
    invalid(api->perform(av::Effect::CLICK,static_cast<av::EffectStrength>(60),cb,&ms));
    invalid(api->on(-1,cb));invalid(api->on(120001,cb));invalid(api->setAmplitude(0));invalid(api->setAmplitude(std::numeric_limits<float>::quiet_NaN()));
    ok(api->on(200,cb));until([&]{return dry->jobs>0;});ok(api->setAmplitude(.25f));until([&]{return dry->amplitude==.25f;});ok(api->off());until([&]{return cb->calls==9;});
    std::vector<av::CompositePrimitive> primitives;ok(api->getSupportedPrimitives(&primitives));assert(primitives.size()==9);
    for(auto p:primitives){ok(api->getPrimitiveDuration(p,&ms));assert(ms>=0&&ms<200);}
    ok(api->getCompositionDelayMax(&v));assert(v==1000);ok(api->getCompositionSizeMax(&v));assert(v==256);
    av::CompositeEffect e;e.delayMs=5;e.scale=.5;e.primitive=av::CompositePrimitive::CLICK;
    ok(api->compose({e,e},cb));until([&]{return cb->calls==10;});
    invalid(api->compose({},cb));e.scale=-1;invalid(api->compose({e},cb));
    std::vector<av::Effect> alwaysOn;
    unsupported(api->setExternalControl(true));unsupported(api->getSupportedAlwaysOnEffects(&alwaysOn));
    unsupported(api->alwaysOnEnable(0,av::Effect::CLICK,av::EffectStrength::LIGHT));unsupported(api->alwaysOnDisable(0));
    unsupported(api->getQFactor(&hz));unsupported(api->getFrequencyResolution(&hz));unsupported(api->getFrequencyMinimum(&hz));
    std::vector<float> map;unsupported(api->getBandwidthAmplitudeMap(&map));
    unsupported(api->getPwlePrimitiveDurationMax(&v));unsupported(api->getPwleCompositionSizeMax(&v));
    std::vector<av::Braking> braking;unsupported(api->getSupportedBraking(&braking));unsupported(api->composePwle({},cb));
    // DSU SystemUI VivoNumPadKey requests private effect 53. Verify the
    // private IDs survive real Binder enum marshalling with bounded duration.
    for(int id:{53,10053}){
        ok(api->perform(static_cast<av::Effect>(id),static_cast<av::EffectStrength>(35),cb,&ms));
        assert(ms==(id==53?11:29));ok(api->off());
    }
    until([&]{return cb->calls==12;});
    for(int id:{134,10134}){
        for(int strength:{11,35,59}){
            ok(api->perform(static_cast<av::Effect>(id),static_cast<av::EffectStrength>(strength),cb,&ms));
            assert(ms==(id==134?14:29));ok(api->off());
        }
    }
    until([&]{return cb->calls==18;});
    // Previously rejected by the demo's 500 ms ceiling. Cancel rather than
    // waiting for the whole waveform; verify completion exactly once.
    ok(api->on(1800,cb));ok(api->off());until([&]{return cb->calls==19;});
    av::CompositeEffect longPart;longPart.delayMs=600;longPart.scale=.3f;longPart.primitive=av::CompositePrimitive::CLICK;
    ok(api->compose({longPart,longPart},cb));ok(api->off());until([&]{return cb->calls==20;});
    int expected=20;
    // Real private Binder enum values for captured upslide, launcher and IME.
    for(int id:{113,138,139,141,142,143,144,145,146,147,148,149,150,10113,10138,10139}){
        ok(api->perform(static_cast<av::Effect>(id),static_cast<av::EffectStrength>(59),cb,&ms));
        assert(ms>0&&ms<150);ok(api->off());expected++;until([&]{return cb->calls==expected;});
    }
    for(auto effect:effects){
        // Exercise every advertised scene through real generated Binder code.
        // DryBackend never opens the physical vibrator; cancel long waveforms.
        ok(api->perform(effect,static_cast<av::EffectStrength>(35),cb,&ms));assert(ms>0&&ms<=WaveModel::maxMs);
        ok(api->off());expected++;until([&]{return cb->calls==expected;});
    }
    ok(api->off());std::cout<<"PASS 24 standard transactions; supported IDs="<<effects.size()<<" callbacks="<<cb->calls<<"; slider styles and strengths; long on/compose/previews interrupted exactly once\n";
}
