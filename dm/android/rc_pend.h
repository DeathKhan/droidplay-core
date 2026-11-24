// Conditional rendering's pending timestamp reports (gfx_vk.c rc_note, rc_value: render_cond), found through an open-addressing index instead of a walk of the list.
// Self-contained: dm/tools/tests/test_rc_pend.py drives it on the PC.
// - What an entry says: an address whose latest report the replay has met and that has not landed counts as 0 if that report was a timestamp one (a timestamp report
//   puts 0 in the counter field), and as unknown if a report of another counter kind came after it. Once the report has landed (its serial at or below the completion
//   thread's: gfx_vk.c post_apply), the entry counts as absent, so a condition reads memory, CPU rewrites included, or the counter report address set (g_rc_occ) for an
//   unknown one. Before, an entry stayed until 128 later addresses pushed it out: a record the CPU set to 1 after its report landed still read 0, and the conditional clear
//   it guards was skipped. The Mac expires an entry when its submission completes (nvn_mtl.m rc_commit, g_rc_complete).
// - Which reports take an entry: timestamp ones only. A samples-passed report, or one of a counter kind the host does not implement, is the address set's. They were added
//   here too, and Ocarina of Time's 530 or so occlusion reports a frame filled the 256 entries: a full list dropped its older half, so a timestamp report noted before them
//   in the same submission was forgotten, and a condition on it read the 1 the CPU had written there. The game clears its 768x432 refraction target under such a condition
//   at every bind and reports once: the later clears ran and wiped the scene copy (DeathKhan/nx2android's replay on an Ayn Odin 3: the water opaque brown).
// - A full list lets the landed entries go first. Only when every entry is still in flight do the least recently noted half go (a note moves an entry's order, so a
//   record noted again in this submission stays), and the caller logs it.
// - The index: each report and each condition walked the whole list, 4.5% of Ocarina of Time's replay thread on a Galaxy Z Fold 8 in Kokiri Forest. It holds each address's
//   position in the list, at most RC_PEND of its RC_PIDX slots (no deletions: rebuilt when the list lets entries go), so a probe ends within a few steps.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { RC_PEND = 256, RC_PIDX = 1024 };
enum { RCP_TIMESTAMP = 1, RCP_UNKNOWN = 2 };   // an entry's kind: a timestamp report (counter 0), or a report of another counter after one to the same address
typedef struct { uint64_t addr, seq, at; int kind; } rc_pent;   // seq: its latest report's serial (landed once the completion thread has applied it); at: its latest note
typedef struct { rc_pent e[RC_PEND]; int n; uint16_t idx[RC_PIDX]; uint64_t at, expired, dropped; } rc_pend;   // idx: position + 1 in e, 0 empty; expired: landed entries let go,
                                                                                                                  // dropped: entries in flight let go when the list was full
static inline uint32_t rcp_slot(const rc_pend *p, uint64_t a) {
    uint32_t i = (uint32_t)((a >> 4) * 0x9E3779B1u) & (RC_PIDX - 1);
    while (p->idx[i] && p->e[p->idx[i] - 1].addr != a) i = (i + 1) & (RC_PIDX - 1);
    return i;
}
// the kind of address a's entry (RCP_TIMESTAMP, RCP_UNKNOWN), or 0 when it has none or its report has landed (serial at or below landed): memory's value then, or the address set's
static inline int rcp_find(const rc_pend *p, uint64_t a, uint64_t landed) {
    uint32_t i = rcp_slot(p, a); if (!p->idx[i]) return 0;
    const rc_pent *e = &p->e[p->idx[i] - 1];
    return e->seq > landed ? e->kind : 0;
}
static inline int rcp_at_cmp(const void *x, const void *y) { uint64_t a = *(const uint64_t *)x, b = *(const uint64_t *)y; return a < b ? -1 : a > b; }
// a full list: the landed entries go; if none has, the least recently noted half
static inline void rcp_trim(rc_pend *p, uint64_t landed) {
    int k = 0;
    for (int j = 0; j < p->n; j++) if (p->e[j].seq > landed) p->e[k++] = p->e[j];
    p->expired += (uint64_t)(p->n - k); p->n = k;
    if (p->n == RC_PEND) {
        uint64_t at[RC_PEND]; for (int j = 0; j < RC_PEND; j++) at[j] = p->e[j].at;
        qsort(at, RC_PEND, sizeof *at, rcp_at_cmp);
        uint64_t keep = at[RC_PEND / 2];   // (each note has its own at: exactly half stay)
        k = 0; for (int j = 0; j < RC_PEND; j++) if (p->e[j].at >= keep) p->e[k++] = p->e[j];
        p->dropped += (uint64_t)(RC_PEND - k); p->n = k;
    }
    memset(p->idx, 0, sizeof p->idx);
    for (int j = 0; j < p->n; j++) p->idx[rcp_slot(p, p->e[j].addr)] = (uint16_t)(j + 1);
}
// a report to address a, with serial seq: timestamp 1 for a timestamp report, 0 for any other counter kind. The address's entry takes the new kind and serial; a timestamp
// report takes a new entry (a full list trimmed first: rcp_trim); another kind takes none
static inline void rcp_note(rc_pend *p, uint64_t a, int timestamp, uint64_t seq, uint64_t landed) {
    uint32_t i = rcp_slot(p, a);
    if (p->idx[i]) { rc_pent *e = &p->e[p->idx[i] - 1]; e->kind = timestamp ? RCP_TIMESTAMP : RCP_UNKNOWN; e->seq = seq; e->at = ++p->at; return; }
    if (!timestamp) return;   // (the counter report address set holds it)
    if (p->n == RC_PEND) { rcp_trim(p, landed); i = rcp_slot(p, a); }
    rc_pent *e = &p->e[p->n]; e->addr = a; e->kind = RCP_TIMESTAMP; e->seq = seq; e->at = ++p->at; p->idx[i] = (uint16_t)++p->n;
}
