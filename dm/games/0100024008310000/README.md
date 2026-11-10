# Street Fighter 30th Anniversary Collection (`0100024008310000`)

Update 1.0.3 (v196608) over the cartridge's base game, SDK **3.7.0** (`nnSdk-3_7_0`, `NintendoSDK_OpenGL-3_7_0`, `NintendoSDK_NVN-3_7_0`, with Pia 5.9.0 and NEX 4.3.1 for online play). It's Digital Eclipse's **Eclipse engine** (`EclipseApplication`, `EclipseMainMenuViewController`), with the twelve arcade games emulated. The RomFS is 18 `.mbundle` bundles (4.97 GB) and a `.plist` under `Bundle/`.

It renders with **OpenGL**, not NVN. `main` imports nine EGL functions (`eglChooseConfig`, `eglCreateContext`, `eglCreateWindowSurface`, `eglDestroySurface`, `eglGetDisplay`, `eglInitialize`, `eglMakeCurrent`, `eglSwapBuffers`, `eglTerminate`) and 56 GL functions directly, with no `eglGetProcAddress` and no NVN function. The host's GL layer answers all of them, so NVIDIA's GL driver (`subsdk0`, `NintendoSDK_OpenGL`) is never loaded. The game asks for an `OpenGL 2.0` context.

- **On the Mac:** plays at 60 fps with sound. It runs from the photosensitivity warning through the title screen, the main menu and the Arcade menu, into a fight in Street Fighter (1987).
- **On Android:** it stops at its first EGL call, because the Android host has no OpenGL layer. See [Android](#android).

## What it needs

| Setting | Where | Why |
|---|---|---|
| `modules main sdk` | [`game.tsv`](game.tsv) | `main` takes nothing from NVIDIA's GL driver: the GL layer answers its EGL and GL imports, so `subsdk0` stays unloaded |
| `sys_resource 0x10000000`, `mem_reserve 0x140000000`, `mem_stack 0x10000000`, `app_pool 0xCD500000` | [`game.tsv`](game.tsv) | the NPDM declares 256 MB of system resource (virtual address memory), so the guest block is 5 GB with a large alias region |
| `mem_alias 0x50000000` | [`game.tsv`](game.tsv) | `nninitStartup` asks for a 3.14 GB heap (`0xc9400000`), nearly the whole pool. The NSP script's heap check on the Mac sized the heap region for it (3.25 GB); with the default region the request fails (`SetHeapSize OOM: req 0xc9400000 > cap 0x8000000`) |
| [`gl_capture.tsv`](gl_capture.tsv) | profile | the four shaders the engine puts together at run time, which no game file holds; the NSP script captures them on the Mac. See [Shaders](#shaders) |
| `shader_groups /Bundle/=bundle` | [`game.tsv`](game.tsv) | no NVN shaders: the profile's NVN tables stay empty, and the GL library comes from `gl_shaders.py` |
| `zip_store .mbundle:.metallib` | [`game.tsv`](game.tsv) | the bundles are already compressed: deflate leaves 1 MB samples of the RomFS at 98–100 % |

The profile is the provisional one `scripts/build_ipa_from_nsp.py` made (the survey's keys, the heap check's `mem_alias`, the GL capture), with its comments written by hand.

## Rendering (OpenGL)

- **Light use of GL:** the GL layer makes three pipelines in all. A fight draws 13 times a frame in two passes, with one texture upload a frame (the emulated arcade screen). The menus draw 8 to 50 times a frame.
- **Unimplemented:** `glLineWidth` only (logged once, at boot). Nothing else drawn on screen was missing in the scenes checked.

## Shaders

- **Current:** four GLSL ES 1.00 sources (`#version 100`, two vertex and two pixel, 895 bytes in all) in [`gl_capture.tsv`](gl_capture.tsv). The engine writes them at run time; they were captured on the Mac with `DM_GL_CAPTURE` while booting to the title screen, and translated by `gl_shaders.py`. The Arcade menu and a fight used no others.

## Audio

- The game opens one stereo `AudioOut` at boot. A `DM_AUDIO_DUMP` of a 97 s Mac run holds sound. In 1 s samples taken every 10 s, three are silent (the first, at boot) and the others reach an RMS of about 3,300 and peaks of 13,900. The sound was judged from the dump; nobody listened to it.

## Mac

- **Checked on an M5 Max** with the host's control file (`DM_CTL`): scripted A presses and screenshots, headless, no hand play.
- **Frame rate:** 60.0 fps from boot. The GL layer's records came every 300 frames at 5.00 s intervals, through the menus and the fight.
- **Route:** about 30 s from launch to the title screen, then A presses through the main menu and the Arcade menu (Street Fighter is the first tile) into a fight. With no input, the CPU wins and the game returns to its attract mode.

## Android

- **Tested 2026-11-10 on a Retroid Pocket 5** (Adreno 650, Vulkan 1.1.128, Android 13), DroidPlay Core 0.7.0-1-g4743c9da, from the provisional profile.
- **What happens:** the game process dies 0.1 s into `nnMain`, and the launcher comes back to the game's page with no message:

  ```
  [dm] nv::InitializeGraphics: pool 8.00 MB, recorded (the host's graphics layer allocates its own)
  [dm] UNIMPL eglGetDisplay (x0=0 x1=… x2=0x62)
  [dm] Break#1 reason=0 Result=0 svc at sdk+0xa9dec
  [dm] bt: sdk+0x43278 main+0x7282c main+0x72bd0 …
  ```

  Nothing is drawn, no sound plays, and there are no frame times.
- **Cause:** the Android host replays NVN only (`dm/android/gfx_vk.c`). The OpenGL layer is the Mac's, on Metal (`dm/gfx/gl/hle_gl.m`), and isn't built for Android. So `eglGetDisplay` goes to an unimplemented stub, which returns 0 (`EGL_NO_DISPLAY`), and the game aborts.
- **What playing it on Android would take:** an OpenGL path in the Android host. This game needs little of one: 9 EGL and 56 GL functions, three pipelines, about 13 draws and one texture upload a frame, and four GLSL ES 1.00 shaders, a dialect Android's own GLES driver compiles as it is.
- **Heap region:** that run's log also had `SetHeapSize OOM: req 0xc9400000 > cap 0x8000000` before the EGL call. `build_android_from_nsp.py` doesn't run `build_ipa_from_nsp.py`'s heap check for a provisional profile. This profile's `mem_alias` is that check's result; it hasn't been run on Android, since the game stops at EGL anyway.

## Open

- **Only the first game was tried:** Street Fighter (1987) on the Mac. The other eleven, the museum, the options (screen filters, borders), training mode and online play are unchecked, and a filter may use shaders the capture lacks.
- **Hand play on the Mac** (a window and a controller) wasn't tried.
- **Android needs an OpenGL layer**, as above.
