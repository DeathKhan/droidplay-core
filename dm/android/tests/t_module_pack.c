// The profile's modules map back to back through loader.c's jit_slot, as Horizon maps them (module_pack.c): each one's base where the last one's image
// ends, a front patch area in between, NROs left out. Then a read past the first module's end lands in the next module's first byte, which Link's
// Awakening's camera constructor needs (main+0x1c180 reads entry 0xFFFF of a table in main, 5.5 MB past its end) and which unmapped memory failed
#include "../../host/loader.c"   // with ../module_pack.c beside it, whose strong hooks replace loader.c's weak ones
#include <assert.h>

void mtl_present_screen(uint32_t w, uint32_t h) { (void)w; (void)h; }   // loader.c: a screen taller than 16:9; no renderer here
struct dm_flags g_flag;
uint64_t now_ns(void) { return 0; }
void gate_register(uint64_t a, const char *k, int n, uint64_t o) { (void)a; (void)k; (void)n; (void)o; }
void hlecount_seal(void) {}
void *hlecount_wrap(const char *kind, const char *name, void *fn) { (void)kind; (void)name; return fn; }
uint64_t resolve_import(module_t *m, const char *name) { (void)m; (void)name; return 0; }
int hle_implements(const char *name) { (void)name; return 0; }
int svc_nro_deactivate(module_t *m) { m->loaded = 0; return 1; }
void dm_thread_service_connect(void) {}

static void file(const char *dir, const char *name, size_t len) {
    char p[1100]; snprintf(p, sizeof p, "%s/%s.patched.bin", dir, name);
    FILE *f = fopen(p, "wb"); assert(f);
    for (size_t i = 0; i < len; i++) fputc(0, f);
    fclose(f);
}

int main(void) {
    const char *tmp = getenv("TMPDIR"); char dir[512];
    snprintf(dir, sizeof dir, "%s/t_module_pack.XXXXXX", tmp && *tmp ? tmp : "/data/local/tmp");
    assert(mkdtemp(dir));
    // main and subsdk0 with the patch area after the image (in the file; sizes round up to 16 KB), sdk with it before the base (image_size and -patch_off)
    file(dir, "main", 0x9100); file(dir, "subsdk0", 0x4000);
    module_t mods[4] = {{.name = "main", .patch_off = 0x8000}, {.name = "subsdk0", .patch_off = 0x3000},
                        {.name = "sdk", .patch_off = -0x8000, .image_size = 0x10000}, {.name = "nro", .nro = 1, .patch_off = 0x1000}};
    dm_modules_reserve(dir, mods, 3);
    uint8_t *b0 = jit_slot(&mods[0], 0xC000, 0), *b1 = jit_slot(&mods[1], 0x4000, 0), *b2 = jit_slot(&mods[2], 0x10000, 0x8000);
    assert(b0 && b1 == b0 + 0xC000 && b2 == b1 + 0x4000 + 0x8000);
    assert(!jit_slot(&mods[3], 0x4000, 0));   // an NRO is mapped where nn::ro likes, as before
    // Mapped as map_image maps them (MAP_FIXED over the reservation), the byte after main is subsdk0's first
    assert(mmap(b0, 0xC000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == b0);
    assert(mmap(b1, 0x4000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == b1);
    assert(mmap(b2 - 0x8000, 0x18000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == b2 - 0x8000);
    b1[0] = 0x5a; b2[-0x8000] = 0xa5;
    assert(((volatile uint8_t *)b0)[0xC000] == 0x5a);
    assert(((volatile uint8_t *)b1)[0x4000] == 0xa5);   // past subsdk0: sdk's front patch area
    char p[1100];
    snprintf(p, sizeof p, "%s/main.patched.bin", dir); unlink(p);
    snprintf(p, sizeof p, "%s/subsdk0.patched.bin", dir); unlink(p);
    rmdir(dir);
    printf("t_module_pack: ok\n");
    return 0;
}
