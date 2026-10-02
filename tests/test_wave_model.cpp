// SPDX-License-Identifier: Apache-2.0
#include "wave_model.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
using namespace mio;
int main(int argc,char **argv){
    assert(argc==2);WaveModel m(argv[1]);
    for(int id:{0,1,2,3,4,5,21,53,113,134,138,139}){
        auto crisp=m.effect(id),soft=m.effect(id+10000);
        assert(crisp!=soft);assert(WaveModel::duration(crisp)<150&&WaveModel::duration(soft)<150);
        std::cout<<"effect="<<id<<" crisp_ms="<<WaveModel::duration(crisp)<<" soft_ms="<<WaveModel::duration(soft)<<'\n';
    }
    assert(WaveModel::duration(m.effect(53))==11);
    assert(WaveModel::duration(m.effect(10053))==29);
    // User-approved keyboard override stays intact; no invented soft IME IDs.
    for(auto range:std::vector<std::pair<int,int>>{{141,146},{147,148},{149,150}}){
        long previous=0;
        for(int id=range.first;id<=range.second;id++){
            auto w=m.effect(id);long energy=0;for(int8_t x:w)energy+=int(x)*int(x);
            assert(energy>previous);previous=energy;
        }
    }
    long keyboardEnergy=0;
    for(int id=141;id<=150;id++){
        auto key=m.effect(id);assert(WaveModel::duration(key)==17);
        long energy=0;for(auto sample:key)energy+=int(sample)*sample;
        assert(energy>keyboardEnergy);keyboardEnergy=energy;
    }
    assert(m.effect(150)==m.primitive(1)); // full-scale official OnePlus effect2
    assert(m.effect(65)==m.effect(10065));
    assert(m.effect(625)!=m.effect(626));
    assert(WaveModel::duration(m.effect(625))==100);
    assert(WaveModel::duration(m.effect(337))==20000);
    assert(WaveModel::duration(m.effect(449))==1102);
    assert(WaveModel::duration(m.effect(519))==44037);
    assert(WaveModel::duration(m.effect(691))==201);
    assert(WaveModel::duration(m.effect(3066))==3000);
    assert(WaveModel::duration(m.effect(3103))==2215);
    assert(WaveModel::duration(m.effect(26007))==554);
    assert(WaveModel::duration(m.effect(626))==19);
    long carEnergy=0;
    for(int id=631;id<=637;id++){
        auto car=m.effect(id);assert(WaveModel::duration(car)==21);
        long energy=0;for(auto x:car)energy+=int(x)*int(x);
        assert(energy>carEnergy);carEnergy=energy;
    }
    auto ids=m.supportedEffects();assert(ids.size()==765);
    for(int id:ids){auto w=m.effect(id);assert(!w.empty());assert(WaveModel::duration(w)<=WaveModel::maxMs);}
    for(int id:{10146,123456}){
        bool missing=false;try{m.effect(id);}catch(const std::out_of_range&){missing=true;}assert(missing);
    }
    std::set<Wave> primitives;
    for(int id=0;id<=8;id++)primitives.insert(m.primitive(id));
    assert(primitives.size()==9);
    float prev=0;
    assert(std::abs(WaveModel::strength(11)-.2f)<1e-6f);
    assert(std::abs(WaveModel::strength(35)-.6f)<1e-6f);
    assert(std::abs(WaveModel::strength(59)-1.f)<1e-6f);
    for(int strength=11;strength<=59;strength++){auto value=WaveModel::strength(strength);assert(value>prev&&value<=1);prev=value;}
    for(int id:{134,10134}){
        auto raw=m.effect(id);long previousEnergy=0;
        for(int strength=11;strength<=59;strength++){
            Wave scaled;WaveModel::append(scaled,raw,WaveModel::strength(strength));
            long energy=0;for(int8_t sample:scaled)energy+=static_cast<int>(sample)*sample;
            assert(energy>=previousEnergy);previousEnergy=energy;
            if(strength==59)assert(scaled==raw);
        }
        Wave minimum,oldMaximum;
        WaveModel::append(minimum,raw,WaveModel::strength(11));
        WaveModel::append(oldMaximum,raw,.2f);
        assert(minimum==oldMaximum);
    }
    for(int strength:{-1,3,10,60,127}){bool threw=false;try{WaveModel::strength(strength);}catch(...){threw=true;}assert(threw);}
    bool threw=false;try{m.effect(99999);}catch(...){threw=true;}assert(threw);
    auto w=WaveModel::ramp({{0,132,1,132,20},{1,132,1,132,60},{1,132,0,132,20}});
    assert(w.size()==2400);assert(w.front()==0&&w.back()==0);
    threw=false;try{WaveModel::ramp({{0,132,std::numeric_limits<float>::quiet_NaN(),132,10}});}catch(...){threw=true;}assert(threw);
    std::cout<<"PASS: 765 scenes; approved IME preserved; recovered ring timelines; two-hit injury; all original IDs covered, explicit compatibility effects; styles, 9 primitives, 49-step strength.\n";
}
