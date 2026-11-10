# Loads the OpenSSL DLLs of this Python by their full path as it starts (stage.ps1 puts this module and a .pth naming it into the runtime's site-packages).
# Inno Setup gives itself the "prefer System32 images" mitigation, and what it runs inherits it: DroidPlay opened from Setup's last page or by Update's
# /RELAUNCH, and every Python that DroidPlay then starts. Such a process looks for a DLL in System32 before the folder of the module that needs it, so where
# another program has left an older libssl-3-x64.dll / libcrypto-3-x64.dll there (OpenSSL 3.0.8 on the test PC), _ssl.pyd binds to it and fails to load
# ("the specified procedure could not be found"): no https, so Install tools and the update check stop with "unknown url type: https". A DLL already loaded
# under the same name is used instead of any search, so loading ours first is enough.
import glob, os, sys

if os.name == 'nt':
    try:
        import ctypes
        for pattern in ('libcrypto-3*.dll', 'libssl-3*.dll'):   # libcrypto first: libssl needs it
            for dll in glob.glob(os.path.join(sys.base_prefix, 'DLLs', pattern)): ctypes.WinDLL(dll)
    except OSError: pass   # then ssl fails as it would have, and says why
