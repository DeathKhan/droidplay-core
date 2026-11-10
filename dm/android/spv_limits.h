// Subgroup operations in a stage where the GPU runs none. The translator (Ryujinx's) runs before the GPU is known and turns Maxwell's warp instructions into
// subgroup operations in every stage: SHFL into OpGroupNonUniformShuffle (Up, Down, Xor), VOTE into Ballot / All / Any / AllEqual, S2R SR_LANEID and the lane
// masks into the subgroup built-ins, and FSWZADD reads the lane ID. Vulkan allows them only in subgroupSupportedStages (VUID-RuntimeSpirv-None-06343): compute on
// the Adreno 650, fragment and compute on the 740 and 830. The Adreno 650's compiler crashes on a fragment shader's shuffle (libllvm-qgl.so+0x2dcef0, Brilliant
// Diamond's fwidth, which Maxwell computes with SHFL and FSWZADD). spv_limits rewrites such a module without them, so that the shader does the same:
// - Fragment shaders: the subgroup is the 2x2 quad, the lane ID (x & 1) | (y & 1) << 1 from gl_FragCoord, as Maxwell numbers a quad's lanes and as Vulkan's quad
//   operations do. A shuffle reads the named lane of the quad through fine derivatives, which every quad computes: the value's two 16-bit halves as floats (exact
//   integers), the other column's by DPdxFine and the other row's by DPdyFine, the lane picked; so the lanes of a quad exchange any 32 bits exactly, which is
//   all Maxwell's derivative idiom asks (SHFL.IDX within the quad). Ballot, vote and elect see the invocation alone (its own bit, the predicate, true): exact in
//   non-uniform control flow, where derivatives are not, and for Maxwell's uses of them (each lane its own leader: one atomic per lane rather than per subgroup);
//   the masks follow the lane ID.
// - Other stages: one lane, as the Apple chain's spv_single_lane (dmsh_build.py): lane ID 0, size 1, shuffles and broadcasts the lane's own value.
// The subgroup built-ins and capabilities go; a module with instructions it does not cover (bit counts, arithmetic, quad and KHR operations) is left as it is.
// Self-contained (no libc) so that dm/tools/tests/test_spv_limits.py builds it alone and runs it over a game's shaders on the PC.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "spv_lit.h"

enum { SPV_LIM_SG_QUAD = 1, SPV_LIM_SG_ONE = 2, SPV_LIM_SG_KEPT = 4 };   // rewritten as a fragment shader's quad, as one lane; uses it cannot rewrite: as it is
// Capabilities GroupNonUniform*, SubgroupBallotKHR, SubgroupVoteKHR, GroupNonUniformPartitionedNV, GroupNonUniformRotateKHR (not DrawParameters, 4427)
static int spv_lim_sgcap(uint32_t c) { return (c >= 61 && c <= 68) || c == 4423 || c == 4431 || c == 5297 || c == 6026; }
// OpGroupNonUniform*, OpSubgroup*KHR, OpGroupNonUniformRotateKHR, OpGroupNonUniformPartitionNV
static int spv_lim_sgop(uint32_t op) { return (op >= 333 && op <= 366) || op == 4421 || op == 4422 || (op >= 4428 && op <= 4432) || op == 5296; }
// The kinds of ids the scan follows: subgroup built-in variables, a pointer to one component of a mask (| SLK_PTR), gl_FragCoord, an Input pointer to a vec4
enum { SLK_LANE = 1, SLK_SIZE, SLK_EQ, SLK_GE, SLK_GT, SLK_LE, SLK_LT, SLK_OTHER, SLK_FRAG, SLK_PV4F, SLK_PTR = 0x100 };
static uint32_t spv_lim_bi(uint32_t b) {   // BuiltIn → its kind: SubgroupLocalInvocationId, SubgroupSize, Eq / Ge / Gt / Le / Lt masks, NumSubgroups and SubgroupId
    switch (b) { case 41: return SLK_LANE; case 36: return SLK_SIZE; case 38: case 40: return SLK_OTHER; default: return b >= 4416 && b <= 4420 ? SLK_EQ + (b - 4416) : 0; }
}
static int spv_lim_sgk(uint32_t k) { return (k >= SLK_LANE && k <= SLK_OTHER) || (k & SLK_PTR); }
struct spv_lim_c { uint32_t tu, ti, tf, tb, tv4u, tv4f, frag, u0, u1, u2, u4, u16, uffff, uf, ue, f05, f1, fm1, t; };
static size_t spv_lim_put(uint32_t *o, size_t k, uint32_t op, const uint32_t *a, uint32_t na) { o[k++] = (na + 1) << 16 | op; for (uint32_t q = 0; q < na; q++) o[k++] = a[q]; return k; }
#define SPV_LIM_PUT(op, ...) do { const uint32_t a_[] = {__VA_ARGS__}; k = spv_lim_put(o, k, (op), a_, (uint32_t)(sizeof a_ / sizeof *a_)); } while (0)
// The quad lane from gl_FragCoord into l (an id given, or a new one), with its column and row bits
static size_t spv_lim_lane(uint32_t *o, size_t k, uint32_t *nb, const struct spv_lim_c *c, uint32_t l, uint32_t *lx, uint32_t *ly) {
    uint32_t fc = (*nb)++, fx = (*nb)++, fy = (*nb)++, ux = (*nb)++, uy = (*nb)++, sy = (*nb)++; *lx = (*nb)++; *ly = (*nb)++;
    SPV_LIM_PUT(61, c->tv4f, fc, c->frag); SPV_LIM_PUT(81, c->tf, fx, fc, 0); SPV_LIM_PUT(81, c->tf, fy, fc, 1);   // OpLoad, OpCompositeExtract
    SPV_LIM_PUT(109, c->tu, ux, fx); SPV_LIM_PUT(109, c->tu, uy, fy);   // OpConvertFToU
    SPV_LIM_PUT(199, c->tu, *lx, ux, c->u1); SPV_LIM_PUT(199, c->tu, *ly, uy, c->u1);   // OpBitwiseAnd
    SPV_LIM_PUT(196, c->tu, sy, *ly, c->u1); SPV_LIM_PUT(197, c->tu, l, *lx, sy);   // OpShiftLeftLogical, OpBitwiseOr
    return k;
}
// r (type t: a 32-bit float, int or uint) = v of the quad's lane s (& 3), in the lane at column lx, row ly. Each half h of v's bits, as a float: the other column's
// h + kx · DPdxFine(h) (kx = ±1 towards it), the other row's of both by DPdyFine the same way, then the lane's of the four picked. Differences of integers under 2^16
// are exact in floats, the lane's own value is picked as it is, and the + 0.5 before the conversion keeps a value that came back a hair under its integer
static size_t spv_lim_xchg(uint32_t *o, size_t k, uint32_t *nb, const struct spv_lim_c *c, uint32_t t, uint32_t r, uint32_t v, uint32_t s, uint32_t lx, uint32_t ly) {
    uint32_t bits = v, half[2], sx = (*nb)++, s1 = (*nb)++, sy = (*nb)++, ex = (*nb)++, ey = (*nb)++, zx = (*nb)++, zy = (*nb)++, kx = (*nb)++, ky = (*nb)++;
    if (t != c->tu) { bits = (*nb)++; SPV_LIM_PUT(124, c->tu, bits, v); }   // OpBitcast
    half[0] = (*nb)++; half[1] = (*nb)++;
    SPV_LIM_PUT(199, c->tu, half[0], bits, c->uffff); SPV_LIM_PUT(194, c->tu, half[1], bits, c->u16);   // OpBitwiseAnd, OpShiftRightLogical
    SPV_LIM_PUT(199, c->tu, sx, s, c->u1); SPV_LIM_PUT(194, c->tu, s1, s, c->u1); SPV_LIM_PUT(199, c->tu, sy, s1, c->u1);
    SPV_LIM_PUT(170, c->tb, ex, sx, lx); SPV_LIM_PUT(170, c->tb, ey, sy, ly); SPV_LIM_PUT(170, c->tb, zx, lx, c->u0); SPV_LIM_PUT(170, c->tb, zy, ly, c->u0);   // OpIEqual
    SPV_LIM_PUT(169, c->tf, kx, zx, c->f1, c->fm1); SPV_LIM_PUT(169, c->tf, ky, zy, c->f1, c->fm1);   // OpSelect: towards the other column / row
    uint32_t u[2];
    for (int h = 0; h < 2; h++) {
        uint32_t a = (*nb)++, d = (*nb)++, m = (*nb)++, b = (*nb)++, da = (*nb)++, ma = (*nb)++, a2 = (*nb)++, db = (*nb)++, mb = (*nb)++, b2 = (*nb)++;
        uint32_t r0 = (*nb)++, r1 = (*nb)++, rr = (*nb)++, rh = (*nb)++; u[h] = (*nb)++;
        SPV_LIM_PUT(112, c->tf, a, half[h]);   // OpConvertUToF
        SPV_LIM_PUT(210, c->tf, d, a); SPV_LIM_PUT(133, c->tf, m, kx, d); SPV_LIM_PUT(129, c->tf, b, a, m);   // the other column: OpDPdxFine, OpFMul, OpFAdd
        SPV_LIM_PUT(211, c->tf, da, a); SPV_LIM_PUT(133, c->tf, ma, ky, da); SPV_LIM_PUT(129, c->tf, a2, a, ma);   // the other row: OpDPdyFine
        SPV_LIM_PUT(211, c->tf, db, b); SPV_LIM_PUT(133, c->tf, mb, ky, db); SPV_LIM_PUT(129, c->tf, b2, b, mb);   // the other column's other row
        SPV_LIM_PUT(169, c->tf, r0, ex, a, b); SPV_LIM_PUT(169, c->tf, r1, ex, a2, b2); SPV_LIM_PUT(169, c->tf, rr, ey, r0, r1);
        SPV_LIM_PUT(129, c->tf, rh, rr, c->f05); SPV_LIM_PUT(109, c->tu, u[h], rh);
    }
    uint32_t hi = (*nb)++, w = t == c->tu ? r : (*nb)++;
    SPV_LIM_PUT(196, c->tu, hi, u[1], c->u16); SPV_LIM_PUT(197, c->tu, w, hi, u[0]);
    if (t != c->tu) SPV_LIM_PUT(124, t, r, w);
    return k;
}

// 1 when the module may need spv_limits: a subgroup instruction or built-in in a module whose stage (its first entry point's) runs none. One pass over the instruction
// heads, without the scratch, so that the modules of a GPU's other stages, and the ones that use none, cost next to nothing
static int spv_lim_wanted(const uint32_t *w, size_t n, uint32_t sg_stages) {
    if (n < 5 || w[0] != 0x07230203) return 0;
    int any = 0;
    for (size_t i = 5; i < n; ) {
        uint32_t len = w[i] >> 16, op = w[i] & 0xffff;
        if (!len || i + len > n) return 0;
        if (op == 15 && len >= 3 && (w[i + 1] > 5 || (sg_stages & (1u << w[i + 1])))) return 0;   // OpEntryPoint of a stage that runs them (or not a shader stage)
        if (spv_lim_sgop(op) || (op == 71 && len >= 4 && w[i + 2] == 11 && spv_lim_bi(w[i + 3]))) any = 1;
        i += len;
    }
    return any;
}

// w[n]: the module. sg_stages: subgroupSupportedStages. scratch: 2 × bound words (w[3]). o[cap]: the result.
// → the result's word count, or 0 when it is left as it is: its stage runs subgroup operations or it uses none (*need 0), *what SPV_LIM_SG_KEPT, or o is NULL or
// smaller than *need, the words a rewrite takes (call again with that much)
static size_t spv_limits(const uint32_t *w, size_t n, uint32_t sg_stages, uint32_t *scratch, uint32_t *o, size_t cap, size_t *need, int *what) {
    *what = 0; *need = 0;
    if (n < 5 || w[0] != 0x07230203) return 0;
    uint32_t bound = w[3], *kind = scratch, *aux = scratch + bound, model = 99, frag = 0, tpv4f = 0;
    for (uint32_t i = 0; i < 2 * bound; i++) scratch[i] = 0;
    struct spv_lim_c c = {0};
    int neps = 0, fn = 0, used = 0, bad = 0, dctl = 0, sites = 0, xch = 0, lanes = 0, listed = 0; size_t ep = 0;
#define ID(x) ((x) < bound)
    for (size_t i = 5; i < n; ) {
        uint32_t len = w[i] >> 16, op = w[i] & 0xffff;
        if (!len || i + len > n) return 0;
        const uint32_t *x = w + i;
        if (op == 54) fn = 1;   // the functions come after every global declaration
        if (op == 17 && len == 2 && x[1] == 51) dctl = 1;   // OpCapability DerivativeControl
        else if (op == 15 && len >= 3) {   // OpEntryPoint model function "name" interface...
            neps++; model = x[1]; ep = i;
        }
        else if (op == 71 && len >= 4 && x[2] == 11 && ID(x[1])) {   // OpDecorate BuiltIn
            if (x[3] == 15) { kind[x[1]] = SLK_FRAG; frag = x[1]; } else if (spv_lim_bi(x[3])) kind[x[1]] = spv_lim_bi(x[3]);
        }
        else if (op >= 73 && op <= 75) bad = 1;   // decoration groups: not followed
        else if (op == 20 && len == 2) c.tb = x[1];   // OpTypeBool
        else if (op == 21 && len == 4 && x[2] == 32) { if (x[3]) c.ti = x[1]; else c.tu = x[1]; }   // OpTypeInt 32
        else if (op == 22 && len == 3 && x[2] == 32) c.tf = x[1];   // OpTypeFloat 32
        else if (op == 23 && len == 4 && x[3] == 4) { if (c.tf && x[2] == c.tf) c.tv4f = x[1]; if (c.tu && x[2] == c.tu) c.tv4u = x[1]; }   // OpTypeVector
        else if (op == 32 && len == 4 && x[2] == 1 && c.tv4f && x[3] == c.tv4f && ID(x[1])) { kind[x[1]] = SLK_PV4F; tpv4f = x[1]; }   // OpTypePointer Input vec4
        else if (op == 59 && !fn && len >= 4 && frag && x[2] == frag && !(ID(x[1]) && kind[x[1]] == SLK_PV4F)) bad = 1;   // gl_FragCoord of another type
        else if (fn && spv_lim_sgop(op)) {
            used = 1; sites++;
            int ok = op == 334 || op == 335 || op == 338;   // All, Any: the predicate; BroadcastFirst: the own value
            if (op == 333 || op == 336) ok = c.tb && x[1] == c.tb;   // Elect, AllEqual: true
            else if (op == 339) { ok = len == 5 && c.tv4u && x[1] == c.tv4u; lanes++; }   // Ballot
            else if (op == 337 || (op >= 345 && op <= 348)) {   // Broadcast, Shuffle, ShuffleXor / Up / Down: type result scope value id
                ok = len == 6 && x[1] && (x[1] == c.tf || x[1] == c.tu || x[1] == c.ti); xch++; lanes++;
            }
            if (!ok) bad = 1;
        }
        else if (fn && op == 61 && len >= 4 && ID(x[3]) && spv_lim_sgk(kind[x[3]])) {   // OpLoad of a subgroup built-in or a mask's component
            uint32_t kd = kind[x[3]]; used = 1; sites++; lanes++;
            if (kd == SLK_OTHER || x[1] != ((kd & SLK_PTR) || kd == SLK_LANE || kd == SLK_SIZE ? c.tu : c.tv4u) || !x[1]) bad = 1;
        }
        else if (fn && (op == 65 || op == 66) && len >= 4 && ID(x[3]) && kind[x[3]] >= SLK_EQ && kind[x[3]] <= SLK_LT) {   // an access chain to a mask's component
            if (len == 5 && ID(x[2])) { kind[x[2]] = SLK_PTR | kind[x[3]]; aux[x[2]] = x[4]; } else bad = 1;
        }
        else if (fn) for (uint32_t q = 1; q < len; q++) if (!spv_lit(op, q) && ID(x[q]) && spv_lim_sgk(kind[x[q]])) bad = 1;   // any other use of one: not followed
        i += len;
    }
    if (!used || neps != 1 || model > 5 || (sg_stages & (1u << model))) return 0;   // the execution model is the stage's bit (vertex 0x1 .. compute 0x20)
    if (frag) {   // gl_FragCoord in the entry point's interface already (it is declared before the decorations that name it)
        uint32_t len = w[ep] >> 16; size_t s = 3;
        while (s < len) { uint32_t y = w[ep + s++]; if (!(y & 0xff) || !(y & 0xff00) || !(y & 0xff0000) || !(y & 0xff000000)) break; }
        for (; s < len; s++) if (w[ep + s] == frag) listed = 1;
    }
    int quad = model == 4; lanes = quad && lanes;
    if (bad || !c.tu) { *what = SPV_LIM_SG_KEPT; return 0; }
    *what = quad ? SPV_LIM_SG_QUAD : SPV_LIM_SG_ONE;
    *need = n + 300 * (size_t)sites + 128;
    if (!o || cap < *need) return 0;
    uint32_t nb = bound, new_tb = 0, new_tf = 0, new_v4f = 0, new_pv4f = 0, new_frag = 0;
    if (!c.tb) c.tb = new_tb = nb++;
    if (!c.tf && lanes) c.tf = new_tf = nb++;
    if (lanes && !frag) {
        if (!c.tv4f) c.tv4f = new_v4f = nb++;
        if (!tpv4f || new_v4f) tpv4f = new_pv4f = nb++;
        frag = new_frag = nb++;
    }
    c.frag = frag;
    c.u0 = nb++; c.u1 = nb++; c.u2 = nb++; c.u4 = nb++; c.u16 = nb++; c.uffff = nb++; c.uf = nb++; c.ue = nb++; c.t = nb++;
    if (c.tf) { c.f05 = nb++; c.f1 = nb++; c.fm1 = nb++; }
    size_t k = 0; int capped = 0, decorated = 0, placed = 0; fn = 0;
    for (size_t i = 0; i < 5; i++) o[k++] = w[i];
    for (size_t i = 5; i < n; ) {
        uint32_t len = w[i] >> 16, op = w[i] & 0xffff;
        const uint32_t *x = w + i;
        i += len;
        if (op != 17 && !capped) { capped = 1; if (quad && xch && !dctl) SPV_LIM_PUT(17, 51); }   // OpCapability DerivativeControl, last of the capabilities
        if (op == 17 && spv_lim_sgcap(x[1])) continue;
        if (op == 10 && len >= 2) {   // OpExtension: the ones that only bring subgroup capabilities
            const char *e = (const char *)(x + 1), *names[] = {"SPV_KHR_shader_ballot", "SPV_KHR_subgroup_vote", "SPV_KHR_subgroup_rotate", "SPV_NV_shader_subgroup_partitioned"};
            int drop = 0;
            for (int q = 0; q < 4 && !drop; q++) { uint32_t b = 0; while (b < (len - 1) * 4 && e[b] == names[q][b] && names[q][b]) b++; drop = b < (len - 1) * 4 && !e[b] && !names[q][b]; }
            if (drop) continue;
        }
        if ((op == 5 || op == 71 || op == 332 || op == 5632) && len >= 2 && ID(x[1]) && kind[x[1]] >= SLK_LANE && kind[x[1]] <= SLK_LT) continue;   // the built-ins' names and decorations
        if (!decorated && new_frag && ((op >= 19 && op <= 52) || op == 59)) { decorated = 1; SPV_LIM_PUT(71, new_frag, 11, 15); }   // OpDecorate BuiltIn FragCoord, last of the annotations
        if (op == 59 && !fn && ID(x[2]) && kind[x[2]] >= SLK_LANE && kind[x[2]] <= SLK_LT) continue;   // the built-in variables
        if (op == 15) {   // the entry point without them, with gl_FragCoord
            size_t at = k, s = 3;
            while (s < len) { uint32_t y = x[s++]; if (!(y & 0xff) || !(y & 0xff00) || !(y & 0xff0000) || !(y & 0xff000000)) break; }
            for (size_t q = 0; q < s; q++) o[k++] = x[q];
            for (; s < len; s++) if (!(ID(x[s]) && kind[x[s]] >= SLK_LANE && kind[x[s]] <= SLK_LT)) o[k++] = x[s];
            if (lanes && (new_frag || !listed)) o[k++] = frag;
            o[at] = (uint32_t)(k - at) << 16 | op;
            continue;
        }
        if (op == 54 && !placed) {   // before the first function: the types, constants and gl_FragCoord it lacks
            placed = 1; fn = 1;
            if (new_tb) SPV_LIM_PUT(20, new_tb);
            if (new_tf) SPV_LIM_PUT(22, new_tf, 32);
            if (new_v4f) SPV_LIM_PUT(23, new_v4f, c.tf, 4);
            if (new_pv4f) SPV_LIM_PUT(32, new_pv4f, 1, c.tv4f);
            SPV_LIM_PUT(43, c.tu, c.u0, 0); SPV_LIM_PUT(43, c.tu, c.u1, 1); SPV_LIM_PUT(43, c.tu, c.u2, 2); SPV_LIM_PUT(43, c.tu, c.u4, 4);
            SPV_LIM_PUT(43, c.tu, c.u16, 16); SPV_LIM_PUT(43, c.tu, c.uffff, 0xffff); SPV_LIM_PUT(43, c.tu, c.uf, 0xf); SPV_LIM_PUT(43, c.tu, c.ue, 0xe); SPV_LIM_PUT(41, c.tb, c.t);
            if (c.tf) { SPV_LIM_PUT(43, c.tf, c.f05, 0x3f000000); SPV_LIM_PUT(43, c.tf, c.f1, 0x3f800000); SPV_LIM_PUT(43, c.tf, c.fm1, 0xbf800000); }
            if (new_frag) SPV_LIM_PUT(59, tpv4f, new_frag, 1);
        }
        if (fn && spv_lim_sgop(op)) {
            uint32_t lx, ly, l;
            if (op == 333 || op == 336) SPV_LIM_PUT(83, x[1], x[2], c.t);   // OpCopyObject true
            else if (op == 334 || op == 335 || op == 338 || !quad) {
                if (op == 339) { uint32_t b = nb++; SPV_LIM_PUT(169, c.tu, b, x[4], c.u1, c.u0); SPV_LIM_PUT(80, c.tv4u, x[2], b, c.u0, c.u0, c.u0); }   // the own bit
                else SPV_LIM_PUT(83, x[1], x[2], x[4]);   // the predicate, the own value
            }
            else {
                l = nb++; k = spv_lim_lane(o, k, &nb, &c, l, &lx, &ly);
                if (op == 339) { uint32_t s = nb++, b = nb++; SPV_LIM_PUT(196, c.tu, s, c.u1, l); SPV_LIM_PUT(169, c.tu, b, x[4], s, c.u0); SPV_LIM_PUT(80, c.tv4u, x[2], b, c.u0, c.u0, c.u0); }
                else {
                    uint32_t s = x[5];
                    if (op == 346) { s = nb++; SPV_LIM_PUT(198, c.tu, s, l, x[5]); }   // ShuffleXor: OpBitwiseXor
                    else if (op == 347) { s = nb++; SPV_LIM_PUT(130, c.tu, s, l, x[5]); }   // ShuffleUp: OpISub
                    else if (op == 348) { s = nb++; SPV_LIM_PUT(128, c.tu, s, l, x[5]); }   // ShuffleDown: OpIAdd
                    k = spv_lim_xchg(o, k, &nb, &c, x[1], x[2], x[4], s, lx, ly);
                }
            }
            continue;
        }
        if (fn && (op == 65 || op == 66) && ID(x[2]) && (kind[x[2]] & SLK_PTR)) continue;   // a mask component's pointer: its loads take the component
        if (fn && op == 61 && ID(x[3]) && spv_lim_sgk(kind[x[3]])) {
            uint32_t kd = kind[x[3]] & 0xff, lx, ly, l, v = (kind[x[3]] & SLK_PTR) ? nb++ : x[2], m;
            if (kd == SLK_LANE) { if (quad) k = spv_lim_lane(o, k, &nb, &c, x[2], &lx, &ly); else SPV_LIM_PUT(83, c.tu, x[2], c.u0); continue; }
            if (kd == SLK_SIZE) { SPV_LIM_PUT(83, c.tu, x[2], quad ? c.u4 : c.u1); continue; }
            if (quad) {   // the masks of the quad's lanes
                uint32_t t = nb++; l = nb++; m = nb++; k = spv_lim_lane(o, k, &nb, &c, l, &lx, &ly);
                if (kd == SLK_EQ) SPV_LIM_PUT(196, c.tu, m, c.u1, l);
                else if (kd == SLK_GE || kd == SLK_GT) { SPV_LIM_PUT(196, c.tu, t, kd == SLK_GE ? c.uf : c.ue, l); SPV_LIM_PUT(199, c.tu, m, t, c.uf); }
                else { SPV_LIM_PUT(196, c.tu, t, kd == SLK_LE ? c.u2 : c.u1, l); SPV_LIM_PUT(130, c.tu, m, t, c.u1); }
            } else m = kd == SLK_GT || kd == SLK_LT ? c.u0 : c.u1;   // one lane: Eq, Ge and Le are its own bit
            SPV_LIM_PUT(80, c.tv4u, v, m, c.u0, c.u0, c.u0);   // OpCompositeConstruct
            if (kind[x[3]] & SLK_PTR) SPV_LIM_PUT(77, c.tu, x[2], v, aux[x[3]]);   // OpVectorExtractDynamic
            continue;
        }
        for (uint32_t q = 0; q < len; q++) o[k++] = x[q];
    }
#undef ID
    o[3] = nb;
    return k;
}
#undef SPV_LIM_PUT
