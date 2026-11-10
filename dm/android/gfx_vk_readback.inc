// ---------------- Arena write-backs read through a cached copy (included by gfx_vk.c after gfx_vk_draw.inc) ----------------
// A storage buffer, buffer image or transform feedback buffer a shader writes lives in the arena, and the completion thread reads it back to land the GPU's bytes in guest
// memory (chunk_done -> wb_apply). The arena takes a host-visible, coherent and cached memory type (ar_mem_type), but a type's flags do not say how the CPU maps it:
// Qualcomm's driver on a Retroid Pocket 5 (Adreno 650, V@0746.0) lists type 4 as 0xf, cached and coherent, yet the CPU reads it at 1.3 GB/s whether the GPU or the CPU
// wrote it, a 1 MB buffer read twice no faster the second time (malloc'd memory: 11-13 GB/s), so it is mapped uncached; wider loads do not help (NEON 64-byte loads 1.4
// GB/s, wb_apply's 8-byte compares 0.45 GB/s, a byte at a time 0.17 GB/s). Pokémon Brilliant Diamond's Eterna City ran at 14.9 fps there, the completion thread on a CPU
// 78% of the time, 70% of it on wb_apply's loads from the arena and 21% in memcpy from it. The same driver's type 2 (0xb: cached, not coherent) reads at 12-15 GB/s after
// vkInvalidateMappedMemoryRanges (0.05 ms a MB), and it lists that type for a buffer used as a transfer destination and never as a source, which the arena cannot be
// (texture uploads read it). So at a submission's end the GPU copies each arena write-back into a buffer of the chunk's own in such a type (rb_record, after the closing
// barrier: one vkCmdCopyBuffer per arena block), and the completion thread invalidates that buffer once and reads the write-backs from it (chunk_done).
// Whether a driver needs it is measured, not guessed from the flags: the first submission with arena write-backs times a CPU read of 256 KB of the arena's type and of
// the readback type (each written by the CPU, best of three passes), and copies only where the arena reads at under a third of the readback type's speed, so a driver
// whose cached type is cached for the CPU keeps reading the arena. The log names both speeds ([vk] arena write-backs: ...).
// DM_VK_WB_READBACK=0 (env.txt): read from the arena, as before; =1: copied wherever a cached transfer-destination type other than the arena's exists
static int g_rb = -1;   // -1 not decided yet, 0 read from the arena, 1 read through the copy (wb_rb_on)
static uint32_t g_rb_mt = UINT32_MAX; static int g_rb_coh;   // the readback buffers' memory type; coherent: no invalidate needed
static volatile uint64_t g_rb_sink;   // keeps the timed reads
static VkDeviceSize rb_align(void) { VkDeviceSize a = g_vk.props.limits.nonCoherentAtomSize; return a > 64 ? a : 64; }   // each copy's offset: invalidated ranges stay atom-aligned
static VkDeviceSize wb_extent(const wb_t *w) { return w->rows ? (VkDeviceSize)w->src_pitch * (w->rows - 1) + w->row_bytes : 0; }   // the bytes it reads from its source
static uint32_t rb_mem_type(uint32_t bits) { return vkb_mem_type(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT); }
// MB/s of a CPU read of 256 KB in a buffer of that usage, in the arena's memory type (readback 0) or the readback type (1), the CPU's own writes read; *mt: the type
static double rb_speed(VkBufferUsageFlags usage, int readback, uint32_t *mt) {
    enum { N = 256u << 10 };
    *mt = UINT32_MAX;
    VkBufferCreateInfo bc = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = N, .usage = usage}; VkBuffer b;
    if (vkCreateBuffer(g_vk.dev, &bc, NULL, &b) != VK_SUCCESS) return 0;
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(g_vk.dev, b, &mr);
    uint32_t t = readback ? rb_mem_type(mr.memoryTypeBits) : ar_mem_type(mr.memoryTypeBits); *mt = t;
    VkMemoryAllocateInfo ma = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = t}; VkDeviceMemory m = VK_NULL_HANDLE; uint8_t *p = NULL; double best = 0;
    if (t != UINT32_MAX && vkAllocateMemory(g_vk.dev, &ma, NULL, &m) == VK_SUCCESS && vkBindBufferMemory(g_vk.dev, b, m, 0) == VK_SUCCESS && vkMapMemory(g_vk.dev, m, 0, N, 0, (void **)&p) == VK_SUCCESS) {
        memset(p, 1, N);
        if (!(g_vk.mp.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { VkMappedMemoryRange r = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = m, .size = VK_WHOLE_SIZE}; vkFlushMappedMemoryRanges(g_vk.dev, 1, &r); }
        for (int k = 0; k < 3; k++) {
            uint64_t t0 = clock_gettime_nsec_np(CLOCK_MONOTONIC), s = 0; const uint64_t *w = (const uint64_t *)(const void *)p;
            for (size_t i = 0; i < N / 8; i++) s += w[i];
            uint64_t dt = clock_gettime_nsec_np(CLOCK_MONOTONIC) - t0; g_rb_sink += s;
            double mbs = (double)N * 1e3 / (double)(dt ? dt : 1); if (mbs > best) best = mbs;
        }
        vkUnmapMemory(g_vk.dev, m);
    }
    if (m) vkFreeMemory(g_vk.dev, m, NULL);
    vkDestroyBuffer(g_vk.dev, b, NULL);
    return best;
}
static int wb_rb_on(void) {   // the replay thread (chunk_submit), once
    if (g_rb >= 0) return g_rb;
    const char *e = getenv("DM_VK_WB_READBACK"); int force = e && *e ? *e != '0' : -1;
    uint32_t amt, rmt; double a = rb_speed(AR_USAGE, 0, &amt), r = rb_speed(VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, &rmt);
    int have = rmt != UINT32_MAX && rmt != amt && r > 0;
    g_rb = force == 0 || !have ? 0 : force == 1 ? 1 : a * 3 < r;
    if (have) { g_rb_mt = rmt; g_rb_coh = (g_vk.mp.memoryTypes[rmt].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0; }
    char tail[200] = "no other cached memory type takes a transfer destination";
    if (have) snprintf(tail, sizeof tail, "a transfer destination's cached type %u (flags %#x) reads at %.0f MB/s", rmt, g_vk.mp.memoryTypes[rmt].propertyFlags, r);
    LOG("[vk] arena write-backs: the arena's memory type %u reads at %.0f MB/s on the CPU, %s: %s%s", amt, a, tail,
        g_rb ? "they are copied there on the GPU at each submission's end and read from the copy" : "they are read from the arena",
        force == 0 ? " (DM_VK_WB_READBACK=0)" : force == 1 ? " (DM_VK_WB_READBACK=1)" : "");
    return g_rb;
}
static void rb_free(chunk *c) { if (c->rb.b) vkDestroyBuffer(g_vk.dev, c->rb.b, NULL); if (c->rb.m) vkFreeMemory(g_vk.dev, c->rb.m, NULL); memset(&c->rb, 0, sizeof c->rb); }
// the chunk's readback buffer, at least need bytes (made again larger: its last use completed before the chunk was taken to record) → 0: none, read from the arena
static int rb_grow(chunk *c, VkDeviceSize need) {
    VkDeviceSize n = c->rb.n ? c->rb.n : (VkDeviceSize)1 << 20; while (n < need) n *= 2;
    rb_free(c);
    VkBufferCreateInfo bc = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = n, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    if (vkCreateBuffer(g_vk.dev, &bc, NULL, &c->rb.b) != VK_SUCCESS) { c->rb.b = VK_NULL_HANDLE; return 0; }
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(g_vk.dev, c->rb.b, &mr);
    VkMemoryAllocateInfo ma = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = g_rb_mt};
    if (!(mr.memoryTypeBits >> g_rb_mt & 1) || vkAllocateMemory(g_vk.dev, &ma, NULL, &c->rb.m) != VK_SUCCESS) { rb_free(c); RED_ONCE(160, "arena write-back readback buffer of %llu bytes failed: read from the arena", (unsigned long long)n); return 0; }
    vkBindBufferMemory(g_vk.dev, c->rb.b, c->rb.m, 0);
    if (vkMapMemory(g_vk.dev, c->rb.m, 0, n, 0, (void **)&c->rb.p) != VK_SUCCESS) { rb_free(c); return 0; }
    c->rb.n = n; return 1;
}
// chunk_submit, after the closing barrier: the GPU copies the chunk's arena write-backs into its readback buffer; each one copied notes where (wb_t.rb)
static void rb_record(chunk *c) {
    if (!c->nwb || !wb_rb_on()) return;
    const VkDeviceSize al = rb_align(); VkDeviceSize need = 0; int nrg = 0;
    for (int i = 0; i < c->nwb; i++) { VkDeviceSize n = c->wb[i].st < 0 ? wb_extent(&c->wb[i]) : 0; if (n) { need += (n + al - 1) & ~(al - 1); nrg++; } }
    if (!need || (need > c->rb.n && !rb_grow(c, need))) return;
    static VkBufferCopy *rg; static int crg;   // the replay thread's alone (chunk_submit runs under g_bk)
    if (nrg > crg) { VkBufferCopy *g = realloc(rg, (size_t)nrg * sizeof *g); if (!g) return; rg = g; crg = nrg; }
    VkDeviceSize at = 0;
    for (int k = 0; k < c->nar; k++) {
        int n = 0;
        for (int i = 0; i < c->nwb; i++) { wb_t *w = &c->wb[i]; VkDeviceSize sz = w->st == -1 - k ? wb_extent(w) : 0; if (!sz) continue;
            rg[n++] = (VkBufferCopy){w->off, at, sz}; w->rb = (size_t)at + 1; at += (sz + al - 1) & ~(al - 1); }
        if (n) vkCmdCopyBuffer(c->cb, c->ar[k].b, c->rb.b, (uint32_t)n, rg);
    }
    c->rb.used = at;
    VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(c->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
}
// the completion thread, in chunk_done before the write-backs land: the copies' bytes made visible to the CPU (the type is not coherent)
static void rb_invalidate(chunk *c) {
    if (!c->rb.used || g_rb_coh) return;
    const VkDeviceSize al = rb_align(), n = (c->rb.used + al - 1) & ~(al - 1);
    VkMappedMemoryRange r = {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = c->rb.m, .size = n >= c->rb.n ? VK_WHOLE_SIZE : n};
    vkInvalidateMappedMemoryRanges(g_vk.dev, 1, &r);
}
static const char *rb_state(void) { return g_rb < 0 ? "not decided yet" : g_rb ? "read through a cached copy" : "read from the arena"; }
