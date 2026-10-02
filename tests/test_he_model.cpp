// SPDX-License-Identifier: Apache-2.0
#include "he_model.h"
#include <cassert>
#include <algorithm>
#include <iostream>
using namespace nyako;
int main(int argc,char** argv){
    assert(argc==2);HeModel model(argv[1]);
    // Exact packet captured while long-pressing the navigation bar to invoke Copilot.
    std::vector<int32_t> actual{2,2,23305,7,65537,0,0,1,4096,18,1,250,100,50,300,4,0,0,-40,20,10,-40,290,20,-40,300,0,-40};
    auto wave=model.render(actual,1,0,255,0);assert(wave.size()==550*24);
    assert(std::all_of(wave.begin(),wave.begin()+250*24,[](int8_t x){return x==0;}));
    assert(std::any_of(wave.begin()+250*24,wave.end(),[](int8_t x){return x!=0;}));
    auto loops=model.render(actual,3,40,255,0);assert(loops.size()==1730*24);
    assert(std::equal(wave.begin(),wave.end(),loops.begin()+590*24));
    auto silent=model.render(actual,1,0,0,0);assert(std::all_of(silent.begin(),silent.end(),[](int8_t x){return x==0;}));
    assert(model.render(actual,1,0,128,0)!=wave);assert(model.render(actual,1,0,255,20)!=wave);
    std::vector<int32_t> transient{1,4097,0,100,50,0,0,0,0,0,0,0,0,0,0,0,0,0};
    auto shortWave=model.render(transient,1,0,255,0);assert(WaveModel::duration(shortWave)==19);
    for(int choice=0;choice<5;choice++){
        auto bad=actual;if(choice==0)bad.pop_back();if(choice==1)bad[14]=-1;if(choice==2)bad[9]=1000;
        if(choice==3)bad[4]=65538;if(choice==4)bad[16]=300;
        bool failed=false;try{model.render(bad,1,0,255,0);}catch(...){failed=true;}assert(failed);
    }
    bool failed=false;try{model.render(actual,65535,0,255,0);}catch(...){failed=true;}assert(failed);
    std::cout<<"PASS captured Copilot HE2: 250ms silence + 300ms envelope; 3 loops with40ms gaps; HE1 transient; amplitude/frequency; malformed/incomplete bounds\n";
}
