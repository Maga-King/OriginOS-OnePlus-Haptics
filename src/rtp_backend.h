// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>

enum { RTP_GAIN=0x5205, RTP_DIRECT=0x5209, RTP_STREAM=0x520a, RTP_STOP=0x5212 };
enum { RTP_VALID=0x55, RTP_FINISHED=0xaa, RTP_INVALID=0xff };
#pragma pack(push,4)
typedef struct {
    uint8_t status,bit;
    int16_t length;
    uint32_t reserved;
    uint64_t kernel_next,user_next;
    int8_t data[1000];
} RtpSlot;
#pragma pack(pop)
#ifdef __cplusplus
static_assert(sizeof(RtpSlot)==1024,"64-bit Qualcomm RichTap ABI");
extern "C" {
#else
_Static_assert(sizeof(RtpSlot)==1024,"64-bit Qualcomm RichTap ABI");
#endif

// Only one owner may call play/stop for a device. The owner serializes jobs.
// Cancel is polled at <=1ms intervals; kernel ioctl time is outside that bound.
typedef struct {
    void *ctx;
    int (*command)(void *,unsigned,uintptr_t);
    int64_t (*now_us)(void *);
    void (*sleep_us)(void *,unsigned);
    int (*cancelled)(void *);
    RtpSlot *slots; // 16KB driver mapping, first 4 x 1024 bytes used.
} RtpTransport;
// Returns 0 only after completion, otherwise negative errno; always stops on error.
// Intensity uses PCM scaling. Global driver gain stays at the stock full-scale value.
int rtp_play(RtpTransport *,const int8_t *,size_t,float);
#ifdef __cplusplus
}
#endif
