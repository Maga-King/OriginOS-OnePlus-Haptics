// SPDX-License-Identifier: GPL-2.0-only
#include "phone_backend.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <linux/input.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
namespace mio {
static float readProfile(int fd,bool allowRecordedProfile,bool& liveCalibration){
    uint32_t hz=0,hw=0;
    int rf=ioctl(fd,0x5211,&hz),rh=ioctl(fd,0x5203,&hw);
    auto readInt=[](const char* path){int n=-1;std::ifstream f(path);f>>n;return n;};
    // Missing runtime F0 uses the recorded stock measurement. No calibration write.
    const int motor=readInt("/sys/class/qcom-haptics/vibrator_type");
    const int period=readInt("/sys/class/qcom-haptics/t_lra_us");
    const int impedance=readInt("/sys/class/qcom-haptics/lra_impedance");
    std::fprintf(stderr,"MIO hardware hw=%u status=%d f0=%u status=%d motor=%d period=%d impedance=%d\n",hw,rh,hz,rf,motor,period,impedance);
    if((rf||hz==0)&&allowRecordedProfile){
        hz=1320;liveCalibration=false;
        std::fprintf(stderr,"MIO using recorded stock 132 Hz profile; live F0 unavailable; no calibration write\n");
    }
    return hz/10.f;
}
float PhoneBackend::preflight(bool allow,bool& live){
    int fd=open("/dev/awinic_haptic",O_RDONLY|O_CLOEXEC);
    if(fd<0)throw std::runtime_error("cannot open physical haptic device");
    try{float hz=readProfile(fd,allow,live);close(fd);return hz;}catch(...){close(fd);throw;}
}
PhoneBackend::PhoneBackend(bool allowRecordedProfile){
    fd_=open("/dev/awinic_haptic",O_RDWR|O_CLOEXEC);
    if(fd_<0)throw std::runtime_error("cannot open physical haptic device");
    try{hz_=readProfile(fd_,allowRecordedProfile,liveCalibration_);}catch(...){close(fd_);fd_=-1;throw;}
    auto p=mmap(nullptr,16384,PROT_READ|PROT_WRITE,MAP_SHARED,fd_,0);
    if(p==MAP_FAILED){close(fd_);fd_=-1;throw std::runtime_error("haptic mmap failed");}
    slots_=static_cast<RtpSlot*>(p);
    // Locate by kernel input name; event numbers change across boots/ROMs.
    for(int i=0;i<64;i++){
        char path[64],name[128]={};snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int f=open(path,O_RDWR|O_CLOEXEC);if(f<0)continue;
        if(ioctl(f,EVIOCGNAME(sizeof(name)),name)>=0&&!strcmp(name,"qcom-hv-haptics")){input_=f;break;}close(f);
    }
}
PhoneBackend::~PhoneBackend(){
    if(input_>=0)close(input_);if(slots_)munmap(slots_,16384);if(fd_>=0)close(fd_);
}
int PhoneBackend::updateGain(){
    float value=state_->amplitude.load();if(value==applied_)return 0;
    if(input_<0)return value==1?0:-ENOTSUP;
    input_event event{};event.type=EV_FF;event.code=FF_GAIN;
    // Same floor/range as the stock InputFFDevice::setAmplitude; no sysfs writes.
    event.value=0x1333+static_cast<int>(std::lround(0x6ccc*value));
    ssize_t n;do{n=write(input_,&event,sizeof(event));}while(n<0&&errno==EINTR);
    if(n!=sizeof(event))return n<0?-errno:-EIO;
    gainWrites_++;applied_=value;return 0;
}
int PhoneBackend::command(void* ctx,unsigned op,uintptr_t arg){
    auto& b=*static_cast<PhoneBackend*>(ctx);int r=ioctl(b.fd_,op,arg);
    if(r<0)return -errno;
    if(op==RTP_DIRECT||op==RTP_STREAM){b.applied_=-1;r=b.updateGain();b.state_->started(r);return r;}
    return 0;
}
int64_t PhoneBackend::now(void*){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return static_cast<int64_t>(t.tv_sec)*1000000+t.tv_nsec/1000;}
void PhoneBackend::sleep(void* ctx,unsigned us){
    auto& b=*static_cast<PhoneBackend*>(ctx);int r=b.updateGain();if(r)b.gainError_=r;
    timespec t{us/1000000,static_cast<long>(us%1000000)*1000};nanosleep(&t,nullptr);
}
int PhoneBackend::cancelled(void* ctx){auto& b=*static_cast<PhoneBackend*>(ctx);return b.state_->cancel||b.gainError_;}
int PhoneBackend::play(const Wave& wave,PlaybackState& state){
    if(wave.size()>static_cast<size_t>(WaveModel::maxMs)*24)return -E2BIG;
    state_=&state;gainError_=0;applied_=-1;
    RtpTransport t{this,command,now,sleep,cancelled,slots_};
#ifdef MIO_DEMO
    int64_t began=now(this);
#endif
    // perform()/compose() already applied the requested strength. Preserve the
    // resulting PCM amplitude; the old demo multiplier made maximum equal 20%.
    int r=rtp_play(&t,wave.data(),wave.size(),1.f);
#ifdef MIO_DEMO
    std::fprintf(stderr,"MIO output samples=%zu elapsed_us=%lld result=%d\n",wave.size(),static_cast<long long>(now(this)-began),r);
#endif
    // Restore stock gain after stopping, without modifying the caller's state.
    PlaybackState restore;state_=&restore;applied_=-1;int reset=updateGain();
    state_=nullptr;
    if(gainError_)return gainError_;return r?r:reset;
}
}
