// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "wave_model.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
namespace mio {
struct PlaybackState {
    std::atomic<bool> cancel{false};std::atomic<float> amplitude{1};
    // Backend acknowledges driver start, so Binder duration does not include an
    // unbounded scheduling delay before the motor has even begun playing.
    void started(int result=0);
    int waitStarted();
private:
    std::mutex readyMutex_;std::condition_variable readyCv_;
    bool ready_=false;int result_=0;
};
class PlaybackBackend {
public:
    virtual ~PlaybackBackend()=default;
    // A single worker owns the physical device. Return only after output stopped.
    virtual int play(const Wave&,PlaybackState&)=0;
    virtual bool amplitudeControl() const=0;
};
class PlaybackQueue {
public:
    using Complete=std::function<void(int)>;
    using Ticket=uint64_t;
    explicit PlaybackQueue(std::shared_ptr<PlaybackBackend>);
    ~PlaybackQueue();
    PlaybackQueue(const PlaybackQueue&)=delete;
    PlaybackQueue& operator=(const PlaybackQueue&)=delete;
    int submit(Wave,Complete={},bool adjustable=false,bool waitForStart=false,Ticket* ticket=nullptr);
    void off();
    void cancel(Ticket);
    void dispatch(Complete,int result=0);
    void amplitude(float);
    bool amplitudeControl() const{return backend_->amplitudeControl();}
    int lastResult() const{return lastResult_.load();}
private:
    struct Job {Wave wave;Complete complete;PlaybackState state;bool adjustable=false;Ticket ticket=0;};
    void run();
    void notify(const std::shared_ptr<Job>&,int);
    void runCallbacks();
    std::shared_ptr<PlaybackBackend> backend_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<Job> current_,pending_;
    bool closing_=false;
    Ticket sequence_=0;
    std::thread thread_;
    std::atomic<int> lastResult_{0};
    std::mutex callbackMutex_;
    std::condition_variable callbackCv_;
    std::deque<std::pair<std::shared_ptr<Job>,int>> completions_;
    bool callbacksClosing_=false;
    std::thread callbackThread_;
};
}
