# A stand-in for Python's fcntl where the platform has none (Windows), which scripts/build_android_from_nsp.py puts on the path only then. nxnative's
# dm/tools/tree_tar.py imports fcntl as build_ipa_from_nsp.py loads, for its lock around Xcode's derived data, which only the iOS build uses: without a module of
# that name, the Android packing script and the desktop app (scripts/nxnative_android.py) stop at their first import on Windows. flock fails the way it does on a
# drive without locks, which tree_tar.py already takes as "no lock"; nothing on the Android side takes one.
import errno

LOCK_SH, LOCK_EX, LOCK_NB, LOCK_UN = 1, 2, 4, 8


def flock(fd, operation):
    raise OSError(errno.ENOTSUP, 'no file locks on this platform (dm/android/tools/compat/fcntl.py)')
