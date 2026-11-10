# stage.ps1's check that the runtime loads its own native libraries in a process started the way Inno Setup starts DroidPlay: this script gives itself the
# "prefer System32 images" mitigation, which children inherit, then has the runtime's python.exe import every extension module of the runtime and the Qt
# modules the app uses, and say which libssl it loaded (_droidplay_dlls.py says why). Where System32 holds an older OpenSSL the import fails without that
# module; elsewhere this checks that the module loaded the runtime's own DLLs.
#   python check_runtime_dlls.py <runtime folder>
import ctypes, ctypes.wintypes, os, subprocess, sys

PREFER_SYSTEM32 = 4   # PROCESS_MITIGATION_IMAGE_LOAD_POLICY.PreferSystem32Images
CHILD = r'''
import ctypes, glob, importlib, os, sys
k = ctypes.WinDLL('kernel32'); k.GetModuleHandleW.restype = ctypes.c_void_p
dlls = os.path.join(sys.base_prefix, 'DLLs')
bad = [f'{os.path.basename(lib)} was not loaded as Python started (no _droidplay_dlls.pth?)' for lib in glob.glob(os.path.join(dlls, 'libssl-3*.dll'))
       if not k.GetModuleHandleW(os.path.basename(lib))]
names = [os.path.basename(f).split('.')[0] for f in glob.glob(os.path.join(dlls, '*.pyd'))]
names += ['ssl', 'PySide6.QtCore', 'PySide6.QtGui', 'PySide6.QtWidgets', 'PySide6.QtSvg']
for n in names:
    try: importlib.import_module(n)
    except Exception as e: bad.append(f'{n}: {e}')
for lib in glob.glob(os.path.join(dlls, 'libssl-3*.dll')):
    h = k.GetModuleHandleW(os.path.basename(lib)); buf = ctypes.create_unicode_buffer(32768)
    if h and k.GetModuleFileNameW(ctypes.c_void_p(h), buf, len(buf)) and os.path.normcase(buf.value) != os.path.normcase(lib):
        bad.append(f'{os.path.basename(lib)} came from {buf.value}, not {dlls}')
print(f'{len(names)} modules'); print('\n'.join(bad)); sys.exit(1 if bad else 0)
'''


class ImageLoadPolicy(ctypes.Structure): _fields_ = [('Flags', ctypes.wintypes.DWORD)]


policy = ImageLoadPolicy(PREFER_SYSTEM32)
if not ctypes.WinDLL('kernel32', use_last_error=True).SetProcessMitigationPolicy(10, ctypes.byref(policy), ctypes.sizeof(policy)):   # 10: ProcessImageLoadPolicy
    sys.exit(f'could not set the image load policy (error {ctypes.get_last_error()})')
r = subprocess.run([os.path.join(sys.argv[1], 'python.exe'), '-c', CHILD], capture_output=True, text=True)
print(r.stdout.strip(), r.stderr.strip()[-2000:], sep='\n' if r.stderr.strip() else '')
sys.exit(r.returncode)
