// Darwin-only interfaces the shared sources call that have no Android counterpart yet; each says what is missing.
#include "dm.h"
// thread_selfcounts (libSystem private): the calling thread's {instructions, cycles}. Android would need perf_event_open, which apps may not have: zeros
int thread_selfcounts(int type, void *buf, size_t nbytes) { (void)type; memset(buf, 0, nbytes); return -1; }

// Not built for Android yet (CMakeLists.txt). The cheat engine (hle_cheats.c) needs blocks and dispatch queues: no cheats are applied
void dm_cheats_init(void) {}
void dm_cheats_present(void) {}
void dm_cheats_exit(void) {}
// AV1 movies (hle_movie_av1.c: VideoToolbox, or dav1d, which this build does not compile): the decoder does not open, so a game with AV1 movies shows none
#include "hle_movie_av1.h"
mav1 *mav1_new(void) { static int once; if (!once++) LOG("[RED] movie: AV1 is not decoded on Android yet: the movie stays black"); return NULL; }
void mav1_free(mav1 *a) { (void)a; }
void mav1_reset(mav1 *a) { (void)a; }
int mav1_decode(mav1 *a, const uint8_t *data, size_t n, mav1_picture *out) { (void)a; (void)data; (void)n; (void)out; return -1; }
const char *mav1_backend(const mav1 *a) { (void)a; return "none"; }
// Frame capture (nvn_capture.inc, included by nvn_mtl.m): DM_NVN_CAPTURE is a Metal replay tool
#include "nvn_obj.h"
void nvn_cap_submit(int n, const uint64_t *handles) { (void)n; (void)handles; }
void nvn_cap_present(const tex_t *t, const int crop[4]) { (void)t; (void)crop; }
void nvn_cap_queue(uint64_t serial, int waits) { (void)serial; (void)waits; }
