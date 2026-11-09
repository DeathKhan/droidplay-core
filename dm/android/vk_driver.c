// The Vulkan driver behind gfx_vk.h's vk* pointers.
//   By default the system's: libvulkan.so, opened when the replay makes its device (vkb), as linking it did before.
//   DM_VK_DRIVER=<file> (env.txt) runs another driver, such as a build of Mesa's Turnip for Adreno GPUs, as players of Switch emulators use: a .so the player puts on the
//   device, named relative to the folder env.txt is in (or by an absolute path). Android loads no library from shared storage, so it is copied into the app's internal files
//   (vk_driver/<name>), and opened through libadrenotools (dm/third_party/libadrenotools): a second copy of the system's libvulkan.so in a linker namespace of its own, where
//   a hook loads the player's driver in place of the vendor's. The loader in that copy still makes the window surface and the swapchain, which Turnip on Android lacks.
//   A driver that cannot be copied or opened, or that offers no GPU, logs one [RED] line, and the system's driver runs.
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "dm.h"
#include "gfx_vk.h"
#include "../third_party/libadrenotools/include/adrenotools/driver.h"   // the type of adrenotools_open_libvulkan, opened with dlsym: the host does not link it

#define VKB_DEF(f) PFN_##f f;
VKB_FNS_GLOBAL(VKB_DEF) VKB_FNS(VKB_DEF)
static PFN_vkGetInstanceProcAddr g_gipa;
static char g_ext[512], g_int[512], g_name[256];   // vk_driver_dirs; g_name: the DM_VK_DRIVER file running

void vk_driver_dirs(const char *ext, const char *internal) { snprintf(g_ext, sizeof g_ext, "%s", ext); snprintf(g_int, sizeof g_int, "%s", internal); }
const char *vk_driver_name(void) { return g_name[0] ? g_name : NULL; }
#define DRV_RED(drv, fmt, ...) LOG("[RED] [vk] DM_VK_DRIVER %s: " fmt, drv, ##__VA_ARGS__)

// src → <dir><name> (dir: <internal>/vk_driver/), unless the copy there has src's size and time already. Read-only once written, as Android 14 asks of code an app loads
// from its own files. → 0 with errno set
static int copy_in(const char *src, const char *dir, const char *name) {
    struct stat s, d; char dst[800], tmp[810];
    if (stat(src, &s)) return 0;
    if (!S_ISREG(s.st_mode)) { errno = EISDIR; return 0; }
    if (mkdir(dir, 0700) && errno != EEXIST) return 0;
    snprintf(dst, sizeof dst, "%s%s", dir, name); snprintf(tmp, sizeof tmp, "%s.part", dst);
    if (!stat(dst, &d) && d.st_size == s.st_size && d.st_mtime == s.st_mtime) return 1;
    int in = open(src, O_RDONLY | O_CLOEXEC); if (in < 0) return 0;
    unlink(tmp); int out = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (out < 0) { int e = errno; close(in); errno = e; return 0; }
    static char buf[1 << 16]; ssize_t k = 0; int ok = 1;
    while (ok && (k = read(in, buf, sizeof buf)) > 0) for (ssize_t w = 0, n; w < k; w += n) if ((n = write(out, buf + w, (size_t)(k - w))) <= 0) { ok = 0; break; }
    struct timespec ts[2] = {s.st_atim, s.st_mtim};
    ok = ok && k == 0 && !fchmod(out, 0500) && !futimens(out, ts);
    int e = errno; close(in); if (close(out)) ok = 0;
    if (ok) { unlink(dst); ok = !rename(tmp, dst); e = errno; }
    if (!ok) unlink(tmp);
    errno = e; return ok;
}

// Whether the file at path is mapped in this process: when adrenotools' hook cannot put the player's driver in place, it opens the vendor's instead and says so only in
// logcat. By device and inode, as /proc/self/maps lists them (its path column may name the file through another mount, /data/data for /data/user/0). (Not
// dl_iterate_phdr: <link.h> clashes with dm.h's ELF types)
static int mapped(const char *path) {
    struct stat st; if (stat(path, &st)) return 0;
    FILE *f = fopen("/proc/self/maps", "re"); if (!f) return 0;
    char line[1024]; unsigned maj, mnr; unsigned long long ino; int found = 0;
    while (!found && fgets(line, sizeof line, f))
        found = sscanf(line, "%*s %*s %*s %x:%x %llu", &maj, &mnr, &ino) == 3 && ino == (unsigned long long)st.st_ino && maj == major(st.st_dev) && mnr == minor(st.st_dev);
    fclose(f); return found;
}

// The GPU a driver offers, on an instance made for the question: a Turnip built for another Adreno generation opens and then has none
static int first_gpu(PFN_vkGetInstanceProcAddr g, VkPhysicalDeviceProperties *pr) {
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)g(NULL, "vkCreateInstance");
    VkApplicationInfo ai = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "nxnative", .apiVersion = VK_API_VERSION_1_1};
    VkInstanceCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
    VkInstance in; if (!create || create(&ic, NULL, &in) != VK_SUCCESS) return 0;
    PFN_vkEnumeratePhysicalDevices gpus = (PFN_vkEnumeratePhysicalDevices)g(in, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties props = (PFN_vkGetPhysicalDeviceProperties)g(in, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)g(in, "vkDestroyInstance");
    uint32_t n = 1; VkPhysicalDevice pd; VkResult r = gpus ? gpus(in, &n, &pd) : VK_ERROR_INITIALIZATION_FAILED;
    int ok = (r == VK_SUCCESS || r == VK_INCOMPLETE) && n && props;
    if (ok) props(pd, pr);
    if (destroy) destroy(in, NULL);
    return ok;
}

// DM_VK_DRIVER's driver → its vkGetInstanceProcAddr, NULL (and one [RED] line) when it cannot run
static PFN_vkGetInstanceProcAddr open_custom(const char *drv) {
    char src[700], dir[600]; const char *name;
    if (drv[0] == '/' || !g_ext[0]) snprintf(src, sizeof src, "%s", drv); else snprintf(src, sizeof src, "%s/%s", g_ext, drv);
    name = strrchr(src, '/') ? strrchr(src, '/') + 1 : src;
    if (!*name || strlen(name) >= sizeof g_name) { DRV_RED(drv, "not a file name; the system's driver instead"); return NULL; }
    if (g_int[0]) {   // the app: copied in
        snprintf(dir, sizeof dir, "%s/vk_driver/", g_int);
        if (!copy_in(src, dir, name)) { DRV_RED(drv, "%s: %s; the system's driver instead", src, strerror(errno)); return NULL; }
    } else {          // the command-line host (adb shell): loaded where it lies, which must be a folder that allows code (/data/local/tmp, not /sdcard)
        if (access(src, R_OK)) { DRV_RED(drv, "%s: %s; the system's driver instead", src, strerror(errno)); return NULL; }
        snprintf(dir, sizeof dir, "%.*s/", name > src ? (int)(name - src - 1) : 1, name > src ? src : ".");
    }
    // adrenotools' hooks (libhook_impl.so, libmain_hook.so) are looked for where the host lies: the app's native library folder, extracted (AndroidManifest extractNativeLibs)
    Dl_info di; char libs[600] = "";
    if (dladdr((void *)open_custom, &di) && di.dli_fname && strrchr(di.dli_fname, '/'))
        snprintf(libs, sizeof libs, "%.*s/", (int)(strrchr(di.dli_fname, '/') - di.dli_fname), di.dli_fname);
    void *at = dlopen("libadrenotools.so", RTLD_NOW | RTLD_LOCAL);
    __typeof__(adrenotools_open_libvulkan) *open_vk = at ? (__typeof__(adrenotools_open_libvulkan) *)dlsym(at, "adrenotools_open_libvulkan") : NULL;
    if (!open_vk) { DRV_RED(drv, "libadrenotools.so is not in this app (%s): pack the app again; the system's driver instead", dlerror()); return NULL; }
    void *lib = open_vk(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM, NULL, libs, dir, name, NULL, NULL);
    PFN_vkGetInstanceProcAddr g = lib ? (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr") : NULL;
    if (!g) { DRV_RED(drv, "libadrenotools could not open it (adb logcat -s hook_impl says why); the system's driver instead"); return NULL; }
    // (a driver that opened and is not used stays loaded: drivers do not always unload cleanly)
    VkPhysicalDeviceProperties pr; char path[900]; snprintf(path, sizeof path, "%s%s", dir, name);
    if (!first_gpu(g, &pr)) { DRV_RED(drv, "it offers no GPU (a build for another GPU?); the system's driver instead"); return NULL; }
    if (!mapped(path)) {
        DRV_RED(drv, "libadrenotools opened the system's driver in its place (adb logcat -s hook_impl says why); the system's driver instead"); return NULL; }
    snprintf(g_name, sizeof g_name, "%s", name);
    LOG("[vk] driver: %s (DM_VK_DRIVER, through libadrenotools), not the system's: %s, Vulkan %u.%u.%u, driver version %#x", path, pr.deviceName,
        VK_API_VERSION_MAJOR(pr.apiVersion), VK_API_VERSION_MINOR(pr.apiVersion), VK_API_VERSION_PATCH(pr.apiVersion), pr.driverVersion);
    return g;
}

int vk_driver_load(void) {
    const char *drv = getenv("DM_VK_DRIVER");
    if (drv && *drv) g_gipa = open_custom(drv);
    if (!g_gipa) {
        void *lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (!lib || !(g_gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr"))) { LOG("[RED] [vk] no Vulkan: libvulkan.so: %s", dlerror()); return 0; }
    }
#define VKB_LOAD_GLOBAL(f) f = (PFN_##f)g_gipa(NULL, #f);
    VKB_FNS_GLOBAL(VKB_LOAD_GLOBAL)
    if (!vkCreateInstance || !vkEnumerateInstanceExtensionProperties) { LOG("[RED] [vk] no Vulkan: the driver has no vkCreateInstance"); return 0; }
    return 1;
}

void vk_driver_instance(VkInstance inst) {
#define VKB_LOAD(f) f = (PFN_##f)g_gipa(inst, #f);
    VKB_FNS(VKB_LOAD)
}
