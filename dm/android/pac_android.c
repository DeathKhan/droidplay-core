// Pointer authentication keys of Switch 2 code on Android (dm/host/loader.c dm_module_text_fixup, before a module's code is sealed).
// Switch 2 SDKs (Ounce) sign return addresses with the B key (pacibsp / autibsp / retab) and frame checksums with the generic key (pacga), and every thread of a Linux
// process shares those keys. nn::os's fibers alone use the A key: FiberImplByHorizon::Initialize signs a new fiber's entry (pacia x16, x9 and pacia x17, x9) and
// DispatchFiberContext checks it (autiasp) when the fiber is first entered. Bionic's pthread_create gives every thread a fresh A key, so a fiber made on one thread and
// entered on another failed its check, which a CPU with FEAT_FPAC turns into SIGILL: Ocarina of Time's nn.npln.GrpcDispatcher died at sdk+0x16d40c a second after the
// first frame on a Galaxy Z Fold 8 (Snapdragon 8 Elite Gen 5). A Mac runs the same code with no fault: an arm64 process there has its authentication keys turned off.
// Every A-key instruction of a guest module takes its B-key twin, so each signature is still made and checked, with a key all threads share. Ocarina of Time v131072 has
// three, all in sdk (two pacia, one autiasp), and main and subsdk0 none. Banana Native (git.eden-emu.dev/moceure/BananaNative) found the cause; no code taken.
#include "dm.h"

// an A-key instruction's B-key twin, or 0 for any other word
static uint32_t b_twin(uint32_t w) {
    switch (w) {
        case 0xD503211F: return 0xD503215F;   // PACIA1716 → PACIB1716
        case 0xD503219F: return 0xD50321DF;   // AUTIA1716 → AUTIB1716
        case 0xD503231F: return 0xD503235F;   // PACIAZ → PACIBZ
        case 0xD503233F: return 0xD503237F;   // PACIASP → PACIBSP
        case 0xD503239F: return 0xD50323DF;   // AUTIAZ → AUTIBZ
        case 0xD50323BF: return 0xD50323FF;   // AUTIASP → AUTIBSP
        case 0xD65F0BFF: return 0xD65F0FFF;   // RETAA → RETAB
    }
    if ((w & 0xFFFFCC00) == 0xDAC10000) return w | 0x400;   // PACIA / PACIZA / AUTIA / AUTIZA (bit 12: auth, bit 13: zero modifier, bit 11 clear: an instruction key, bit 10: key B)
    if ((w & 0xFEDFFC00) == 0xD61F0800) return w | 0x400;   // BRAA / BRAAZ / BLRAA / BLRAAZ (bit 24: a register modifier, bit 21: with link) → their B-key forms
    return 0;
}

void dm_module_text_fixup(module_t *m) {
    if (!m->base || !m->text_end) return;
    uint32_t *w = (uint32_t *)m->base; size_t n = (size_t)(m->text_end / 4), cnt = 0;
    for (size_t i = 0; i < n; i++) { uint32_t t = b_twin(w[i]); if (t) { w[i] = t; cnt++; } }
    if (cnt) LOG("%s: %zu A-key pointer authentication instruction(s) switched to the B key (Android gives each thread its own A key)", m->name, cnt);
}
