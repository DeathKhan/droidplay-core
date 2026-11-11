// The image eviction rules (gfx_vk_evict.inc) as plain functions of what the replay knows about an image, so dm/tools/tests/test_img_evict.py checks them on the host
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// Clean: its content is still its storage's (a packaged texture, or a shared one filled from its storage when made), nothing on the GPU wrote it, the texture pool by
// ID never read it (a view dropped from the pool set reads 0 until the sync after a draw reads its ID again), and no lookup for age presents
static inline int ie_clean(int gpu_w, int pool_read, int pkg, int shared, int filled, uint64_t last_present, uint64_t now, uint64_t age) {
    return !gpu_w && !pool_read && (pkg || (shared && filled)) && now - last_present >= age;
}
// A shared image: its pool, its storage range there, the presents of its last lookup and of its last write by the GPU (0: none), and the caller's own pointer for it
typedef struct { uint64_t pool; int64_t lo, hi; uint64_t last_present, last_w; void *p; } ie_ent;
// a was overwritten by b: b was written after a's last lookup and covers at least half of a's storage (layouts are padded: an image another one only clips at its
// edge kept its content on the console), and a had no lookup for age presents
static inline int ie_overwritten(const ie_ent *a, const ie_ent *b, uint64_t now, uint64_t age) {
    int64_t lo = a->lo > b->lo ? a->lo : b->lo, hi = a->hi < b->hi ? a->hi : b->hi;
    return a->pool == b->pool && hi > lo && b->last_w > a->last_present && now - a->last_present >= age && 2 * (hi - lo) >= a->hi - a->lo;
}
static inline int ie_cmp(const void *x, const void *y) {
    const ie_ent *a = x, *b = y;
    return a->pool != b->pool ? (a->pool < b->pool ? -1 : 1) : a->lo != b->lo ? (a->lo < b->lo ? -1 : 1) : 0;
}
// Sorts e by pool and offset and marks each entry another one overwrote (mark[i] for e[i] after the sort). Every pair whose storage overlaps is looked at once:
// sorted by offset, the entries that overlap e[i] from after it are exactly those that start before it ends
static inline void ie_sweep(ie_ent *e, uint32_t n, uint64_t now, uint64_t age, uint8_t *mark) {
    qsort(e, n, sizeof *e, ie_cmp);
    for (uint32_t i = 0; i < n; i++) mark[i] = 0;
    for (uint32_t i = 0; i < n; i++) for (uint32_t j = i + 1; j < n && e[j].pool == e[i].pool && e[j].lo < e[i].hi; j++) {
        if (ie_overwritten(&e[i], &e[j], now, age)) mark[i] = 1;
        if (ie_overwritten(&e[j], &e[i], now, age)) mark[j] = 1;
    }
}
// Memory pressure (Linux PSI, /proc/pressure/memory). Android's lmkd ends the foreground game by how long tasks stall on memory ("thrashing", "swap is low"), not by
// how much RAM is free, and compressed swap keeps MemAvailable up while the game already stalls, so the ages above can be too slow. From IE_PSI_HOLD percent of the last
// 10 s stalled, the images whose content a lookup makes again stop growing; from IE_PSI_SHRINK an eighth of the image memory goes at each scan, the least recently looked up
// first; never under IE_FLOOR. The rule BananaNative's 483ce24a gives its texture cache (MIT), written anew for these ages
#define IE_PSI_HOLD 5.0
#define IE_PSI_SHRINK 15.0
#define IE_FLOOR (512ull << 20)
// "some avg10=" of /proc/pressure/memory's text, in percent; -1 when it isn't there
static inline double ie_psi_some10(const char *buf) { const char *a = buf ? strstr(buf, "some avg10=") : NULL; return a ? strtod(a + 11, NULL) : -1; }
static inline int ie_psi_level(double some10) { return some10 >= IE_PSI_SHRINK ? 2 : some10 >= IE_PSI_HOLD ? 1 : 0; }
// The image bytes to get down to at a level, given what is alive: none (UINT64_MAX) without pressure; while holding, the least alive at any scan since the hold began
// (*cap, kept between scans: images made since then go again); while shrinking, an eighth under what is alive (the hold continues from there); never under IE_FLOOR
static inline uint64_t ie_target(int level, uint64_t live, uint64_t *cap) {
    if (!level) { *cap = 0; return UINT64_MAX; }
    if (level >= 2) *cap = live - live / 8;
    else if (!*cap || live < *cap) *cap = live;
    if (*cap < IE_FLOOR) *cap = IE_FLOOR;
    return *cap;
}
// An image the pressure rule may take (its content made again by its next lookup, no lookup for a few presents): its last lookup, its bytes, the caller's pointer
typedef struct { uint64_t last_present, bytes; void *p; } ie_cand;
static inline int ie_cand_cmp(const void *x, const void *y) { const ie_cand *a = x, *b = y; return a->last_present != b->last_present ? (a->last_present < b->last_present ? -1 : 1) : 0; }
// Sorts c by last lookup and marks the least recently looked up until need bytes are chosen (mark[i] for c[i] after the sort); returns the bytes chosen
static inline uint64_t ie_pick(ie_cand *c, uint32_t n, uint64_t need, uint8_t *mark) {
    qsort(c, n, sizeof *c, ie_cand_cmp);
    uint64_t got = 0;
    for (uint32_t i = 0; i < n; i++) { mark[i] = got < need; if (mark[i]) got += c[i].bytes; }
    return got;
}
