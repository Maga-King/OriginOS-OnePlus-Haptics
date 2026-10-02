// SPDX-License-Identifier: Apache-2.0
#include "wave_model.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <dirent.h>
#include <cstdio>
#include <stdexcept>
namespace mio {
static constexpr double pi=3.14159265358979323846;
struct DonorScene {int scene;int effect;float gain;};
#include "donor_scenes.inc"
#include "supplemental_scenes.inc"
Wave WaveModel::stock(const char *style,int id) const {
    std::ifstream f(root_+"/"+style+"/effect_"+std::to_string(id)+".bin",std::ios::binary);
    if(!f)throw std::out_of_range("stock wave missing");
    f.seekg(0,std::ios::end);auto n=f.tellg();
    if(n<=0||n>maxMs*24)throw std::out_of_range("invalid stock wave size");
    f.seekg(0);Wave w(static_cast<size_t>(n));f.read(reinterpret_cast<char*>(w.data()),n);
    if(!f)throw std::runtime_error("stock wave read failed");return w;
}
void WaveModel::append(Wave& dst,const Wave& src,float gain){
    if(!std::isfinite(gain)||gain<0||gain>1||dst.size()+src.size()>maxMs*24)throw std::invalid_argument("invalid composition");
    for(int8_t x:src)dst.push_back(static_cast<int8_t>(std::lround(x*gain)));
}
float WaveModel::strength(int value){
    if(value>=0&&value<=2)return value==0?.5f:value==1?.75f:1.f;
    // 49 vivo steps, now 20..100% of the waveform rather than a second
    // demo-stage attenuation. Full scale never exceeds the source PCM peak.
    if(value>=11&&value<=59)return .2f+.8f*(value-11)/48.f;
    throw std::invalid_argument("invalid effect strength");
}
Wave WaveModel::effect(int id) const {
    // User-requested OnePlus keyboard feel: every IME level uses the factory
    // effect_2 short key waveform instead of the donor RAM-family changes.
    if(id>=141&&id<=150){
        Wave w;append(w,stock("def",2),.25f+.75f*(id-141)/9.f);return w;
    }
    // HEAVY_CLICK precedes the assistant HE animation. Use the factory strong
    // transient, not the envelope-matched but weak donor RAM5 substitute.
    if(id==5)return stock("def",109);
    if(id==10005)return stock("soft",109);
    // Exact scene -> waveform relationship from the supplied PD2620 DTB.
    // Scene IDs, effect-file IDs and style offsets are separate namespaces.
    auto entry=std::lower_bound(std::begin(donorScenes),std::end(donorScenes),id,
        [](const DonorScene& row,int value){return row.scene<value;});
    if(entry!=std::end(donorScenes)&&entry->scene==id){
        Wave w;append(w,stock("donor",entry->effect),entry->gain);return w;
    }
    auto extra=std::lower_bound(std::begin(supplementalScenes),std::end(supplementalScenes),id,
        [](const DonorScene& row,int value){return row.scene<value;});
    if(extra!=std::end(supplementalScenes)&&extra->scene==id){
        Wave w;append(w,stock("donor",extra->effect),extra->gain);return w;
    }
    // Captured DSU callers absent from this donor's scene table. Explicit
    // 0916T substitutes; not claimed to reproduce a missing original effect.
    switch(id){
    case 138:return stock("def",0);
    case 139:return stock("def",109);
    case 10138:return stock("soft",0);
    case 10139:return stock("soft",109);
    default:throw std::out_of_range("unmapped scene");
    }
}
std::vector<int> WaveModel::supportedEffects() const {
    std::vector<int> ids{138,139,10138,10139};
    for(const auto& row:donorScenes)ids.push_back(row.scene);
    for(const auto& row:supplementalScenes)ids.push_back(row.scene);
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());return ids;
}
Wave WaveModel::primitive(int id) const {
    // Use distinct factory profiles; no aliasing every primitive to CLICK.
    switch(id){
    case 0:return {};
    case 1:return stock("def",2);
    case 2:return ramp({{0,132,.65f,190,30},{.65f,190,0,260,30}},.35f);
    case 3:return ramp({{0,132,.65f,190,60},{.65f,190,0,260,60}},.35f);
    case 4:return ramp({{0,260,.65f,190,30},{.65f,190,0,132,30}},.35f);
    case 5:return stock("def",109);
    case 6:return stock("def",11);
    case 7:return stock("def",0);
    case 8:return stock("def",9);
    default:throw std::out_of_range("invalid primitive");
    }
}
Wave WaveModel::ramp(const std::vector<Ramp>& ramps,float maxAmplitude){
    Wave w;double phase=0;
    for(auto r:ramps){
        if(r.ms<0||r.ms>maxMs||!std::isfinite(r.a0)||!std::isfinite(r.a1)||!std::isfinite(r.hz0)||!std::isfinite(r.hz1)||r.a0<0||r.a0>1||r.a1<0||r.a1>1||r.hz0<50||r.hz0>350||r.hz1<50||r.hz1>350||w.size()+static_cast<size_t>(r.ms)*24>maxMs*24)throw std::invalid_argument("invalid PWLE");
        size_t count=static_cast<size_t>(r.ms)*24;
        for(size_t i=0;i<count;i++){
            double t=count>1?static_cast<double>(i)/(count-1):0;
            double a=r.a0+(r.a1-r.a0)*t,hz=r.hz0+(r.hz1-r.hz0)*t;
            w.push_back(static_cast<int8_t>(std::lround(127*maxAmplitude*a*std::sin(phase))));
            phase=std::remainder(phase+2*pi*hz/rate,2*pi);
        }
    }
    return w;
}
}
