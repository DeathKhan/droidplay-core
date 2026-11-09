# What droidnative's Android packing needs on this computer, installed where scripts/build_android_from_nsp.py finds it: its --setup, and the desktop app's
# Settings > Install what's missing. Windows only for now; on a Mac, DroidPlay's Setup installs the same with Homebrew.
#   JDK 21        Microsoft's build (aka.ms), checked against its .sha256sum.txt                             into <data>/tools/jdk
#   .NET SDK 10   the newest 10.0 SDK in Microsoft's release metadata, checked against its SHA-512             into <data>/tools/dotnet
#   Android SDK   Google's command-line tools (repository2-3.xml, checked against its SHA-1), then the packages with them: Android CLI ("android sdk install",
#                 from command-line tools 23, which turned sdkmanager into a wrapper of it) or sdkmanager before that. Into the SDK build_android_from_nsp.py found,
#                 else Android Studio's place, %LOCALAPPDATA%\Android\Sdk. Only once the player has accepted the Android SDK License (accept).
# The JDK and .NET SDK go where only this tool looks, so a computer's own stay as they are; add_tool_paths puts them first on PATH for this process and what it runs.
import hashlib, json, os, platform, shutil, subprocess, urllib.request, xml.etree.ElementTree as ET, zipfile

REPOSITORY = 'https://dl.google.com/android/repository/'
DOTNET_RELEASES = 'https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json'
AGENT = {'User-Agent': 'droidnative-setup'}
WINDOWS = os.name == 'nt'
NOWINDOW = 0x08000000 if WINDOWS else 0   # CREATE_NO_WINDOW: no console flashing up under the desktop app


class SetupError(Exception): pass


def arch(): return 'arm64' if platform.machine().lower() in ('arm64', 'aarch64') else 'x64'


def add_tool_paths(tools, sdk=None):
    """The JDK and .NET SDK this module installed into tools first on PATH, for this process and what it runs: they are there because the computer had none
    (or no .NET 10), so they win over an older one. The SDK's platform-tools go last, for adb, so a computer's own adb keeps its server."""
    path = [p for p in os.environ.get('PATH', '').split(os.pathsep) if p]
    first = []
    if (tools / 'jdk' / 'bin').is_dir(): first.append(str(tools / 'jdk' / 'bin')); os.environ.setdefault('JAVA_HOME', str(tools / 'jdk'))
    if (tools / 'dotnet').is_dir(): first.append(str(tools / 'dotnet')); os.environ['DOTNET_ROOT'] = str(tools / 'dotnet')
    last = [str(sdk / 'platform-tools')] if sdk and (sdk / 'platform-tools').is_dir() else []
    os.environ['PATH'] = os.pathsep.join(first + [p for p in path if p not in first and p not in last] + last)
    os.environ.setdefault('DOTNET_CLI_TELEMETRY_OPTOUT', '1'); os.environ.setdefault('DOTNET_NOLOGO', '1')   # dotnet build sends usage data unless told not to


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=60) as r: return r.read()


def download(url, dst, algo, digest, prog, what):   # into dst, checked against the digest its publisher gives
    h, n = hashlib.new(algo), 0
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=60) as r, open(dst, 'wb') as f:
        total = int(r.headers.get('Content-Length') or 0) or None
        while chunk := r.read(1 << 20):
            f.write(chunk); h.update(chunk); n += len(chunk); prog.set(done=n, total=total, unit='B', step=f'downloading {what}')
    if h.hexdigest().lower() != digest.lower(): dst.unlink(missing_ok=True); raise SetupError(f'{what}: the download does not match its {algo} ({url})')


def unpack(zip_path, dst, prog, what, top=False):   # dst made whole or not at all; top: the archive holds one folder, whose contents become dst
    part = dst.with_name(dst.name + '.partial'); shutil.rmtree(part, ignore_errors=True)
    prog.set(done=None, total=None, step=f'unpacking {what}')
    with zipfile.ZipFile(zip_path) as z: z.extractall(part)
    src = next(part.iterdir()) if top else part
    shutil.rmtree(dst, ignore_errors=True); dst.parent.mkdir(parents=True, exist_ok=True); shutil.move(str(src), str(dst)); shutil.rmtree(part, ignore_errors=True)


def install_jdk(tools, work, prog):
    url = f'https://aka.ms/download-jdk/microsoft-jdk-21-windows-{"aarch64" if arch() == "arm64" else "x64"}.zip'
    digest = fetch(url + '.sha256sum.txt').decode().split()[0]
    z = work / 'jdk.zip'; download(url, z, 'sha256', digest, prog, 'the JDK'); unpack(z, tools / 'jdk', prog, 'the JDK', top=True); z.unlink()
    return tools / 'jdk'


def install_dotnet(tools, work, prog):
    rel = json.loads(fetch(DOTNET_RELEASES)); v = rel['latest-sdk']; rid = f'win-{arch()}'
    f = next((f for r in rel['releases'] if (r.get('sdk') or {}).get('version') == v for f in r['sdk']['files'] if f['rid'] == rid and f['name'].endswith('.zip')), None)
    if not f: raise SetupError(f'no .NET SDK {v} for {rid} in {DOTNET_RELEASES}')
    z = work / 'dotnet.zip'; download(f['url'], z, 'sha512', f['hash'], prog, f'the .NET SDK {v}'); unpack(z, tools / 'dotnet', prog, 'the .NET SDK'); z.unlink()
    return tools / 'dotnet'


def install_cmdline_tools(sdk, work, prog):   # the newest command-line tools, as <sdk>/cmdline-tools/latest
    root = ET.fromstring(fetch(REPOSITORY + 'repository2-3.xml'))
    pkg = next((p for p in root.iter('remotePackage') if p.get('path') == 'cmdline-tools;latest'), None)
    a = next((a for a in (pkg.iter('archive') if pkg is not None else []) if a.findtext('host-os') == 'windows'), None)
    if a is None: raise SetupError(f'no command-line tools for Windows in {REPOSITORY}repository2-3.xml')
    c = a.find('complete'); z = work / 'cmdline-tools.zip'
    download(REPOSITORY + c.findtext('url'), z, 'sha1', c.findtext('checksum'), prog, 'the Android command-line tools')
    unpack(z, sdk / 'cmdline-tools' / 'latest', prog, 'the Android command-line tools', top=True); z.unlink()


def install_packages(sdk, packages, prog):
    """packages: [(sdkmanager's name, a file it puts there)]. The Android CLI takes one package a call, named with / for ; (ndk/28.2.13676358)"""
    bin_ = sdk / 'cmdline-tools' / 'latest' / 'bin'; cli = bin_ / 'android.exe'; sm = bin_ / 'sdkmanager.bat'
    for i, (p, made) in enumerate(packages):
        prog.set(done=i, total=len(packages), unit='', step=f'installing {p}')
        if cli.is_file(): cmd, feed = [str(cli), '--no-metrics', f'--sdk={sdk}', 'sdk', 'install', p.replace(';', '/')], None
        else: cmd, feed = [str(sm), f'--sdk_root={sdk}', '--install', p], 'y\n' * 20   # the licences, which the player accepted in the desktop app
        r = subprocess.run(cmd, input=feed, capture_output=True, text=True, errors='replace', creationflags=NOWINDOW)
        if r.returncode or not made.exists():
            tail = '\n'.join(l for l in (r.stdout + r.stderr).splitlines() if '[=' not in l and '[#' not in l)[-600:]
            raise SetupError(f'{p} did not install (exit {r.returncode}, {made} {"made" if made.exists() else "missing"}):\n{tail}')
    prog.set(done=len(packages), total=len(packages), unit='')


def setup(missing, sdk, tools, packages, accept, prog, say):
    """Install what check() reports missing. missing: the names of its failing rows; sdk: the SDK found, else where to make one; packages: {row name: (sdkmanager's
    name, a file it puts there)} for the SDK's rows. Returns what was installed"""
    if not WINDOWS: raise SetupError('--setup installs for Windows only so far: on a Mac, DroidPlay\'s Setup installs the same; elsewhere, install what --check lists')
    jdk = bool(missing & {'JDK keytool', 'JDK javac'}); dotnet = '.NET SDK 10' in missing
    pkgs = [packages[n] for n in packages if n in missing or 'Android SDK' in missing]
    tools_sdk = not (sdk / 'cmdline-tools' / 'latest' / 'bin').is_dir() and bool(pkgs)
    if pkgs and not accept: raise SetupError('the Android SDK\'s packages install only once its licence is accepted (https://developer.android.com/studio/terms): give --accept-android-licenses')
    steps = [s for s, on in (('JDK 21', jdk), ('.NET SDK 10', dotnet), ('Android command-line tools', tools_sdk), ('Android SDK packages', pkgs)) if on]
    if not steps: say('nothing to install: this computer has everything --check looks for'); return []
    work = tools / 'downloads'; work.mkdir(parents=True, exist_ok=True); done = []
    prog.plan(len(steps))
    for s in steps:
        prog.stage(s)
        if s == 'JDK 21': prog.end(str(install_jdk(tools, work, prog)))
        elif s == '.NET SDK 10': prog.end(str(install_dotnet(tools, work, prog)))
        elif s == 'Android command-line tools': install_cmdline_tools(sdk, work, prog); prog.end(str(sdk / 'cmdline-tools' / 'latest'))
        else: install_packages(sdk, pkgs, prog); prog.end(', '.join(p for p, _ in pkgs))
        done.append(s)
    shutil.rmtree(work, ignore_errors=True)
    return done
