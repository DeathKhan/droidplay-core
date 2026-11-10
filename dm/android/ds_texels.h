// A depth / stencil texture read back by the CPU (gfx_vk.c mtl_read_texels, copy_t2b): Vulkan copies an image's depth and stencil aspects to a buffer one aspect at a time,
// each tightly packed whatever the combined format: depth 2 bytes a texel (D16_UNORM) or 4 (D32_SFLOAT; D24_UNORM_S8_UINT's depth in the low 24 bits of 4, the top 8
// undefined), stencil 1 byte. NVN's texels are rebuilt from those planes here, in the layouts its formats have in memory (Maxwell's):
//   STENCIL8 (0x31)            the stencil byte
//   DEPTH16 (0x32)             16-bit unorm
//   DEPTH24 (0x33)             24-bit unorm in the low 24 bits of 4 bytes, the top 8 zero (kept as D32_SFLOAT here: nvn_fmt_mtl.c)
//   DEPTH32F (0x34)            the float
//   DEPTH24_STENCIL8 (0x35)    24-bit unorm depth in the low 24 bits, the stencil in the top 8 (kept as D32_SFLOAT_S8_UINT, or D24_UNORM_S8_UINT)
//   DEPTH32F_STENCIL8 (0x36)   the float, the stencil byte, 3 zero bytes: 8 bytes (Maxwell's ZF32_X24S8; kept as D32_SFLOAT_S8_UINT, or D24_UNORM_S8_UINT)
// Self-contained (no Vulkan headers) so that dm/tools/tests/test_ds_texels.py builds it alone on the PC.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { DS_NONE, DS_D16, DS_D32F, DS_D24 };   // how the image's depth aspect is copied out: none, 2-byte unorm, 4-byte float, 24-bit unorm in 4 bytes

static inline size_t ds_depth_bytes(int dkind) { return dkind == DS_D16 ? 2 : dkind ? 4 : 0; }
static inline size_t ds_texel_bytes(int nvn_format) {   // a texel of an NVN depth / stencil format in memory; 0: not one
    return nvn_format == 0x31 ? 1 : nvn_format == 0x32 ? 2 : nvn_format == 0x36 ? 8 : nvn_format >= 0x33 && nvn_format <= 0x35 ? 4 : 0;
}

static inline uint32_t ds_unorm24(float x) {   // a depth in [0, 1] as 24-bit unorm (NaN and below 0: 0)
    if (!(x > 0)) return 0;
    if (x >= 1) return 0xffffffu;
    return (uint32_t)(x * 16777215.0f + 0.5f);
}

// n texels of one row: dep (dkind's bytes a texel, NULL: the image has no depth aspect, read 0) and sten (1 byte a texel, NULL: no stencil, read 0) into out, in
// nvn_format's layout. → the bytes written (n × the format's texel size), 0 for a format that is not depth or stencil
static size_t ds_texels(int nvn_format, int dkind, const uint8_t *dep, const uint8_t *sten, size_t n, uint8_t *out) {
    size_t db = ds_depth_bytes(dkind), tb = ds_texel_bytes(nvn_format);
    if (!tb) return 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t raw = 0; if (dep && db) memcpy(&raw, dep + db * i, db);
        float f = 0; uint32_t u24 = 0;   // the depth as a float and as 24-bit unorm
        if (dep && dkind == DS_D32F) { memcpy(&f, &raw, 4); u24 = ds_unorm24(f); }
        else if (dep && dkind == DS_D24) { u24 = raw & 0xffffffu; f = (float)u24 / 16777215.0f; }
        uint8_t st = sten ? sten[i] : 0; uint8_t *o = out + tb * i;
        switch (nvn_format) {
        case 0x31: o[0] = st; break;
        case 0x32: { uint16_t d = (uint16_t)(dkind == DS_D16 ? raw : 0); memcpy(o, &d, 2); break; }
        case 0x33: memcpy(o, &u24, 4); break;
        case 0x34: { uint32_t b; memcpy(&b, &f, 4); memcpy(o, &b, 4); break; }
        case 0x35: { uint32_t w = u24 | (uint32_t)st << 24; memcpy(o, &w, 4); break; }
        case 0x36: memcpy(o, &f, 4); o[4] = st; o[5] = o[6] = o[7] = 0; break;
        }
    }
    return tb * n;
}
