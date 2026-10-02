// SPDX-License-Identifier: Apache-2.0
#include "he_model.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace mio {
namespace {
struct Point {int t,a,f;};
struct Event {int type,start,intensity,frequency,duration;std::vector<Point> points;};
void require(bool valid){if(!valid)throw std::invalid_argument("invalid HE pattern");}
int value(const std::vector<int32_t>& p,size_t at){require(at<p.size());return p[at];}
Event event(const std::vector<int32_t>& p,size_t at,size_t count,bool v2,int base){
    require(at<=p.size()&&count<=p.size()-at);int kind=value(p,at);require(kind==4096||kind==4097);
    size_t begin=at+(v2?3:1);int relative=value(p,begin),duration=value(p,begin+3);
    require(relative>=0&&relative<=WaveModel::maxMs&&base>=0&&base<=WaveModel::maxMs-relative);
    Event e{kind,base+relative,value(p,begin+1),value(p,begin+2),duration,{}};
    require(e.intensity>=0&&e.intensity<=100&&e.frequency>=0&&e.frequency<=100&&duration>=0&&duration<=WaveModel::maxMs-e.start);
    if(v2)require(value(p,at+2)==0||value(p,at+2)==1); // original virtual vibrator IDs -> one target motor
    if(kind==4096){
        int points=v2?value(p,at+7):4;size_t first=at+(v2?8:5);
        require(duration>0&&points>=2&&points<=16&&first+size_t(points)*3<=at+count);
        for(int i=0;i<points;i++){
            size_t n=first+i*3;Point point{value(p,n),value(p,n+1),value(p,n+2)};
            require(point.t>=0&&point.t<=duration&&point.a>=0&&point.a<=100&&point.f>=-100&&point.f<=100);
            require(e.points.empty()||point.t>e.points.back().t);e.points.push_back(point);
        }
        require(e.points.front().t==0&&e.points.back().t==duration);
    }
    return e;
}
}
Wave HeModel::render(const std::vector<int32_t>& p,int flags,int interval,int amplitude,int frequency) const {
    require(!p.empty()&&p.size()<=65536&&interval>=0&&interval<=WaveModel::maxMs&&amplitude>=0&&amplitude<=255&&frequency>=-100&&frequency<=100);
    int loops=flags&0xffff;require(loops>0);std::vector<Event> events;
    if(p[0]==1){
        require((p.size()-1)%17==0);
        for(size_t at=1;at<p.size();at+=17)events.push_back(event(p,at,17,false,0));
    }else if(p[0]==2){
        require(p.size()>=8&&p[1]==2);unsigned total=unsigned(p[4])&0xffff,wrapped=unsigned(p[4])>>16;
        // Single-packet HE2 is implemented here. Incomplete multi-packet streams
        // must be assembled by the caller, never silently played as a full job.
        require(total>0&&total==wrapped);size_t at=5;std::set<int> patterns;
        for(unsigned i=0;i<wrapped;i++){
            int index=value(p,at),base=value(p,at+1),count=value(p,at+2);at+=3;
            require(index>=0&&unsigned(index)<total&&patterns.insert(index).second&&count>0&&count<=4096);
            for(int j=0;j<count;j++){
                int n=value(p,at+1);require(n>=5&&n<=54);size_t length=size_t(n)+2;
                events.push_back(event(p,at,length,true,base));at+=length;
            }
        }
        require(at==p.size());
    }else throw std::out_of_range("unsupported HE version");
    require(!events.empty());std::vector<double> mixed;
    for(const auto& e:events){
        Wave w;
        if(e.type==4097){
            w=waves_.primitive(5); // official OnePlus strong short transient
            if(e.duration>WaveModel::duration(w))w.resize(size_t(e.duration)*24,0);
        }else{
            w.resize(size_t(e.duration)*24);size_t segment=0;double phase=0;
            for(size_t i=0;i<w.size();i++){
                double t=i/24.0;while(segment+2<e.points.size()&&t>e.points[segment+1].t)segment++;
                auto a=e.points[segment],b=e.points[segment+1];double frac=(t-a.t)/(b.t-a.t);
                double level=(a.a+(b.a-a.a)*frac)/100.0;
                double delta=e.frequency-50+frequency+a.f+(b.f-a.f)*frac;
                double hz=std::clamp(132.0*(1.0+delta/200.0),60.0,260.0);
                phase+=2*3.14159265358979323846*hz/24000;
                w[i]=int8_t(std::lround(127*level*std::sin(phase)));
            }
            w.front()=0;w.back()=0;
        }
        size_t offset=size_t(e.start)*24;require(offset+w.size()<=WaveModel::maxMs*24u);
        mixed.resize(std::max(mixed.size(),offset+w.size()),0);
        double gain=e.intensity/100.0*amplitude/255.0;
        for(size_t i=0;i<w.size();i++)mixed[offset+i]+=w[i]*gain;
    }
    require(!mixed.empty());Wave cycle;cycle.reserve(mixed.size());
    for(double sample:mixed)cycle.push_back(int8_t(std::lround(std::clamp(sample,-127.0,127.0))));
    uint64_t total=uint64_t(cycle.size())*loops+uint64_t(interval)*24*(loops-1);
    require(total<=WaveModel::maxMs*24u);Wave result;result.reserve(total);
    for(int i=0;i<loops;i++){if(i)result.resize(result.size()+size_t(interval)*24,0);WaveModel::append(result,cycle);}
    return result;
}
}
