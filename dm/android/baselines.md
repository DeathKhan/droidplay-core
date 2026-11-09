# Android baselines

The first baselines of droidnative on more than one device: six Switch 1 games on three Snapdragon devices, run the same way on each with
`scripts/bench_android.py` (the same save, scripted input and measuring window). The first four were measured 2026-11-08 on droidnative `main` at `f4f7abb3`
with PR #1 (the present's vsync sleep outside the submission lock) and PR #3 (the game window keeps the screen on); The Legend of Zelda: Link's Awakening
and Skyward Sword HD the same evening on `main` at `ee18169a`. Each game was packed by `build_android_from_nsp.py`. They are a starting point for fixes,
not a verdict: every known problem below that is not marked fixed is open.

## Devices

| Device | SoC | GPU, driver, Vulkan | Android | RAM |
|---|---|---|---|---|
| Ayn Odin 2 | QCS8550 (Snapdragon 8 Gen 2) | Adreno 740, V@0676.32, 1.3.128 | 13 | 12 GB |
| OnePlus Pad 3 | SM8750P (Snapdragon 8 Elite) | Adreno 830, V@0800.74, 1.3.284 | 16 | 12 GB |
| Retroid Pocket 5 | SM8250 (Snapdragon 865) | Adreno 650, V@0746.0, 1.1.128 | 13 | 8 GB |

All three use 4 KB pages. The Odin 2 and the Retroid Pocket 5 have built-in controls; the Pad 3 shows the on-screen controls over the game.

## Results

Frames per second over a 30 s window, then frame time p50 / p95 in ms, from two measured runs after a warm-up run (ranges where they differ). GPU is the
median GPU time per frame. Animal Crossing runs at 30 fps on the Switch too.

| Game, scene | Odin 2 | Pad 3 | Retroid Pocket 5 |
|---|---|---|---|
| **Super Mario Odyssey 1.4.1**, Start menu (a fresh save) | 60 · 16.7 / 17.6–18.7 | 60 · 16.6 / 18.0 | 60 · 16.6 / 17.5–17.9 |
| Super Mario Odyssey, the opening cutscene (Start pressed on a fresh save) | 59.9 · 16.7 / 18.4 · GPU 5.5 (one run crashed at boot) | 59.9 · 16.7 / 18.9 · GPU 9.3 | **38** · 25.9 / 30.6–31.5 · GPU 12.9 |
| Super Mario Odyssey, play (the Cap Kingdom, at the Look Around hint) | 60 · 16.7 / 18.5 · GPU 4.5 (one run, 15 s; it crashed at the hint video before the fix in problem 1) | not run | not run |
| **Pokémon Brilliant Diamond 1.3.0**, language menu (a fresh save) | 60 · 16.7 / 19.2–19.4 | 60 · 16.6–16.7 / 18.5–18.8 | **Crashes** at 2.6 s |
| Pokémon Brilliant Diamond, Eterna City (a save) | 60 · 16.6 / 18.1–18.2 · GPU 8.1 | 58.9–59.7 · 16.6–16.8 / 18.8–19.7 · GPU 10.7 | **Crashes** at 2.6 s |
| **Animal Crossing: New Horizons 3.0.3**, the new game's airport (a fresh save) | 30 · 33.3 / 34.7–35.4 · GPU 5.3 | 30 · 33.3 / 35.7–35.8 · GPU 7.1 | 30 · 33.4 / 36.5–36.7 · GPU 15.8 |
| Animal Crossing, outside the player's house (a save) | 30 · 33.3–33.4 / 35.7–35.8 · GPU 9.4–9.7 | 29.9 · 33.3–33.4 / 38.0–38.1 · GPU 15.5–15.9 | Not measured: the game stops at its first save (see below) |
| **Super Smash Bros. Ultimate 13.0.5**, boot with Android's Roboto as the shared font (fixed: problem 3) | **Crashes** 2–4 s in | **Crashes** | **Crashes** |
| Smash, the opening movie (with a hinted shared font, now packed with the game) | 60 · 12.0 / 33.9 | 59.9 · 13.1–13.4 / 34.7–35.1 | 59.7 · 13.5–13.9 / 35.5–36.0 |
| Smash, a battle (a fuller shared font; stage and CPU fighter vary) | 39.7–59.3 fps | 16.8–29.7 fps, frames up to 215 ms | 13.4–19.3 fps, frames up to 368 ms |
| **The Legend of Zelda: Link's Awakening 1.1.0**, the opening movie (a fresh save) | 59.8–59.9 · 16.7 / 17.6–17.7 · GPU 2.3–2.4 | 59.7 · 16.7 / 16.8 · GPU 4.5 (the other run crashed at boot) | **Crashes** at boot (problem 9) |
| Link's Awakening, the overworld at the Ancient Ruins (a save) | 59.9–60 · 16.7 / 17.2–17.3 · GPU 4.1 | 31.3 · 33.2 / 34.7 · GPU 12.0 (the other run crashed at boot) | **Crashes** at boot |
| **The Legend of Zelda: Skyward Sword HD 1.0.1**, the opening movie (a fresh save) | 55.2–55.9 · 18.1–18.3 / 36.6–37.7 · GPU 2.3–2.4 | 59.9–60 · 16.7 / 24.5–25.1 · GPU 2.8 | 41.1–41.4 · 24.4–24.6 / 49.1–49.8 · GPU 3.7 |
| Skyward Sword HD, Skyloft's plaza (a save) | 30.0–30.5 · 33.3 / 36.6–46.8 · GPU 4.8–4.9 | 30.0–34.0 · 30.6–32.9 / 44.4–46.5 · GPU 12.6–12.8 | Not measured: the game stops at its first save (see below) |

Link's Awakening's numbers are for a wrong picture (problem 8): the 3D world never reaches the screen, so the overworld's frame rate leaves out part of
its real work. Skyward Sword HD's Skyloft is drawn with grey terrain (problem 10).

Time from launch to the first frame, warm: about 1.3 s (Odyssey), 3.2–4.3 s (Brilliant Diamond, a Unity game), 1.5 s (Animal Crossing), 2.0–3.0 s (Smash),
0.4–0.6 s (Link's Awakening), 1.3–2.1 s (Skyward Sword HD). The first launch after an install is slower while the driver builds its pipelines: Brilliant
Diamond's first frame took 33 s on the Pad 3's first launch.

## Known problems

Numbered in the order they were found, so links to a number stay right:

1. **Fixed: Super Mario Odyssey crashed when a hint video played.** The Snapdragon 8 Gen 2's H.264 decoder gave the host's image reader UBWC-compressed
   NV12, whose colour planes the CPU cannot map, and `media_android.c` read through the null plane (`CRASH sig=11 addr=0`) as play started in the Cap
   Kingdom. The decoder now asks for flexible YUV, which gets linear NV12, and a picture without readable planes is dropped rather than read: [Android:
   H.264 pictures in a buffer the CPU cannot read](../../docs/reference/movies.md#android-h264-pictures-in-a-buffer-the-cpu-cannot-read). Games whose movies are
   VP9 (Smash) take the software decoder and were not affected.
2. **Brilliant Diamond crashes on the Retroid Pocket 5** in the Adreno 650 driver's shader compiler (`libllvm-qgl.so`, `addr=0x48`), while it builds its first
   pipelines. The newer drivers of the other two take the same SPIR-V. Which shader it is is not known yet: nothing logs each pipeline as it is built.
3. **Fixed: Smash crashed at boot with Android's stock shared font.** The standard shared font was the device's Roboto, which has no TrueType hinting,
   and Smash's font engine needs a hinted font (`main+0x7caa0`, `addr=0x38`). The packer now copies a hinted TrueType font into each game's
   `font/standard.ttf` (Arial Unicode on a Mac, as the iPhone build does), and Smash boots on all three devices: [Android: the standard shared font must
   be hinted](../../docs/reference/services.md#android-the-standard-shared-font-must-be-hinted).
4. **Smash and Super Mario Odyssey crash now and then at boot.** Smash at `main+0x385201c` (`addr=0x50`), even with the font, on all three devices (about one
   launch in six). Odyssey only on the Odin 2, three times in about 20 launches, all in its first seconds of loading: twice in its loader thread as the mode-select
   layout loads (`main+0x95d414` with `addr=0`, once `svcBreak`), once at `main+0x954fb8`; never in 18 launches on the other two.
5. **The pipeline cache is saved only when the app pauses** (`vkb_pipeline_cache_save` on `APP_CMD_PAUSE`). An app stopped by force (the benchmark, or a player
   swiping it away) loses it, so every run builds its pipelines from scratch. The Pad 3's driver builds them about six times slower than the Odin 2's: a Smash
   battle made 682 to 943 pipelines in 11 to 12 s on it (1,147 in 3.1 s on the Odin 2, 368 in 12.5 s on the Retroid Pocket 5), which is where its battle
   frames of 100 to 370 ms come from.
6. **The Retroid Pocket 5 cannot hold 60 fps in Odyssey's opening cutscene** (38 fps, GPU 12.9 ms a frame).
7. **The Pad 3's H.264 decoder takes no input** in `dm/android/tests/t_media` (`[RED] movie: the AVC decoder took no input for 2 s`), before and
   after the fix in problem 1. Not yet tried in a game: an H.264 movie there may show black.
8. **Link's Awakening draws wrongly on all three devices.** Its movies and two of its HUD icons come out in ring-banded colours, and in play the 3D world
   is black behind the HUD. A pass dump (`dump_now`) of the overworld on the Odin 2 shows two separate faults:
   - **The colours: a raw copy between two formats.** The frame's last target (`s4`, R11G11B10F) holds the right colours; the game then copies it into
     its RGBA8 window texture. `copy_t2t` in `gfx_vk.c` takes the raw `vkCmdCopyImage` path whenever `s->conv == d->conv && s->px == d->px`, which
     holds for any two formats the replay does not convert on the CPU (both are 0), so the packed-float bits land in the window as bytes. A raw copy
     should be kept for the same format (or the same CPU conversion), and a format change should be a blit.
   - **The black world: an occlusion of 0.** The scene is drawn (pass 22), but the pass that multiplies it by the ambient occlusion (pass 30, fragment
     shader `933b6380fb6fc0f0`: occlusion × occlusion × scene) gets 0. The occlusion pass writes 1.0 everywhere (pass 25), and its five-tap blur
     (pass 26: vertex shader `6e91ec1dc9c02f57`, fragment shader `359bd17463237937`) writes 0 from it. The blur's weights come from the vertex
     constant buffer in slot 6 (the dump reads 0.070, 0.316, 0.227, 0.316 and 0.070 from guest memory) and reach the fragment shader in the varyings'
     `z`: either the GPU sees other weights than the dump, or it reads the occlusion target as 0. Not pinned down yet; a Mac dump of the same frame is
     the reference.
   - The replay also skips the game's half-size depth-stencil copies: `[RED] [vk] texture copy with a format change or scaling between formats 130 and
     130: not blittable on this GPU` (`D32_SFLOAT_S8_UINT`). The Mac draws them as a pass (`copy_t2t_draw`); the occlusion pass's stencil mask and the
     depth of field read them.
9. **Link's Awakening crashes at boot** on every launch on the Retroid Pocket 5 (7 of 7), now and then on the Pad 3 (3 of 8), never on the Odin 2
   (0 of 10): `CRASH sig=11 pc=main+0x1c180` about 0.1 s in, in the constructor of the camera `MainCamera_1P`. It reads an entry of a 0x58-byte table at
   `main+0x1bcc898` by a 16-bit index that is still 0xFFFF, the default its caller sets at `main+0xbdb68` until a lookup fills it, so the read lands
   5.5 MB past the table. Whatever that lookup finds is in place on the Odin 2 by then and often not on the other two: a race at startup, not found yet.
10. **Skyward Sword HD draws Skyloft's terrain and sky flat grey** on the Odin 2 and the Pad 3, with no red line. A pass dump on the Pad 3 shows the terrain
    grey in the first pass that draws it, while Link and the bird statue, drawn next with the same ASTC 6×6 textures, have their colours. The terrain's draws
    also sample 64×64 textures the game renders earlier in the frame, which come out black from the game's own constants; whether that is right also needs
    a Mac dump to compare. The opening movie is right.
11. **Fixed for Skyward Sword HD: it played at 30 fps** on the Odin 2 and the Pad 3 (the Mac runs Skyloft at 60): the game drops its present interval
    to 2 when frames come late. Its long frames showed the replay writing a 1920×1080 RGBA8 render target back to guest memory after its pass (`[vk]
    linear render target s19 1920x1080 (format 0x25) written back to guest memory`) and uploading it again (`texture uploads 0 first + 2 re-uploads,
    15.8 MB`), with submit and present taking up to 36 ms while the GPU takes 5 ms on the Odin 2. Its profile now turns on `DM_VK_WB_ABSORB` (59.7 fps
    on the Pad 3, 59.3 on the Odin 2): [A linear render target uploaded again after its own write-back, every
    frame](../../docs/reference/rendering.md#a-linear-render-target-uploaded-again-after-its-own-write-back-every-frame). Link's Awakening's overworld
    likewise falls to 31 fps on the Pad 3: the game switched interval 175 times in one run there, against 4 on the Odin 2.

## Reading the numbers

- **Cold pipelines.** Because of problem 5, every run here built its pipelines from scratch. A run whose cache survives should stutter less, most of all on the
  Pad 3.
- **GPU time is at the clock the driver chose.** The Pad 3 shows longer GPU times than the Odin 2 in light scenes although its GPU is faster: it keeps its
  clocks low when the work is light. Compare the GPU column within one device, not across devices.
- **The Retroid Pocket 5 can't run a route whose game saves early.** The benchmark keeps each run's save in the app's external files, and on the Retroid
  Pocket 5 that storage cannot swap two folders atomically (`renameat2` with `RENAME_EXCHANGE` fails with `EINVAL`), which the host's save commit needs:
  `[RED] save commit: atomic swap failed errno=22`. Animal Crossing saves as soon as the island loads, so it stopped with its "an error occurred" screen,
  which it drew at 30 fps, and the first published version of this page counted that as the island. A player's saves are in the app's internal storage,
  which can swap, so play is not affected; only this route on this device is. Brilliant Diamond crashes there earlier, and Smash's saves did not fail.
  Skyward Sword HD saves while its file list loads, and crashes (`main+0xf2b210`, `addr=0x20`) when it reads the save back after the failed commit, so its
  Skyloft route is not measured there either; its boot route survives the failed commit.
- **The Pad 3's logcat drops lines from a game that logs fast.** Android 16 rate-limits an app's log, and Link's Awakening logs every file open, so the
  line that marks it ready never reached logcat. The benchmark now takes it from the host's own `log.txt` when logcat lacks it.
- **Smash's battles are a range, not a baseline.** The route picks Random stage and a random CPU fighter (D-pad moves on the stage grid did not land on the
  same stage from run to run), so the load differs per run.
- **Animal Crossing's island has a pinned clock.** `DM_FIXED_TIME` fixes the island at 3 PM on the day after the save, so the light and Isabelle's announcement
  repeat.
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

Smash's battle and Odyssey's routes start from a fresh save.

## Running them again

```
python3 scripts/bench_android.py <NSP folder> [<NSP folder> ...] -s <serial> [-s <serial> ...] --route <name> --warmup --runs 2
```

The routes are in each game's `dm/games/<id>/bench_android.tsv` ([dm/android/README.md, Benchmarks](README.md#benchmarks)). Smash needs its packed
`font/standard.ttf` (problem 3); a game packed before that change needs packing again.
