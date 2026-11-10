// Darwin-only interfaces the shared sources call that have no Android counterpart yet; each says what is missing.
#include "dm.h"
// thread_selfcounts (libSystem private): the calling thread's {instructions, cycles}. Android would need perf_event_open, which apps may not have: zeros
int thread_selfcounts(int type, void *buf, size_t nbytes) { (void)type; memset(buf, 0, nbytes); return -1; }

// Not built for Android yet (CMakeLists.txt). The cheat engine (hle_cheats.c) needs blocks and dispatch queues: no cheats are applied
void dm_cheats_init(void) {}
void dm_cheats_present(void) {}
void dm_cheats_exit(void) {}
// Frame capture (nvn_capture.inc, included by nvn_mtl.m): DM_NVN_CAPTURE is a Metal replay tool
#include "nvn_obj.h"
void nvn_cap_submit(int n, const uint64_t *handles) { (void)n; (void)handles; }
void nvn_cap_present(const tex_t *t, const int crop[4]) { (void)t; (void)crop; }
void nvn_cap_queue(uint64_t serial, int waits) { (void)serial; (void)waits; }
