// seL4-like API stub for host compilation. Real seL4 builds use the actual
// libsel4 headers; this header only makes the call sequence expressible in C.
#ifndef SEL4_STUBS_H
#define SEL4_STUBS_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t seL4_CapData_t;
typedef uintptr_t seL4_Word_t;

typedef struct { seL4_Word_t words[2]; } seL4_MessageInfo_t;
typedef struct { seL4_Word_t badge; } seL4_Reply_t;
typedef struct { seL4_Word_t label; } seL4_Fault_t;

typedef enum {
    seL4_NoError = 0,
    seL4_InvalidArgument = 1,
    seL4_InvalidCapability = 2,
    seL4_FailedLookup = 3,
    seL4_TruncatedMessage = 4
} seL4_Error;

static inline seL4_Error seL4_IRQHandler_SetNotification(seL4_CapData_t cap,
                                                         seL4_CapData_t ntfn)
{
    (void)cap; (void)ntfn;
    return seL4_NoError;
}

static inline seL4_Word_t seL4_Wait(seL4_CapData_t ntfn, seL4_Reply_t *reply)
{
    (void)ntfn; (void)reply;
    return 1u;
}

static inline seL4_Word_t seL4_Call(seL4_CapData_t ep,
                                    seL4_MessageInfo_t mi,
                                    seL4_Reply_t *reply)
{
    (void)ep; (void)mi; (void)reply;
    return 1u;
}

extern volatile uint32_t g_device_reg; /* mapped via granted frame capability */

#endif
