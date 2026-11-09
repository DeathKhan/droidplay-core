# DroidPlay for Windows' updates (scripts/nxnative_android.py): the newest release on GitHub, and its DroidPlay-Setup downloaded and checked before the app
# runs it. Without Qt, so the packing tests (dm/tools/tests/test_android_cli.py) run it where PySide6 isn't installed.
#   The app says a newer release is out, as DroidPlay for Mac does. A copy DroidPlay-Setup installed (dm/android/windows/droidplay.iss) also offers Update:
#   the release's Setup, downloaded by the app and checked against the SHA-256 GitHub lists for it, then run silently (/RELAUNCH opens the new DroidPlay).
#   A download made by the app carries no mark of the web, so Windows doesn't warn about an unsigned file on every release, as it does for one from a browser.
import hashlib, json, os, re, urllib.request
from pathlib import Path

RELEASES_API = 'https://api.github.com/repos/enjoyerofslop/droidplay-core/releases/latest'
SETUP_NAME = re.compile(r'DroidPlay-Setup-\d+\.\d+\.\d+\.exe')


class Cancelled(Exception): pass


def release_number(v):   # (0, 5, 1) for v0.5.1 or 0.5.1; None for anything else, such as a checkout's 0.5.0-3-gabc1234, which no release can be newer than
    m = re.fullmatch(r'v?(\d+)\.(\d+)\.(\d+)', (v or '').strip())
    return tuple(int(x) for x in m.groups()) if m else None


def latest_release(api=RELEASES_API):
    """The newest release: {tag, url (its page), setup: {name, url, size, sha256} or None}. Its Setup only with the digest GitHub lists for it, which the
    download is checked against. Raises when offline or rate-limited"""
    req = urllib.request.Request(api, headers={'User-Agent': 'DroidPlay', 'Accept': 'application/vnd.github+json'})
    with urllib.request.urlopen(req, timeout=15) as r: d = json.load(r)
    setup = None
    for a in d.get('assets') or []:
        digest = re.fullmatch(r'sha256:([0-9a-f]{64})', a.get('digest') or '')
        if SETUP_NAME.fullmatch(a.get('name') or '') and digest:
            setup = {'name': a['name'], 'url': a['browser_download_url'], 'size': a.get('size') or 0, 'sha256': digest.group(1)}
    return {'tag': d['tag_name'], 'url': d['html_url'], 'setup': setup}


def setup_install(repo):   # whether DroidPlay-Setup installed this copy: <app>\core, with the runtime and Inno's uninstaller beside it
    app = Path(repo).parent
    return (app / 'unins000.exe').is_file() and (app / 'runtime' / 'pythonw.exe').is_file()


def download_setup(setup, folder, progress=None, cancelled=lambda: False):
    """The release's Setup into folder, checked: its path. Raises Cancelled, or ValueError when it doesn't match the release's digest (deleted then).
    progress(done, total) is called as it comes; older downloads in folder go"""
    folder = Path(folder); folder.mkdir(parents=True, exist_ok=True)
    out = folder / setup['name']; part = folder / (setup['name'] + '.part')
    for f in folder.glob('DroidPlay-Setup-*'):
        if f.name not in (out.name, part.name): f.unlink(missing_ok=True)
    h, done = hashlib.sha256(), 0
    req = urllib.request.Request(setup['url'], headers={'User-Agent': 'DroidPlay'})
    try:
        with urllib.request.urlopen(req, timeout=30) as r, open(part, 'wb') as f:
            total = setup.get('size') or int(r.headers.get('Content-Length') or 0)
            while chunk := r.read(1 << 16):
                if cancelled(): raise Cancelled()
                f.write(chunk); h.update(chunk); done += len(chunk)
                if progress: progress(done, total)
    except BaseException:
        part.unlink(missing_ok=True); raise
    if h.hexdigest() != setup['sha256']:
        part.unlink(missing_ok=True)
        raise ValueError(f"{setup['name']} doesn't match the release's checksum, so it was deleted and nothing was installed")
    os.replace(part, out)
    return out
