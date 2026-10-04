// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "playback_queue.h"
#include "rtp_backend.h"
#include <array>
#include <vector>
namespace nyako {
// One physical stream; normal sessions and rejected short feedback own separate
// cursors. Frame retirement, rather than submission, releases normal callbacks.
class PcmMixer {
public:
    static constexpr size_t frameSamples=240; // 10 ms at 24 kHz
    using DirectPlay=std::function<int(const Wave&,PlaybackState&)>;
    explicit PcmMixer(RtpTransport,DirectPlay={});
    ~PcmMixer();
    int play(const Wave&,PlaybackState&);
    int overlay(Wave,uint64_t owner);
    void cancelOverlay(uint64_t owner);
private:
    struct Job {Wave wave;PlaybackState feedback;PlaybackState* state=nullptr;size_t pos=0;
        uint64_t owner=0,end=0;int result=0;bool done=false;};
    void run();
    bool render(int8_t*,uint64_t);
    void retire(uint64_t);
    void fail(int);
    RtpTransport t_;
    DirectPlay direct_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<Job> base_,overlay_;
    std::vector<std::shared_ptr<Job>> draining_;
    bool closing_=false;
    std::thread thread_;
};
}
