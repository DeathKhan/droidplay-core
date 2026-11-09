"""The Android packing script's machine-readable side and its phone copy (scripts/build_android_from_nsp.py), as the desktop app (scripts/nxnative_android.py) uses them:
--check prints its requirement rows as JSON alone on stdout; the games table is read from dm/games/README.md; the host library keeps one host per game and the APK is made
from all of them; a game is copied to files/games/<title ID>/ and its folders opened to the app (Android 15 kept it out of adb's drwxrws--- folders); a one-game install
(files/game) of the same game is moved there with its legacy_save marker, one of another game is left alone. No phone, SDK or game files needed (adb is a recording stand-in)."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[3]
SCRIPT = REPO / 'scripts/build_android_from_nsp.py'
spec = importlib.util.spec_from_file_location('build_android_from_nsp', SCRIPT)
assert spec and spec.loader
ba = importlib.util.module_from_spec(spec); spec.loader.exec_module(ba)
GID = '010015100B514000'


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp()); self.addCleanup(shutil.rmtree, self.tmp)
        self.said = []
        for cm in (mock.patch.object(ba.bi, 'PROG', mock.Mock()), mock.patch.object(ba, 'say', self.said.append), mock.patch.object(ba, 'data_dir', lambda: self.tmp / 'data')):
            cm.__enter__(); self.addCleanup(cm.__exit__, None, None, None)


class Json(unittest.TestCase):
    def test_check_prints_only_json_rows(self):
        r = subprocess.run([sys.executable, SCRIPT, '--check'], capture_output=True, text=True, encoding='utf-8', timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr)
        rows = json.loads(r.stdout)
        self.assertTrue(rows and all(set(x) == {'name', 'ok', 'detail', 'fix'} for x in rows))
        names = {x['name'] for x in rows}
        for want in ('Android SDK', 'adb', 'JDK keytool', '.NET SDK 10', 'Python module lz4'): self.assertIn(want, names)
        self.assertTrue(all(x['fix'] == '' for x in rows if x['ok']))

    def test_games_table(self):
        t = ba.games_table()
        self.assertIn(GID, t)
        self.assertEqual(t[GID]['graphics'], 'NVN')
        self.assertTrue(all(len(k) == 16 for k in t))


class Windows(unittest.TestCase):
    def test_loads_where_python_has_no_fcntl(self):   # Windows: build_ipa_from_nsp.py loads tree_tar.py, which imports fcntl; dm/android/tools/compat's stand-in is found
        code = '\n'.join([
            'import importlib.machinery, importlib.util, sys',
            'class NoFcntl:   # the platform has no fcntl, as on Windows: only a stand-in on the path is found',
            '    def find_spec(self, name, path=None, target=None):',
            "        if name != 'fcntl': return None",
            "        spec = importlib.machinery.PathFinder.find_spec(name, [p for p in sys.path if p.endswith('compat')])",
            "        if spec is None: raise ModuleNotFoundError(\"No module named 'fcntl'\")",
            '        return spec',
            "sys.modules.pop('fcntl', None); sys.meta_path.insert(0, NoFcntl())",
            f"spec = importlib.util.spec_from_file_location('build_android_from_nsp', {str(SCRIPT)!r})",
            'ba = importlib.util.module_from_spec(spec); spec.loader.exec_module(ba)',
            'import fcntl, tree_tar',
            'print(fcntl.__file__)',
            'try: fcntl.flock(0, fcntl.LOCK_EX | fcntl.LOCK_NB)',
            "except OSError: print('no lock')",
        ])
        r = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True, encoding='utf-8', timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        where, lock = r.stdout.splitlines()
        self.assertEqual(Path(where), REPO / 'dm/android/tools/compat/fcntl.py')
        self.assertEqual(lock, 'no lock')

    def test_game_app_stage_without_symbolic_links(self):   # Windows makes them only with Developer Mode on (or as administrator): the stage takes the files instead
        tmp = Path(tempfile.mkdtemp()); self.addCleanup(shutil.rmtree, tmp)
        spv = tmp / 'spv'; (spv / 'vs').mkdir(parents=True); (spv / 'vs' / '0001.spv').write_bytes(b'spv'); (tmp / 'icon.jpg').write_bytes(b'jpg')
        stage = tmp / 'stage'; stage.mkdir()
        def refused(*a, **k): raise OSError(1314, 'A required privilege is not held by the client')
        with mock.patch.object(Path, 'symlink_to', refused):
            ba.stage_link(stage / 'spv', spv); ba.stage_link(stage / 'icon.jpg', tmp / 'icon.jpg')
        self.assertFalse((stage / 'spv').is_symlink())
        self.assertEqual((stage / 'spv/vs/0001.spv').read_bytes(), b'spv')
        self.assertEqual((stage / 'icon.jpg').read_bytes(), b'jpg')


class Setup(unittest.TestCase):   # --setup (dm/android/tools/host_setup.py): what --check finds missing, installed where the script looks; nothing is downloaded here
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp()); self.addCleanup(shutil.rmtree, self.tmp)
        self.hs = ba.host_setup; self.sdk = self.tmp / 'sdk'; self.tools = self.tmp / 'tools'
        cm = mock.patch.object(self.hs, 'WINDOWS', True); cm.__enter__(); self.addCleanup(cm.__exit__, None, None, None)

    def test_installed_tools_go_first_on_path_and_the_sdks_adb_last(self):   # they are there because the computer had none; its own adb keeps its server
        for d in ('tools/jdk/bin', 'tools/dotnet', 'sdk/platform-tools'): (self.tmp / d).mkdir(parents=True)
        with mock.patch.dict(os.environ, {'PATH': os.pathsep.join(['/usr/bin', str(self.sdk / 'platform-tools')])}):
            os.environ.pop('JAVA_HOME', None); self.hs.add_tool_paths(self.tools, self.sdk)
            self.assertEqual(os.environ['PATH'].split(os.pathsep), [str(self.tools / 'jdk/bin'), str(self.tools / 'dotnet'), '/usr/bin', str(self.sdk / 'platform-tools')])
            self.assertEqual(os.environ['JAVA_HOME'], str(self.tools / 'jdk')); self.assertEqual(os.environ['DOTNET_ROOT'], str(self.tools / 'dotnet'))

    def test_only_what_is_missing_is_installed(self):
        calls = []
        with mock.patch.object(self.hs, 'install_jdk', lambda *a: calls.append('jdk') or self.tools / 'jdk'), \
             mock.patch.object(self.hs, 'install_dotnet', lambda *a: calls.append('dotnet')), \
             mock.patch.object(self.hs, 'install_cmdline_tools', lambda *a: calls.append('cmdline-tools')), \
             mock.patch.object(self.hs, 'install_packages', lambda sdk, pkgs, prog: calls.append([p for p, _ in pkgs])):
            done = self.hs.setup({'JDK javac', 'NDK', 'Python module PySide6'}, self.sdk, self.tools, ba.sdk_packages(self.sdk), True, mock.Mock(), lambda m: None)
        self.assertEqual(calls, ['jdk', 'cmdline-tools', [f'ndk;{ba.NDK}']])
        self.assertEqual(done, ['JDK 21', 'Android command-line tools', 'Android SDK packages'])

    def test_no_sdk_found_installs_every_package(self):
        got = []
        with mock.patch.object(self.hs, 'install_cmdline_tools', lambda *a: None), mock.patch.object(self.hs, 'install_packages', lambda sdk, pkgs, prog: got.extend(pkgs)):
            self.hs.setup({'Android SDK'}, self.sdk, self.tools, ba.sdk_packages(self.sdk), True, mock.Mock(), lambda m: None)
        self.assertEqual({p for p, _ in got}, {'platform-tools', f'platforms;{ba.PLATFORM}', f'build-tools;{ba.BUILD_TOOLS}', f'ndk;{ba.NDK}', f'cmake;{ba.CMAKE}'})

    def test_the_sdk_waits_for_its_licence(self):
        with self.assertRaisesRegex(self.hs.SetupError, 'accept-android-licenses'):
            self.hs.setup({'NDK'}, self.sdk, self.tools, ba.sdk_packages(self.sdk), False, mock.Mock(), lambda m: None)

    def test_off_windows_it_says_where_to_go(self):
        with mock.patch.object(self.hs, 'WINDOWS', False), self.assertRaisesRegex(self.hs.SetupError, 'DroidPlay'):
            self.hs.setup({'NDK'}, self.sdk, self.tools, ba.sdk_packages(self.sdk), True, mock.Mock(), lambda m: None)

    def run_install(self, cli):   # install_packages with subprocess stubbed: the command for the NDK, and what it was fed
        b = self.sdk / 'cmdline-tools/latest/bin'; b.mkdir(parents=True); (b / ('android.exe' if cli else 'sdkmanager.bat')).touch()
        seen = []
        def run(cmd, input=None, **k): seen.append((cmd, input)); (self.sdk / 'ndk' / ba.NDK).mkdir(parents=True); return mock.Mock(returncode=0, stdout='', stderr='')
        with mock.patch.object(self.hs.subprocess, 'run', run): self.hs.install_packages(self.sdk, [ba.sdk_packages(self.sdk)['NDK']], mock.Mock())
        return seen[0]

    def test_packages_go_through_the_android_cli_without_metrics(self):   # command-line tools 23: one package a call, / for ;
        cmd, fed = self.run_install(cli=True)
        self.assertEqual(cmd[1:], ['--no-metrics', f'--sdk={self.sdk}', 'sdk', 'install', f'ndk/{ba.NDK}']); self.assertIsNone(fed)

    def test_older_command_line_tools_use_sdkmanager(self):
        cmd, fed = self.run_install(cli=False)
        self.assertEqual(cmd[1:], [f'--sdk_root={self.sdk}', '--install', f'ndk;{ba.NDK}']); self.assertTrue(fed.startswith('y\n'))


class Library(Base):
    def test_hosts_kept_per_game_and_all_in_the_apk(self):
        for gid in (GID, '0100A6301214E000'):
            so = self.tmp / f'libdmhost_{gid}.so'; so.write_bytes(b'host'); loader = self.tmp / 'libnxloader.so'; loader.write_bytes(b'loader')
            st = self.tmp / 'st'; st.mkdir(exist_ok=True); (st / 'main.patched.bin').write_bytes(b'x')
            ba.library_add(gid, so, loader, st, {'name': gid})
        self.assertEqual(sorted(ba.library()), sorted([GID, '0100A6301214E000']))
        ran = []
        with mock.patch.object(ba.bi, 'run', lambda cmd, log, env=None: ran.append([str(c) for c in cmd])):
            apk = ba.make_apk(Path('sdk'), self.tmp, {})
        self.assertEqual(apk, self.tmp / 'data' / 'nxnative.apk')
        cmd = ran[0]
        self.assertEqual([cmd[i + 1] for i, c in enumerate(cmd) if c == '--so'], [str(self.tmp / 'data/hosts' / g / f'libdmhost_{g}.so') for g in sorted([GID, '0100A6301214E000'])])
        self.assertIn('--loader', cmd)

    def test_no_host_no_apk(self):
        with self.assertRaises(ba.BuildError): ba.make_apk(Path('sdk'), self.tmp, {})


class Journal(Base):
    def test_events_in_the_journal_whatever_the_console_mode(self):
        p = ba.Journaled('off', ['x', '--no-push'])
        p.plan(2); p.stage('one', step='a', total=10, done=3); p.set(done=7); p.end('fine'); p.say('hello'); p.stage('two'); p.exit(0)
        (f,) = (self.tmp / 'data/builds').glob('*.jsonl')
        evs = [json.loads(line) for line in f.read_text(encoding='utf-8').splitlines()]
        self.assertEqual(evs[0]['ev'], 'start'); self.assertEqual(evs[0]['argv'], ['x', '--no-push'])
        self.assertEqual(evs[-1], {'ev': 'exit', 'code': 0, 't': evs[-1]['t']})
        kinds = [e['ev'] for e in evs]
        self.assertEqual(kinds.count('stage'), 2); self.assertEqual(kinds.count('stage_end'), 2)   # the second ended by exit()
        self.assertIn('say', kinds)
        self.assertEqual([e['done'] for e in evs if e['ev'] == 'progress' and e['name'] == 'one'][-1], 7)


class Phone(Base):
    def setUp(self):
        super().setUp()
        self.cmds = []; self.remote = {}   # remote path → exists
        def adb(serial, *a, log=None, check=True):
            c = [str(x) for x in a]; self.cmds.append(c)
            if c[:1] == ['shell'] and len(c) == 2 and c[1].startswith('[ -e '):
                path = c[1].split("'")[1]; return 'yes\n' if self.remote.get(path) else ''
            if c[:1] == ['shell'] and len(c) == 2 and c[1].startswith('sha256sum '): return self.remote.get('sha', '') + '  x\n'
            if c[:1] == ['shell'] and len(c) == 2 and c[1].startswith('cat ') and ba.MANIFEST in c[1]: return self.remote.get('manifest', '')
            return ''
        cm = mock.patch.object(ba, 'adb', adb); cm.__enter__(); self.addCleanup(cm.__exit__, None, None, None)
        self.game = self.tmp / 'v1'; (self.game / 'RomFS').mkdir(parents=True); (self.game / 'RomFS/a.bin').write_bytes(b'a')
        (self.game / 'control.nacp').write_bytes(b'nacp'); (self.game / 'icon.jpg').write_bytes(b'jpg')
        self.patched = self.tmp / 'patched_android_v1'; self.patched.mkdir(); (self.patched / 'main.fixups.tsv').write_text('x')
        self.font = self.tmp / 'font'; self.font.mkdir(); (self.font / 'f.ttf').write_bytes(b'f')
        (self.tmp / 'logs').mkdir()

    def push(self):
        ba.push_game('S', GID, self.game, self.patched, None, self.font, self.tmp / 'aoc', 'en-US', self.tmp, self.tmp / 'logs', {'title_id': GID})
        return [' '.join(c) for c in self.cmds]

    def test_copies_to_games_folder_and_opens_it(self):
        flat = self.push(); dst = f'{ba.REMOTE}/games/{GID}'
        self.assertTrue(any(c.startswith('push') and c.endswith(f'{dst}/') for c in flat))
        self.assertTrue(any(c.startswith('push') and c.endswith(f'{dst}/game.json') for c in flat))
        self.assertTrue(any(f"chmod -R a+rwX '{dst}' 2>&1; chmod a+rwx '{ba.REMOTE}/games' 2>&1" in c for c in flat), flat)
        writes = [c for c in flat if not c.startswith('shell [ -e ')]   # the one-game layout is only looked for
        self.assertFalse(any(f'{ba.REMOTE}/game/' in c or c.endswith(f'{ba.REMOTE}/game') for c in writes), writes)

    def test_one_game_install_of_the_same_game_is_moved(self):
        self.remote = {f'{ba.REMOTE}/game/code/main.fixups.tsv': True, 'sha': hashlib.sha256(b'nacp').hexdigest()}
        flat = self.push()
        mv = [c for c in flat if ' mv ' in c and 'legacy_save' in c]
        self.assertEqual(len(mv), 1, flat)
        self.assertIn(f"mv '{ba.REMOTE}/game' '{ba.REMOTE}/games/{GID}'", mv[0])

    def test_one_game_install_of_another_game_is_left(self):
        self.remote = {f'{ba.REMOTE}/game/code/main.fixups.tsv': True, 'sha': hashlib.sha256(b'other').hexdigest()}
        flat = self.push()
        self.assertFalse(any(' mv ' in c and 'legacy_save' in c for c in flat))
        self.assertTrue(any('another game' in s for s in self.said))

    def test_many_files_go_as_one_archive_and_only_changed_ones_again(self):
        big = self.tmp / 'spv'; big.mkdir()
        for i in range(ba.TAR_FILES + 5): (big / f'{i:04}.spv').write_bytes(b'x' * (i % 7 + 1))
        ba.push_dir('S', big, f'{ba.REMOTE}/games/{GID}', 'spv', self.tmp / 'logs' / 'p.log', sync=True)
        flat = [' '.join(c) for c in self.cmds]
        self.assertEqual(sum(c.startswith('push ') and c.endswith('/.spv.tar') for c in flat), 1, flat)
        self.assertTrue(any(f"tar -xf '{ba.REMOTE}/games/{GID}/.spv.tar' -C '{ba.REMOTE}/games/{GID}/spv'" in c for c in flat), flat)
        self.assertFalse(any(c.startswith('push ') and c.endswith('/spv') or c.endswith(f'{GID}/') for c in flat if 'manifest' not in c and '.tar' not in c), flat)
        man = (self.tmp / 'logs' / 'p.manifest').read_text(encoding='utf-8')
        self.assertEqual(len(man.splitlines()), ba.TAR_FILES + 5)
        self.cmds.clear(); self.remote['manifest'] = man   # the phone has them: nothing is sent again
        ba.push_dir('S', big, f'{ba.REMOTE}/games/{GID}', 'spv', self.tmp / 'logs' / 'p.log', sync=True)
        self.assertFalse(any(' '.join(c).endswith('.tar') for c in self.cmds if c[0] == 'push'), self.cmds)
        self.assertTrue(any('0 of' in s for s in self.said), self.said)

    def test_large_file_goes_on_from_what_the_phone_holds(self):
        import re
        files = {}; sent = []
        def q(s): return s.strip().strip("'")
        def adb(serial, *a, log=None, check=True):
            c = [str(x) for x in a]
            if c[0] == 'push':
                if len(sent) == 1 and not getattr(adb, 'cut', False): adb.cut = True; raise ba.BuildError('no response: Broken pipe')
                sent.append(c[2]); files[c[2]] = Path(c[1]).read_bytes(); return ''
            cmd = c[1]
            if m := re.fullmatch(r"stat -c %s (.+) 2>/dev/null", cmd): p = q(m[1]); return f'{len(files[p])}\n' if p in files else ''
            if m := re.fullmatch(r": > (.+)", cmd): files[q(m[1])] = b''; return ''
            if m := re.fullmatch(r"dd if=(.+) bs=4096 skip=(\d+) count=(\d+) 2>/dev/null \| head -c (\d+) \| sha256sum", cmd):
                b = files[q(m[1])][int(m[2]) * 4096:][:int(m[3]) * 4096][:int(m[4])]; return hashlib.sha256(b).hexdigest() + '  -\n'
            if m := re.fullmatch(r"cat (.+) >> (.+) && rm (.+)", cmd): files[q(m[2])] += files.pop(q(m[1])); return ''
            if m := re.fullmatch(r"mv -f (.+) (.+)", cmd): files[q(m[2])] = files.pop(q(m[1])); return ''
            raise AssertionError(c)
        big = self.tmp / 'RomFS.img'; big.write_bytes(bytes(range(256)) * 9000)   # 2.2 MB, in 1 MB pieces
        with mock.patch.object(ba, 'adb', adb), mock.patch.object(ba, 'CHUNK', 1 << 20):
            with self.assertRaises(ba.BuildError): ba.push_resumable('S', big, '/r/RomFS.img', self.tmp / 'logs' / 'p.log')   # cut after the first piece
            self.assertEqual(len(files['/r/RomFS.img.partial']), 1 << 20)
            ba.push_resumable('S', big, '/r/RomFS.img', self.tmp / 'logs' / 'p.log')
        self.assertEqual(files['/r/RomFS.img'], big.read_bytes())
        self.assertEqual(len(sent), 3)   # the first piece once, then the two left
        self.assertTrue(any('going on from there' in s for s in self.said), self.said)

    def test_install_is_an_update(self):
        apk = self.tmp / 'a.apk'; apk.write_bytes(b'apk')
        with mock.patch.object(ba, 'KEYSTORE', self.tmp / 'k.jks'):
            (self.tmp / 'k.jks').write_bytes(b'k'); ba.install('S', apk, self.tmp / 'logs')
        self.assertEqual(self.cmds[-1][:2], ['install', '-r'])
        self.assertFalse(any('uninstall' in ' '.join(c) for c in self.cmds))


if __name__ == '__main__':
    unittest.main()
