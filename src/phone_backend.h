// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "playback_queue.h"
#include "rtp_backend.h"
namespace mio {
// Isolated diagnostic backend: <=500ms and <=20% PCM. No service registration.
class PhoneBackend final:public PlaybackBackend {
public:
    explicit PhoneBackend(bool allowRecordedProfile=false);
    static float preflight(bool allowRecordedProfile,bool& liveCalibration);
    ~PhoneBackend() override;
    int play(const Wave&,PlaybackState&) override;
    bool amplitudeControl() const override{return input_>=0;}
    float measuredHz() const{return hz_;}
    bool liveCalibration() const{return liveCalibration_;}
    int gainWrites() const{return gainWrites_;}
private:
    int fd_=-1,input_=-1;RtpSlot *slots_=nullptr;float hz_=0;
    PlaybackState *state_=nullptr;float applied_=-1;int gainError_=0;
    std::atomic<int> gainWrites_{0};
    bool liveCalibration_=true;
    int updateGain();
    static int command(void*,unsigned,uintptr_t);
    static int64_t now(void*);
    static void sleep(void*,unsigned);
    static int cancelled(void*);
};
}
