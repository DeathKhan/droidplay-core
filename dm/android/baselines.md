# Android baselines

The first baselines of DroidPlay Core (then droidnative) on more than one device: six Switch 1 games on three Snapdragon devices, run the same way on each with
`scripts/bench_android.py` (the same save, scripted input and measuring window). The first four were measured 2026-11-08 on its `main` at `f4f7abb3`
with PR #1 (the present's vsync sleep outside the submission lock) and PR #3 (the game window keeps the screen on); The Legend of Zelda: Link's Awakening
and Skyward Sword HD the same evening on `main` at `ee18169a`. Each game was packed by `build_android_from_nsp.py`. They are a starting point for fixes,
not a verdict: every known problem below that is not marked fixed is open. The Samsung Galaxy S25 Edge's column and Final Fantasy Resonance's rows were
added 2026-11-10, measured the same way on `main` at `cf42941d` (Brilliant Diamond not yet on the S25 Edge).

## Devices

| Device | SoC | GPU, driver, Vulkan | Android | RAM |
|---|---|---|---|---|
| Ayn Odin 2 | QCS8550 (Snapdragon 8 Gen 2) | Adreno 740, V@0676.32, 1.3.128 | 13 | 12 GB |
| OnePlus Pad 3 | SM8750P (Snapdragon 8 Elite) | Adreno 830, V@0800.74, 1.3.284 | 16 | 12 GB |
| Retroid Pocket 5 | SM8250 (Snapdragon 865) | Adreno 650, V@0746.0, 1.1.128 | 13 | 8 GB |
| Samsung Galaxy S25 Edge | SM8750 (Snapdragon 8 Elite for Galaxy) | Adreno 830, V@0800.40.1, 1.3.284 | 16 | 12 GB |

All four use 4 KB pages. The Odin 2 and the Retroid Pocket 5 have built-in controls; the Pad 3 and the S25 Edge show the on-screen controls over the game.

The RAM column is the device's installed capacity, not a measurement of how much memory a game uses. These baselines report frame rates and frame
times, not per-game peak memory use or a matched memory-use comparison with an emulator. A result on the 8 GB Retroid Pocket 5 does not establish
how the same game runs on a different 8 GB device.

## Results

Frames per second over a 30 s window, then frame time p50 / p95 in ms, from two measured runs after a warm-up run (ranges where they differ). GPU is the
median GPU time per frame. Animal Crossing runs at 30 fps on the Switch too.

| Game, scene | Odin 2 | Pad 3 | Retroid Pocket 5 | Galaxy S25 Edge |
|---|---|---|---|---|
| **Super Mario Odyssey 1.4.1**, Start menu (a fresh save) | 60 · 16.7 / 17.6–18.7 | 60 · 16.6 / 18.0 | 60 · 16.6 / 17.5–17.9 | 60 · 16.7 / 17.6–17.7 · GPU 2.2 |
| Super Mario Odyssey, the opening cutscene (Start pressed on a fresh save) | 59.9 · 16.7 / 18.4 · GPU 5.5 (one run crashed at boot) | 59.9 · 16.7 / 18.9 · GPU 9.3 | **38** · 25.9 / 30.6–31.5 · GPU 12.9 | 60 · 16.6 / 19.3–19.4 · GPU 9.9 |
| Super Mario Odyssey, play (the Cap Kingdom, at the Look Around hint) | 60 · 16.7 / 18.5 · GPU 4.5 (one run, 15 s; it crashed at the hint video before the fix in problem 1) | not run | not run | not run |
| **Pokémon Brilliant Diamond 1.3.0**, language menu (a fresh save) | 60 · 16.7 / 19.2–19.4 | 60 · 16.6–16.7 / 18.5–18.8 | 60 · 16.6–16.7 / 18.1–19.4 (2026-11-10, after problem 2's fix) | not run |
| Pokémon Brilliant Diamond, Eterna City (a save) | 60 · 16.6 / 18.1–18.2 · GPU 8.1 | 58.9–59.7 · 16.6–16.8 / 18.8–19.7 · GPU 10.7 | 54.6–60.0 · 16.6–18.2 / 18.5–21.3 · GPU 13.5–15.0 (2026-11-10, after problem 13's fix; 24.0–24.6 before) | not run |
| **Animal Crossing: New Horizons 3.0.3**, the new game's airport (a fresh save) | 30 · 33.3 / 34.7–35.4 · GPU 5.3 | 30 · 33.3 / 35.7–35.8 · GPU 7.1 | 30 · 33.4 / 36.5–36.7 · GPU 15.8 | 30 · 33.4 / 35.8–35.9 · GPU 15.8 |
| Animal Crossing, outside the player's house (a save) | 30 · 33.3–33.4 / 35.7–35.8 · GPU 9.4–9.7 | 29.9 · 33.3–33.4 / 38.0–38.1 · GPU 15.5–15.9 | 30.0 · 33.3 / 36.3–36.5 · GPU 21.8–22.3 (2026-11-10, after the save fix below) | 30 · 33.2–33.4 / 35.6–36.3 · GPU 22.7–23.1 |
| **Super Smash Bros. Ultimate 13.0.5**, boot with Android's Roboto as the shared font (fixed: problem 3) | **Crashes** 2–4 s in | **Crashes** | **Crashes** | not run (fixed before) |
| Smash, the opening movie (with a hinted shared font, now packed with the game) | 60 · 12.0 / 33.9 | 59.9 · 13.1–13.4 / 34.7–35.1 | 59.7 · 13.5–13.9 / 35.5–36.0 | 60 · 9.8–12.4 / 33.5–33.7 · GPU 1.9–2.1 |
| Smash, a battle (a fuller shared font; stage and CPU fighter vary) | 39.7–59.3 fps | 16.8–29.7 fps, frames up to 215 ms | 13.4–19.3 fps, frames up to 368 ms | 59.9 fps, frames up to 52 ms · GPU 6.3–6.6 |
| **The Legend of Zelda: Link's Awakening 1.1.0**, the opening movie (a fresh save) | 59.8–59.9 · 16.7 / 17.6–17.7 · GPU 2.3–2.4 | 59.7 · 16.7 / 16.8 · GPU 4.5 (the other run crashed at boot) | 57.4–58.9 · 16.7 / 17.0–18.6 · GPU 5.6–6.4 (10 runs, 2026-11-10, after problem 9's fix) | 60 · 16.7 / 16.8 · GPU 4.9 |
| Link's Awakening, the overworld at the Ancient Ruins (a save) | 59.9–60 · 16.7 / 17.2–17.3 · GPU 4.1 | 31.3 · 33.2 / 34.7 · GPU 12.0 (the other run crashed at boot) | 34.7–36.0 · 32.8–33.0 / 34.5 · GPU 12.8 (2026-11-10, after problem 9's fix; the game falls to its 30 fps interval, problem 12) | 38.1–44.6 · 16.8–32.5 / 33.8–34.1 · GPU 13.0–13.7 |
| **The Legend of Zelda: Skyward Sword HD 1.0.1**, the opening movie (a fresh save) | 55.2–55.9 · 18.1–18.3 / 36.6–37.7 · GPU 2.3–2.4 | 59.9–60 · 16.7 / 24.5–25.1 · GPU 2.8 | 41.1–41.4 · 24.4–24.6 / 49.1–49.8 · GPU 3.7 | 60 · 16.6 / 18.0–18.1 · GPU 2.5 |
| Skyward Sword HD, Skyloft's plaza (a save) | 30.0–30.5 · 33.3 / 36.6–46.8 · GPU 4.8–4.9 | 30.0–34.0 · 30.6–32.9 / 44.4–46.5 · GPU 12.6–12.8 | 43.2–43.3 · 17.7 / 34.9–35.2 · GPU 8.5 (2026-11-10, after the save fix below; terrain and sky in colour) | 60 · 16.6–16.7 / 20.6–20.7 · GPU 9.8–9.9 |
| **Final Fantasy Resonance 1.0.1**, the Lancarse logo and the Brightness screen (a fresh save) | 59.6–59.7 · 16.8 / 18.7 · GPU 1.8 | not measured here | not run | 59.6 · 16.8 / 21.3–21.5 · GPU 5.8 |
| Final Fantasy Resonance, the first field (a save made by its `tofield` route) | 42.2–42.6 · 23.3–23.6 / 27.1–27.4 · GPU 12.5–12.7 | not measured here | not run | 30.1–33.3 · 29.7–33.0 / 40.9–42.8 · GPU 20.2–22.7 |
| **The Legend of Zelda: Breath of the Wild 1.9.0**, the Shrine of Resurrection (the `outdoors` save, a 40 s window) | no GPU hang: 30.0–30.5 · 33.0–33.3 / 37.5–38.2 · GPU 13.2–15.3 on main (the frame step tries 60 and fails), **29.9 · 33.2 / 36.2–36.6 · GPU 14.3** with droidplay-core#108 | **30.0 · 33.2–33.3 / 37.9–38.4 · GPU 16.5–16.7** on main with BotW's defaults and the empty slot 0 filled (problem 14, fixed; the picture is still dark, problem 15). Before that fix the GPU was lost at the first frame in the field; with the hanging draw skipped: 20.0 · 48.6–49.2 / 68–71 · GPU 22.3 on main | not run | not run |

Link's Awakening's numbers are for a wrong picture (problem 8): the 3D world never reaches the screen, so the overworld's frame rate leaves out part of
its real work. Skyward Sword HD's Skyloft is drawn with grey terrain (problem 10); on the Galaxy S25 Edge its terrain has its colours.

Time from launch to the first frame, warm: about 1.3 s (Odyssey), 3.2–4.3 s (Brilliant Diamond, a Unity game), 1.5 s (Animal Crossing), 2.0–3.0 s (Smash),
0.4–0.6 s (Link's Awakening), 1.3–2.1 s (Skyward Sword HD). The first launch after an install is slower while the driver builds its pipelines: Brilliant
Diamond's first frame took 33 s on the Pad 3's first launch. On the Galaxy S25 Edge: 1.0–1.2 s (Odyssey), 1.2–1.4 s (Animal Crossing), 1.6–1.9 s
(Smash), 0.4–0.5 s (Link's Awakening), 1.0–1.1 s (Skyward Sword HD), 0.5–0.6 s (Final Fantasy Resonance).

## Switch 2: The Legend of Zelda: Ocarina of Time on a Galaxy Z Fold 8

The first Switch 2 game measured on Android (2026-11-10, #76's branch with #82-#87 and #94, chain 5 shaders), on a fourth device: a Galaxy Z Fold 8 (SM-F971U1, SM8850 / Snapdragon 8 Elite
Gen 5, Adreno 840, Qualcomm's driver 0x8034a013, Vulkan 1.4.295, Android 17, 11.3 GB of RAM), folded, on its cover display. `scripts/bench_android.py`, route `title`,
two runs without a warm-up (the pipeline cache warm from earlier runs):

| Game, scene | Galaxy Z Fold 8 |
|---|---|
| **Ocarina of Time v131072**, the title and its menu (a fresh save) | 29.6-31.1 · 33.2-33.3 / 35.1-48.2 · GPU 26.7 |
| Ocarina of Time, Kokiri Forest (a Mac save, by taps) | about 2 fps (frame about 540 ms, GPU about 165 ms), the forest not drawn (texture pool shaders); runs, at 2.9 GB of graphics memory (killed 15-60 s in before #94) |

The title's GPU frame is nn::ndve's last convolution (36.8 ms in a `prof_now` frame). Kokiri Forest's memory (`DM_VK_MEMREPORT`, #95), its black picture and what is open: the game's note,
[Android](../games/0400486024DEC000/README.md#android).

## Known problems

Numbered in the order they were found, so links to a number stay right:

1. **Fixed: Super Mario Odyssey crashed when a hint video played.** The Snapdragon 8 Gen 2's H.264 decoder gave the host's image reader UBWC-compressed
   NV12, whose colour planes the CPU cannot map, and `media_android.c` read through the null plane (`CRASH sig=11 addr=0`) as play started in the Cap
   Kingdom. The decoder now asks for flexible YUV, which gets linear NV12, and a picture without readable planes is dropped rather than read: [Android:
   H.264 pictures in a buffer the CPU cannot read](../../docs/reference/movies.md#android-h264-pictures-in-a-buffer-the-cpu-cannot-read). Games whose movies are
   VP9 (Smash) take the software decoder and were not affected.
2. **Fixed: Brilliant Diamond crashed on the Retroid Pocket 5** in the Adreno 650 driver's shader compiler (`libllvm-qgl.so+0x2dcef0`, `addr=0x48`) on every
   launch, as it built its first pipelines. The pipeline being built (vertex shader `f8d70797a143b99f`, fragment shader `b2f8ca02b9c1a06c`) is one of the 23 whose
   fragment shader shuffles: Maxwell's `fwidth`, translated to `OpGroupNonUniformShuffle`, in a stage where this GPU runs no subgroup operations
   (`subgroupSupportedStages` is compute alone; the Adreno 740 and 830 run them in fragment shaders too). With that shuffle made a copy of its own value, the
   pipeline built and the crash moved to the next of the 23 (`74f249b01166d890`); with all 23 shuffles out, the game booted. Such modules are now rewritten as
   they are made, the quad's lanes read through fine derivatives: [Subgroup operations rewritten for stages the GPU runs none in](../../docs/reference/rendering.md#subgroup-operations-rewritten-for-stages-the-gpu-runs-none-in).
   The game boots and plays on the phone's own driver (2026-11-10): the language menu at 60 fps, Eterna City at 14.9 fps (problem 13). Mesa's Turnip compiled
   the modules as they were (`DM_VK_DRIVER`, [Another Vulkan driver](README.md#another-vulkan-driver-dm_vk_driver)): Eterna City at 57.7–58.2 fps (GPU 8.8–9.2 ms) on the same build.
3. **Fixed: Smash crashed at boot with Android's stock shared font.** The standard shared font was the device's Roboto, which has no TrueType hinting,
   and Smash's font engine needs a hinted font (`main+0x7caa0`, `addr=0x38`). The packer now copies a hinted TrueType font into each game's
   `font/standard.ttf` (Arial Unicode on a Mac, as the iPhone build does), and Smash boots on all three devices: [Android: the standard shared font must
   be hinted](../../docs/reference/services.md#android-the-standard-shared-font-must-be-hinted).
4. **Fixed: Smash and Odyssey crashed now and then at boot.** Smash crashed at `main+0x385201c` (`addr=0x50`) in a `render dispatch` thread, even with the font, on
   all three devices (about one launch in five; on the Retroid Pocket 5 3 of 12, 2 of 12 with `DM_PREEMPT=1`, 1 of 6 with a test host keeping that sum). Its two
   render dispatch threads sort their pool of command buffers by `nvnCommandBufferGetCommandMemorySize` while the other one records into them, and the host summed
   that size at each call from the bytes used and the write pointer, which a record written in between tore. The test host compared each sum with the size kept for
   the buffer: 7 to 29 torn sums per boot, and in the boot that crashed, the last one was the sort's pivot, read 0x190 bytes short just before the crash; the sort
   then read it right and walked past the vector's start. Fixed with nxnative's 843081d0, the size kept from `AddCommandMemory`: [`GetCommandMemorySize` read while
   another thread records](../../docs/reference/nvn-objects.md#getcommandmemorysize-read-while-another-thread-records); with it, 0 of 18 boots crashed on the
   Retroid Pocket 5, at the same frame times. Odyssey crashed on the Odin 2 three times in about 20 launches, all in its first seconds of loading: twice in its
   loader thread as the mode-select layout loads (`main+0x95d414` with `addr=0`, once `svcBreak`), once at `main+0x954fb8`; never in 18 launches on the other two
   then, but later in about one launch in four on the Retroid Pocket 5. Fixed by the host for every game (first by `core_preempt on` in its profile): [A thread woken by a higher-priority thread of
   its core runs before that one blocks](../../docs/reference/threads.md#a-thread-woken-by-a-higher-priority-thread-of-its-core-runs-before-that-one-blocks).
5. **Fixed: the pipeline cache was saved only when the app paused** (`vkb_pipeline_cache_save` on `APP_CMD_PAUSE`). The list of pipelines to make again at start
   (`pipeline.cache.keys4`) is written as each one is made, but the driver's compiled binaries are in the cache, so after a stop by force (the benchmark, or a player
   swiping the app away) the next start made the whole list again without them, in the background while the game played. The Pad 3's driver builds them about six
   times slower than the Odin 2's: a Smash battle made 682 to 943 pipelines in 11 to 12 s on it (1,147 in 3.1 s on the Odin 2, 368 in 12.5 s on the Retroid Pocket
   5), which is where its battle frames of 100 to 370 ms came from. The cache is now also written during play, after every 64 new pipelines and at most every
   15 s, on a thread of its own: [The pipeline cache written during play](../../docs/reference/rendering.md#the-pipeline-cache-written-during-play). Two Smash
   battles on the Pad 3 afterwards: 32.3 and 34.0 fps, frame p95 37.7 and 38.6 ms (16.8 to 29.7 fps and 73 to 81 ms before; the stage differs from run to run). Pipelines met for the first time are now built in the background, their draws left out for a few frames: from an empty
   cache, two battles at 40.0 and 40.2 fps against 27.9 and 26.5, frame p95 33 and 35 ms against 96 and 85: [Pipelines met for the first time built in the
   background](../../docs/reference/rendering.md#pipelines-met-for-the-first-time-built-in-the-background).
6. **The Retroid Pocket 5 cannot hold 60 fps in Odyssey's opening cutscene** (38 fps, GPU 12.9 ms a frame).
7. **The Pad 3's H.264 decoder takes no input** in `dm/android/tests/t_media` (`[RED] movie: the AVC decoder took no input for 2 s`), before and
   after the fix in problem 1. Not yet tried in a game: an H.264 movie there may show black.
8. **Fixed: Link's Awakening drew wrongly on all three devices.** Its movies and two of its HUD icons came out in ring-banded colours, and in play the 3D
   world was black behind the HUD, without ambient occlusion. Pass dumps (`dump_now`) of the overworld on the Odin 2 showed where:
   - **Fixed: the colours, a raw copy between two formats.** The frame's last target (`s4`, R11G11B10F) holds the right colours; the game then copies it
     into its RGBA8 window texture, and `copy_t2t` in `gfx_vk.c` copied the bits, since its test for a raw copy held for any two formats the replay does
     not convert on the CPU. A format change between float, normalized and sRGB formats is now a blit: [Texture copies that change format convert on
     Vulkan](../../docs/reference/rendering.md#texture-copies-that-change-format-convert-on-vulkan). The movies, the file select and the HUD have their
     colours on the Odin 2.
   - **Fixed: the black world, culled full-screen passes.** The scene was drawn (pass 22), but every pass after it kept its cleared value: the occlusion,
     its blurs and the composite. A dump with each draw's depth and stencil tests showed the blurs and the composite had none, so they were culled: the
     replay inverted the front face by the device's origin and the queue's raster flip together, which cancel for a game that keeps the lower-left origin
     for both. It now inverts by the queue's flip alone, as Metal's draws do: [The front face follows the queue's raster flip alone on
     Vulkan](../../docs/reference/rendering.md#the-front-face-follows-the-queues-raster-flip-alone-on-vulkan). The Ancient Ruins draw at 59.9 fps on the
     Odin 2.
   - **Fixed: no ambient occlusion.** The replay skipped the game's half-size depth-stencil copies (`[RED] [vk] texture copy with a format change or
     scaling between formats 130 and 130: not blittable on this GPU`): no Adreno blits `D32_SFLOAT_S8_UINT`. The occlusion pass tests its stencil (bit 1)
     against the copy, so it wrote nothing and its target kept its cleared 1.0. The copies are now drawn: [Depth-stencil copies a blit cannot make, drawn on
     Vulkan](../../docs/reference/rendering.md#depth-stencil-copies-a-blit-cannot-make-drawn-on-vulkan).
9. **Fixed: Link's Awakening crashed at boot** on every launch on the Retroid Pocket 5 (7 of 7), now and then on the Pad 3 (3 of 8), never on the Odin 2
   (0 of 10): `CRASH sig=11 pc=main+0x1c180` about 0.4 s in, in the constructor of the camera `MainCamera_1P`. It reads an entry of a 0x58-byte table at
   `main+0x1bcc898` by a 16-bit index that is always 0xFFFF there (its caller sets it at `main+0xbdb68` and nothing changes it), so every boot reads 5.5 MB
   past the table, past main's end. Not a race in the game: on a Switch the modules are mapped back to back and that address is subsdk0's code, while the
   host mapped each module where `mmap` put it, and on Android what lay past main depended on what other mappings had filled the gap by then. The modules
   are now mapped back to back: [Modules back to back, as Horizon maps them](../../docs/reference/memory.md#modules-back-to-back-as-horizon-maps-them-android).
   On the Retroid Pocket 5 0 of 11 boots crashed after it, and 6 of 6 on the same build with `DM_MODULE_PACK=0` (the old placement), each at `main+0x214c889`.
10. **Skyward Sword HD draws Skyloft's terrain and sky flat grey** on the Odin 2 and the Pad 3, with no red line. A pass dump on the Pad 3 shows the terrain
    grey in the first pass that draws it, while Link and the bird statue, drawn next with the same ASTC 6×6 textures, have their colours. The terrain's draws
    also sample 64×64 textures the game renders earlier in the frame, which come out black from the game's own constants; whether that is right also needs
    a Mac dump to compare. The opening movie is right.
11. **Fixed for Skyward Sword HD: it played at 30 fps** on the Odin 2 and the Pad 3 (the Mac runs Skyloft at 60): the game drops its present interval
    to 2 when frames come late. Its long frames showed the replay writing a 1920×1080 RGBA8 render target back to guest memory after its pass (`[vk]
    linear render target s19 1920x1080 (format 0x25) written back to guest memory`) and uploading it again (`texture uploads 0 first + 2 re-uploads,
    15.8 MB`), with submit and present taking up to 36 ms while the GPU takes 5 ms on the Odin 2. Its profile now turns on `DM_VK_WB_ABSORB` (59.7 fps
    on the Pad 3, 59.3 on the Odin 2): [A linear render target uploaded again after its own write-back, every
    frame](../../docs/reference/rendering.md#a-linear-render-target-uploaded-again-after-its-own-write-back-every-frame). That Odin 2 figure was one run:
    later runs spread from 33 to 57 fps, the write-back's 8.3 MB staging buffer being made and freed every frame. It is pooled now (60 fps in three runs of
    three on the Odin 2, 60.0 on the Pad 3): [A 1080p linear render target's write-back staged in a buffer of its own every
    frame](../../docs/reference/rendering.md#a-1080p-linear-render-targets-write-back-staged-in-a-buffer-of-its-own-every-frame). Link's Awakening's overworld
    likewise falls to 31 fps on the Pad 3: the game switched interval 175 times in one run there, against 4 on the Odin 2. On the Galaxy S25 Edge, the
    other Adreno 830 here, it plays at 38.1–44.6 fps over the Ancient Ruins window (frame p50 16.8–32.5 ms, GPU 13.0–13.7 ms).
12. **Link's Awakening's overworld plays at 30 fps on the Retroid Pocket 5** (34.7–36.0 fps over the Ancient Ruins window, frame p50 33 ms, p95 34.5 ms, the
    picture right). The game sets its present interval from the vsyncs its last frame took (`main+0xca44`–`0xcb24`, applied at `main+0x1144c18`), and frames
    there take two even at interval 1, so it switches between 1 and 2 about 350 times a run. Per frame in that window: the game's own CPU 5.3 ms, the
    replay's recording of about 616 draws 9.7 ms on the game's thread (`@nvn_ms`), a 9.0 ms `nvnSyncWait` on the GPU (`@sync_ms`), GPU 12.9 ms at 490
    MHz, the floor this device keeps (`min_pwrlevel` 3; 670 MHz at most), 44% busy. The wait and the work after it are in series, so a frame's work takes about
    24 ms, two vsyncs; 60 fps needs the recording cost or the GPU's time to come down by about a third, or the two to overlap. Not worked on yet.
13. **Fixed: Brilliant Diamond's Eterna City ran at 15–25 fps on the Retroid Pocket 5's own driver** (14.9 fps after problem 2's fix, 24.0–24.6 on `main` at `4743c9da`,
    frame p50 40–41 ms, GPU 16.8–17.0 ms), against 57.7–58.2 fps with Turnip on the same build. The completion thread (`vk-done`) took 26 ms a frame landing six
    storage-buffer write-backs of about 494 KB: the driver lists the arena's memory type 4 as cached and coherent (`0xf`), but the CPU maps it uncached (1.3–1.6 GB/s
    with wide loads, about 50 ns a byte in `wb_apply`'s byte loop, against 11–13 GB/s from malloc'd memory). The GPU now copies such write-backs into a cached
    transfer-destination buffer (type 2, `0xb`) at each submission's end and the completion thread reads them from there, 1.7 ms a frame: Eterna City at 54.6–60.0 fps
    (60.0 in three runs of four; frame p50 16.6–18.2 ms, GPU 13.5–15.0 ms) in runs alternating with `main`'s, the picture the same. [Storage buffer write-backs read from
    memory the CPU maps uncached](../../docs/reference/rendering.md#storage-buffer-write-backs-read-from-memory-the-cpu-maps-uncached).

14. **Fixed: Breath of the Wild lost the GPU on the Pad 3** at its first frame in the field. Then `vkQueueSubmit` gave -4 (`VK_ERROR_DEVICE_LOST`), the loading screen stayed up for good, and the GPU time read 0.
    - **What hangs.** One draw a frame hangs the GPU: vertex `2decd3e894b3736a`, fragment `26ae1640f3448ba4`, a fullscreen triangle sampling one 640×360 RGBA8 texture into another, the vertical pass of a blur whose horizontal pass runs fine just before it. The driver recovers each hang about 250 ms in (the chunk's fence signals, its end timestamp never written) until one runs past 3 s. `VK_EXT_device_fault` gives no address and no vendor record.
    - **How it was found.** droidplay-core#90 logs such chunks with their draws, and a `vk_mid` file holding 1 cuts the chunks after every draw.
    - **Still hangs with:**
      - `DM_VK_UBWC=0`, `DM_VK_LAYERED=0`, `DM_VK_TBUF_RANGE=0`, `DM_VK_SAFE=1`, `DM_VK_GEOCACHE=0`, `DM_VK_BIND_FILTER=0`;
      - robustBufferAccess;
      - a full colour write mask (the draw's is 0xe);
      - the zero stand-in texture in place of its source;
      - the horizontal pass's fragment or vertex module in place of its own;
      - `VK_EXT_depth_clip_control` instead of the `.z` vertex variant;
      - its target cleared before the pass;
      - draws that sample an attachment of their own pass skipped;
      - duplicate colour attachments left unused;
      - the earlier pass that binds the target alone (below) with its draws skipped: its clear still begins it.
    - **What differs.** Its pipeline key is the horizontal pass's byte for byte but for the write mask, with the same D16 depth image. Only the target image differs: it is also the sixth colour target of the G-buffer passes.
    - **Not on the Odin 2.** Its Adreno 740 (driver 0x802a4000) runs the same draw without a hang: seven launches of the same save on main 1e4dd802 with droidplay-core#108, nothing skipped, no slow submission reported.
    - **Cause.** An earlier pass of the frame binds that target alone in colour slot 5, with slots 0–4 empty (`VK_ATTACHMENT_UNUSED`). That pass runs; the later one drawing into the target as slot 0 hangs. A scratch target in every empty slot below a used one stops it, and so does one in slot 0 alone. Filling only a G-buffer pass's empty slot 6 below slot 7, or zeroing the write masks of empty slots, doesn't.
    - **Fix.** On Adreno 8xx a render pass with colour slot 0 empty below a used slot gets a scratch RGBA8 target there (`DM_VK_SLOT0_FILL`, [rendering.md](../../docs/reference/rendering.md#a-render-pass-with-colour-slot-0-empty-hangs-a-later-pass-on-the-adreno-830-android)). About one pass a frame gets it in BotW.
    - **Check** (main 4743c9da with the fix, BotW's defaults from droidplay-core#108): 30.0 fps in three runs, no hang report. With `DM_VK_SLOT0_FILL=0` the GPU is lost as before. The picture is still wrong there (problem 15).

15. **Open: Breath of the Wild's Shrine of Resurrection is drawn dark and blurred on the Pad 3**: Link unlit, the walls a blur, where the Odin 2 draws them right.
    - The 1280×720 G-buffer passes bind one image in two colour slots (1 and 2, 3 and 4) and write only the first of each pair (mask 0xf0f0ff).
    - A local test leaving the second slot of each pair unused draws the room as the Odin 2 does. So the Adreno 830 probably stores the unwritten copy over the written one.
    - Not fixed yet.

## Reading the numbers

- **Cold pipelines.** Because of problem 5 (since fixed), every run here began without the driver binaries of the pipelines earlier runs made, most of all on
  the Pad 3.
- **GPU time is at the clock the driver chose.** The Pad 3 shows longer GPU times than the Odin 2 in light scenes although its GPU is faster: it keeps its
  clocks low when the work is light. Compare the GPU column within one device, not across devices.
- **Fixed: the Retroid Pocket 5 couldn't run a route whose game saves early.** The benchmark keeps each run's save in the app's external files. On the
  Retroid Pocket 5 (kernel 4.19) that folder is sdcardfs, which cannot swap two folders atomically (`renameat2` with `RENAME_EXCHANGE` fails with `EINVAL`)
  and has no extended attributes, so every save commit failed: `[RED] save commit: atomic swap failed errno=22`. Animal Crossing saves as soon as the island
  loads, so it stopped with its "an error occurred" screen, which it drew at 30 fps, and the first published version of this page counted that as the island.
  Skyward Sword HD saves while its file list loads, and crashed (`main+0xf2b210`, `addr=0x20`) when it read the save back after the failed commit. A player's
  saves are in the app's internal storage (f2fs), which can swap, so play was never affected. The host now swaps with plain renames where `RENAME_EXCHANGE`
  is refused, and the save's mount puts back a swap that a kill cut short ([services reference](../../docs/reference/services.md#android-a-save-folder-on-sdcardfs)).
  Both routes now run there with every commit made (18 a run in Skyward Sword HD, one in Animal Crossing), and a restart on the committed save loads it.
- **The Pad 3's logcat drops lines from a game that logs fast.** Android 16 rate-limits an app's log, and Link's Awakening logs every file open, so the
  line that marks it ready never reached logcat. The benchmark now takes it from the host's own `log.txt` when logcat lacks it.
- **Smash's battles are a range, not a baseline.** The route picks Random stage and a random CPU fighter (D-pad moves on the stage grid did not land on the
  same stage from run to run), so the load differs per run.
- **Animal Crossing's island has a pinned clock.** `DM_FIXED_TIME` fixes the island at 3 PM on the day after the save, so the light and Isabelle's announcement
  repeat.
- **The Galaxy S25 Edge's runs alternate with another build's.** Its column and the Odin 2's Final Fantasy Resonance rows were measured during PR #76's
  regression check: `main`'s warm-up and first run came after the PR's warm-up and first run, and its second run after the PR's second, each build
  reinstalled from its own APK with its own code and shaders, so a second run starts with the driver's pipeline cache holding both builds' pipelines. On the
  S25 Edge, Link's Awakening's overworld (problem 11) and Final Fantasy Resonance's field vary by a few fps from run to run, and Smash's battle with its stage.
- **The Odin 2's runs after the first may have started from the last run's save** in the boot routes and Odyssey's cutscene (a script bug since fixed). Those
  routes never reach a save point, so the scene is the same.

## Saves

Brilliant Diamond, Animal Crossing and the two Zelda games start from downloaded saves, which this repository does not contain (they are other people's
files):

- Brilliant Diamond: MegaMarble's 1.3.0 "100% Save File" (GameBanana mod 634809, `pkmnbrilliantdiamond_100.7z`, `SaveData.bin` only), in the NSP folder's
  parent as `saves/eterna`.
- Animal Crossing: Anrei's 3.0.1 + DLC save (GBAtemp download 39784), linked once to the host's user (the unlinked-resident prompt: Existing data, then the
  resident, then Link) and kept as `saves/island_linked`. The host's user ID is fixed, so the linked save works on every device and on the Mac.
- Link's Awakening: techhit's save from the Ancient Ruins, after the fifth dungeon (a Tinfoil backup attached to GBAtemp thread 549404,
  `1D6B3FEC0200001087A5E3E5734E0501.zip`; `savedata0.bin` and `savedata3.bin` only), as `saves/ruins`. Made on 1.0.x in 2019; 1.1.0 loads it.
- Skyward Sword HD: soaresden's Checkpoint dump attached to GBAtemp thread 591874 (`20210726-014323 Soaresden.zip`; `wiiking2.sav` and `skip.dat` only), as
  `saves/skyloft`. Its file list has the autosave and three files; the route takes the third, a Hero Mode file at 31 hours that starts at Skyloft's bird
  statue. Made on 1.0.0; 1.0.1 loads it.

- Breath of the Wild: `saves/outdoors`, the save the Mac's smoke route uses (`dm/games/01007EF00011E000/smoke.tsv`). It loads in the Shrine of Resurrection.

Smash's battle and Odyssey's routes start from a fresh save.

## Running them again

```
python3 scripts/bench_android.py <NSP folder> [<NSP folder> ...] -s <serial> [-s <serial> ...] --route <name> --warmup --runs 2
```

The routes are in each game's `dm/games/<id>/bench_android.tsv` ([dm/android/README.md, Benchmarks](README.md#benchmarks)). Smash needs its packed
`font/standard.ttf` (problem 3); a game packed before that change needs packing again.

One run or install at a time on a device: `bench_android.py` locks every device it uses for the whole run, and `build_android_from_nsp.py` locks the phone
while it installs or decodes shaders there, so a second one is refused with who holds the device and since when (`--wait-device <s>` waits instead). Two
benches on one device used to force-stop each other's game and overwrite each other's `env.txt`, voiding both. `python3 dm/android/tools/device_lock.py`
lists who holds which device, and `NXNATIVE_DEVICE_HOLDER=<name>` names a run in that list. A crashed run gives its lock up.
