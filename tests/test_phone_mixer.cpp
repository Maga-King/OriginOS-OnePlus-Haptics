// SPDX-License-Identifier: GPL-2.0-only
// Explicit diagnostic executable; never runs at boot or during installation.
#include "phone_backend.h"
#include <chrono>
#include <cstdio>
#include <future>
using namespace nyako;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    auto backend=std::make_shared<PhoneBackend>(true,true);
    WaveModel model(argv[1]);
    {PlaybackState key;int r=backend->play(model.effect(148),key);
        std::printf("physical key-direct result=%d\n",r);if(r)return 5;}
    for(int i=0;i<3;i++){
        PlaybackState state;
        Wave normal=WaveModel::ramp({{.15f,132,.15f,132,i==0?3000:300}},.15f);
        auto start=std::chrono::steady_clock::now();
        auto result=std::async(std::launch::async,[&]{return backend->play(normal,state);});
        int begun=state.waitStarted();if(begun){std::fprintf(stderr,"start failed=%d\n",begun);return 3;}
        std::this_thread::sleep_for(std::chrono::milliseconds(45));
        int overlay=backend->overlay(model.effect(148),100+i);
        if(i==1){std::this_thread::sleep_for(std::chrono::milliseconds(2));backend->cancelOverlay(101);}
        if(i==2){std::this_thread::sleep_for(std::chrono::milliseconds(3));state.cancel=true;}
        int r=result.get();
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("physical case=%d overlay=%d result=%d elapsed_ms=%lld\n",i,overlay,r,static_cast<long long>(ms));
        if(overlay||r!=(i==2?-ECANCELED:0))return 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
