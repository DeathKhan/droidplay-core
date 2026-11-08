// Android stand-in for <mach/thread_policy.h>: Mach's time-constraint (real-time) policy has no counterpart an app may set on Android
// (SCHED_FIFO needs a privilege apps lack). What the declaration tells the system, the frame's period, goes to an Android Performance Hint
// session instead (plat_android.c dm_android_rt_declare, API 33), as its target unless the frame step's hold sets one (gfx_vk.c hint_frame):
// setting it succeeds while there is a session (or one closed for heat), and fails as before where there is none (an older Android, a power HAL
// without hint sessions, DM_VK_ADPF=0), so the callers keep the thread's ordinary priority
#pragma once
#include "dm_android.h"
typedef int *thread_policy_t;
typedef int thread_policy_flavor_t;
#define THREAD_TIME_CONSTRAINT_POLICY 2
#define THREAD_TIME_CONSTRAINT_POLICY_COUNT 4
typedef struct { uint32_t period, computation, constraint; boolean_t preemptible; } thread_time_constraint_policy_data_t;
kern_return_t dm_android_rt_declare(mach_port_t t, uint64_t period_ns, uint64_t comp_ns);   // plat_android.c
int dm_android_rt_declared(mach_port_t t, uint64_t *period_ns, uint64_t *comp_ns);
// mach time units are nanoseconds here (mach_timebase_info is 1 / 1)
static inline kern_return_t thread_policy_set(mach_port_t t, thread_policy_flavor_t f, thread_policy_t p, mach_msg_type_number_t n) {
    if (f != THREAD_TIME_CONSTRAINT_POLICY || !p || n < THREAD_TIME_CONSTRAINT_POLICY_COUNT) return KERN_FAILURE;
    const thread_time_constraint_policy_data_t *c = (const thread_time_constraint_policy_data_t *)(const void *)p;
    return dm_android_rt_declare(t, c->period, c->computation);
}
static inline kern_return_t thread_policy_get(mach_port_t t, thread_policy_flavor_t f, thread_policy_t p, mach_msg_type_number_t *n, boolean_t *def) {
    uint64_t per, comp;
    if (f != THREAD_TIME_CONSTRAINT_POLICY || !p || !dm_android_rt_declared(t, &per, &comp)) return KERN_FAILURE;
    thread_time_constraint_policy_data_t *c = (thread_time_constraint_policy_data_t *)(void *)p;
    c->period = c->constraint = (uint32_t)per; c->computation = (uint32_t)comp; c->preemptible = 1;
    if (n) *n = THREAD_TIME_CONSTRAINT_POLICY_COUNT; if (def) *def = 0;
    return KERN_SUCCESS;
}
