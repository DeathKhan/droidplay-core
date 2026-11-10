// The game's modules back to back in one reservation, in load order, as Horizon's loader maps a process's modules: each one starts where the last ends
// (Atmosphère's ldr adds every module's size, page aligned, to the next one's address). A read past the end of one module then lands in the next, as on
// a Switch. loader.c's mmap(NULL) left that to the kernel, which on Android places mappings top down wherever it finds room: what followed a module was
// whatever happened to be mapped there, often the hole its freed file buffer left, filled or not by the time the game read it.
// Link's Awakening reads entry 0xFFFF of a 0x58-byte table in main on every boot (the camera constructor at main+0x1c180: the index stays at the
// caller's default), 5.5 MB past main's end, which is subsdk0's code on a Switch: unmapped memory on every boot of a Retroid Pocket 5, now and then
// on a OnePlus Pad 3, never on an Ayn Odin 2 (docs/reference/memory.md, Modules back to back, as Horizon maps them).
// loader.c asks through two weak hooks (dm_modules_reserve, dm_module_slot); NROs, which nn::ro maps where it likes, are not part of the layout.
#include "dm.h"
#include <sys/mman.h>
#include <sys/stat.h>

#define PACK_MAX 16   // a profile's modules: main, subsdk0-9 and sdk at most
static module_t *g_mods; static int g_n;
static uint8_t *g_base;
static uint64_t g_off[PACK_MAX], g_size[PACK_MAX], g_pre[PACK_MAX];   // each module's place: its front patch area at g_off, its base at g_off + g_pre

// The size map_image will give module m and its front patch area (loader.c): with the patch area before the base, the image and -patch_off; else the
// file, which holds the patch area after the image, rounded to 16 KB
static int module_extent(const char *dir, const module_t *m, uint64_t *size, uint64_t *pre) {
    if (m->patch_off < 0) { *size = m->image_size; *pre = (uint64_t)-m->patch_off; return 1; }
    char path[1024]; struct stat st; snprintf(path, sizeof path, "%s/%s.patched.bin", dir, m->name);
    if (stat(path, &st)) return 0;
    *size = ((uint64_t)st.st_size + 0x3FFF) & ~0x3FFFull; *pre = 0; return 1;
}

void dm_modules_reserve(const char *dir, module_t *mods, int n) {
    const char *e = getenv("DM_MODULE_PACK");   // 0: each module where the kernel puts it, as before (an A/B for a crash of this class)
    if (e && !strcmp(e, "0")) { LOG("modules: DM_MODULE_PACK=0, each one where the kernel puts it"); return; }
    if (n <= 0 || n > PACK_MAX) { LOG("modules: %d to map, more than the %d laid out back to back: each one where the kernel puts it", n, PACK_MAX); return; }
    uint64_t span = 0;
    for (int i = 0; i < n; i++) {
        if (!module_extent(dir, &mods[i], &g_size[i], &g_pre[i])) { LOG("modules: no %s/%s.patched.bin to size: each one where the kernel puts it", dir, mods[i].name); return; }
        g_off[i] = span; span += g_pre[i] + g_size[i];
    }
    // Address space only: map_image maps each module over its part with MAP_FIXED, and the parts cover the whole reservation
    void *a = mmap(NULL, span, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (a == MAP_FAILED) { LOG("modules: cannot reserve %#llx bytes for %d modules back to back: each one where the kernel puts it", (unsigned long long)span, n); return; }
    g_base = a; g_mods = mods; g_n = n;
    LOG("modules: %d back to back at %p (%#llx bytes), as Horizon maps them", n, a, (unsigned long long)span);
}

uint8_t *dm_module_slot(module_t *m, uint64_t size, uint64_t pre) {
    if (!g_base || m < g_mods || m >= g_mods + g_n) return NULL;
    long i = m - g_mods;
    if (size != g_size[i] || pre != g_pre[i])
        die("modules: %s is %#llx + %#llx bytes to map, %#llx + %#llx when its place was reserved", m->name, (unsigned long long)size, (unsigned long long)pre,
            (unsigned long long)g_size[i], (unsigned long long)g_pre[i]);
    return g_base + g_off[i] + g_pre[i];
}
