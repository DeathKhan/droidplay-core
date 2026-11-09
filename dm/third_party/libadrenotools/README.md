# libadrenotools

From [bylaws/libadrenotools](https://github.com/bylaws/libadrenotools) at commit `8fae8ce` (2024-09-10), with its submodule `lib/linkernsbypass`
([bylaws/liblinkernsbypass](https://github.com/bylaws/liblinkernsbypass)) at `aa39758` (2024-09-07). BSD 2-Clause: `LICENSE`, and `lib/linkernsbypass/LICENSE` for the submodule.

Only what loading a custom Vulkan driver takes is kept, unmodified: `src/driver.cpp` with `include/adrenotools/`, the hook implementation and the `android_dlopen_ext` hook
(`src/hook/hook_impl.*`, `hook_impl_params.h`, `kgsl.h`, `main_hook.c`) and liblinkernsbypass's sources. Left out: the BC enabler (`bcenabler.*`, `gen/`), the file-redirect
and GSL-allocation hooks (`file_redirect_hook.c`, `gsl_alloc_hook.c`, which only `ADRENOTOOLS_DRIVER_FILE_REDIRECT` and `ADRENOTOOLS_DRIVER_GPU_MAPPING_IMPORT` load), `tools/`
and the CMake files.

`dm/android/CMakeLists.txt` builds three libraries from it, which every APK carries in `lib/arm64-v8a/`: `libadrenotools.so` (liblinkernsbypass inside), `libhook_impl.so` and
`libmain_hook.so`. The host opens `libadrenotools.so` only when `DM_VK_DRIVER` names a driver (`dm/android/vk_driver.c`): liblinkernsbypass's constructor patches the
linker's `dlopen` page as it loads, which a run on the system's driver should not do.
