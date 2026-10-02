// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace nyako {
using Wave=std::vector<int8_t>;
struct Ramp {float a0,hz0,a1,hz1;int ms;};
class WaveModel {
public:
    static constexpr int rate=24000;
    static constexpr int maxMs=120000;
    explicit WaveModel(std::string root):root_(std::move(root)){}
    Wave effect(int id) const;
    std::vector<int> supportedEffects() const;
    Wave primitive(int id) const;
    static float strength(int strength);
    static Wave ramp(const std::vector<Ramp>&,float maxAmplitude=.18f);
    static void append(Wave&,const Wave&,float gain=1);
    static int duration(const Wave& w){return static_cast<int>((w.size()+23)/24);}
private:
    Wave stock(const char *style,int id) const;
    std::string root_;
};
}
