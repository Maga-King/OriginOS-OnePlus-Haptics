// SPDX-License-Identifier: Apache-2.0
// Read-only hardware queries: no gain, playback, calibration, STOP or mmap.
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
int main(void){
 int fd=open("/dev/awinic_haptic",O_RDONLY|O_CLOEXEC);
 if(fd<0){perror("open");return 1;}
 uint32_t hw=0,f0=0;errno=0;
 int rh=ioctl(fd,0x5203,&hw),eh=errno;errno=0;
 int rf=ioctl(fd,0x5211,&f0),ef=errno;
 printf("{\"hardware\":%u,\"hardware_status\":%d,\"hardware_errno\":%d,\"f0_tenths_hz\":%u,\"f0_status\":%d,\"f0_errno\":%d}\n",hw,rh,eh,f0,rf,ef);
 close(fd);return rh||rf;
}
