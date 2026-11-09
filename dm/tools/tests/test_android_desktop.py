"""DroidPlay for Windows' first run, as the desktop app (scripts/nxnative_android.py) shows it, from its Qt-free helpers in dm/android/desktop: the "Get started"
checklist's four steps worked out from what the computer has (first_run.py), and Install tools' one bar across --setup's stages. No PySide6, phone or network needed."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'dm/android/desktop'))
import first_run as fr   # noqa: E402  # pyright: ignore[reportMissingImports]  (dm/android/desktop, put on the path above)

ROWS_OK = [{'name': n, 'ok': True, 'detail': '', 'fix': ''} for n in ('Python module lz4', '.NET SDK 10', 'Android SDK', 'NDK', 'CMake', 'build-tools 36.0.0',
                                                                       'platform android-35', 'adb', 'JDK keytool', 'JDK javac', 'signing key', 'game hosts')]


def rows(*missing): return [dict(r, ok=r['name'] not in missing) for r in ROWS_OK]


class Keys(unittest.TestCase):
    def setUp(self): self.tmp = Path(tempfile.mkdtemp()); self.addCleanup(shutil.rmtree, self.tmp)

    def test_none_picked_asks_for_them(self):
        s = fr.keys_step('')
        self.assertEqual((s.state, s.button), (fr.TODO, 'Choose prod.keys…'))

    def test_prod_keys_are_done(self):
        f = self.tmp / 'prod.keys'; f.write_text('header_key = 00\nmaster_key_00 = 11\n')
        s = fr.keys_step(str(f))
        self.assertTrue(s.done); self.assertIn('prod.keys', s.text)

    def test_title_keys_are_a_problem(self):   # Lockpick_RCM writes both: title.keys holds rights IDs and their keys, which can't open a game alone
        f = self.tmp / 'title.keys'; f.write_text('01006f8002326000000000000000000a = 0123456789abcdef0123456789abcdef\n')
        s = fr.keys_step(str(f))
        self.assertEqual(s.state, fr.PROBLEM); self.assertIn('not title.keys', s.text)

    def test_a_file_since_moved_is_a_problem(self):
        s = fr.keys_step(str(self.tmp / 'gone.keys'))
        self.assertEqual(s.state, fr.PROBLEM); self.assertIn("isn't there any more", s.text)


class Tools(unittest.TestCase):
    def test_looking_while_check_runs(self):
        self.assertEqual(fr.tools_step(None, True).state, fr.WORKING)

    def test_all_there_is_done(self):
        self.assertTrue(fr.tools_step(rows(), True).done)

    def test_a_new_pc_installs_all_three(self):
        s = fr.tools_step(rows('.NET SDK 10', 'Android SDK', 'adb', 'JDK keytool', 'JDK javac'), True)
        self.assertEqual((s.state, s.button), (fr.TODO, 'Install tools…')); self.assertIn('1.5 GB', s.text)

    def test_only_what_is_missing_is_named(self):
        s = fr.tools_step(rows('NDK'), True)
        self.assertIn('Still to install: the Android SDK.', s.text); self.assertNotIn('Java', s.text.split('Still')[1])

    def test_elsewhere_settings_says_how(self):   # --setup installs on Windows alone
        s = fr.tools_step(rows('.NET SDK 10'), False)
        self.assertEqual(s.button, 'Open Settings'); self.assertIn('Settings says how', s.text)

    def test_one_bar_while_setup_runs(self):
        s = fr.tools_step(rows('NDK'), True, ('running', 0.4, 'Installing the Android SDK · the NDK (4 of 5)'))
        self.assertEqual((s.state, s.fraction), (fr.WORKING, 0.4))

    def test_a_stopped_setup_says_why_and_tries_again(self):
        s = fr.tools_step(rows('NDK'), True, ('failed', 'the download does not match its sha1'))
        self.assertEqual((s.state, s.button), (fr.PROBLEM, 'Try again')); self.assertIn('does not match', s.text)

    def test_a_check_that_failed_looks_again(self):
        s = fr.tools_step([{'name': 'check', 'ok': False, 'detail': 'boom', 'fix': ''}], True)
        self.assertEqual((s.state, s.button), (fr.PROBLEM, 'Look again'))


class Phone(unittest.TestCase):
    S25 = {'serial': 'R5C0', 'state': 'device', 'name': 'Galaxy S25 Edge', 'model': 'Galaxy S25 Edge', 'android': '16'}

    def test_no_adb_points_at_the_tools(self):
        s = fr.phone_step([], False)
        self.assertEqual(s.state, fr.TODO); self.assertIn('step 2', s.text)

    def test_connected_is_done_with_its_name(self):
        s = fr.phone_step([self.S25], True)
        self.assertTrue(s.done); self.assertIn('Galaxy S25 Edge', s.text); self.assertIn('Android 16', s.text)

    def test_one_waiting_for_allow_says_what_to_tap(self):   # adb lists it with no model until it allows this PC
        s = fr.phone_step([{'serial': 'R5C0', 'state': 'unauthorized', 'model': ''}], True)
        self.assertEqual(s.state, fr.WAITING); self.assertIn('Your phone', s.text); self.assertIn('Allow', s.text)

    def test_offline_says_to_plug_it_again(self):
        s = fr.phone_step([dict(self.S25, state='offline')], True)
        self.assertEqual(s.state, fr.PROBLEM); self.assertIn('plug it back in', s.text)

    def test_one_set_up_before_stays_done_unplugged(self):   # the step can't come undone each time a phone is unplugged, or the checklist would return at every launch
        self.assertTrue(fr.phone_step([], True, seen=True).done)
        self.assertEqual(fr.phone_step([], True).state, fr.TODO)

    def test_the_picked_phone_first(self):
        pad = dict(self.S25, serial='PAD', name='OnePlus Pad 3')
        self.assertIn('OnePlus Pad 3', fr.phone_step([self.S25, pad], True, picked='PAD').text)


class Game(unittest.TestCase):
    BD = {'folder': 'D:/Games/BD', 'name': 'Pokémon Brilliant Diamond', 'state': 'build', 'tested': True, 'error': None}

    def test_none_yet_asks_for_a_folder(self):
        s = fr.game_step([], [], False)
        self.assertEqual((s.state, s.button), (fr.TODO, 'Add a game…'))

    def test_reading_names_the_folder(self):
        s = fr.game_step(['D:/Games/BD'], [], True)
        self.assertEqual((s.state, s.fraction), (fr.WORKING, -1)); self.assertIn('BD', s.text)

    def test_read_is_done_and_builds_next(self):
        s = fr.game_step(['D:/Games/BD'], [self.BD], False)
        self.assertTrue(s.done); self.assertEqual((s.button, s.ref), ('Build and install', 'D:/Games/BD')); self.assertIn('played on Android', s.text)

    def test_waiting_for_tools_isnt_called_unreadable(self):   # a supported game read before the tools came read as "Not supported" (droidplay-core #55)
        s = fr.game_step(['D:/Games/BD'], [dict(self.BD, state='tools')], False)
        self.assertEqual(s.state, fr.TODO); self.assertIn('once the tools are installed', s.text)

    def test_unreadable_says_why(self):
        s = fr.game_step(['D:/x'], [dict(self.BD, name='x', state='none', error='no NSP or XCI in it')], False)
        self.assertEqual(s.state, fr.PROBLEM); self.assertIn('no NSP or XCI', s.text)

    def test_names_are_escaped(self):
        self.assertIn('&lt;b&gt;', fr.game_step(['D:/x'], [dict(self.BD, name='<b>')], False).text)


class SetupBar(unittest.TestCase):   # Install tools: one bar across the parts --setup installs, rather than one per stage that starts again from nothing
    def run_events(self, *evs, t0=0.0):
        p = fr.SetupProgress(t0)
        for e in evs: p.feed(e)
        return p

    def test_a_new_pc_runs_all_four_weighted_by_size(self):
        p = self.run_events({'ev': 'stage', 'k': 1, 'n': 4, 'name': 'JDK 21'}, {'ev': 'progress', 'k': 1, 'step': 'downloading the JDK', 'done': 95 << 20, 'total': 190 << 20, 'unit': 'B'})
        self.assertEqual(p.planned(), [s[0] for s in fr.SETUP_STAGES])
        self.assertAlmostEqual(p.fraction(), 95 / 1580, places=3)
        self.assertTrue(p.text(5).startswith('Downloading Java · 95 MB of 190 MB'))

    def test_later_stages_count_what_came_before(self):
        p = self.run_events({'ev': 'stage', 'k': 1, 'n': 2, 'name': '.NET SDK 10'}, {'ev': 'stage_end', 'k': 1},
                            {'ev': 'stage', 'k': 2, 'n': 2, 'name': 'Android SDK packages'}, {'ev': 'progress', 'k': 2, 'step': 'installing ndk;28.2.13676358', 'done': 3, 'total': 5, 'unit': ''})
        self.assertEqual(p.planned(), ['.NET SDK 10', 'Android SDK packages'])
        self.assertAlmostEqual(p.fraction(), (290 + 950 * 130 / 850) / 1240, places=3)   # the NDK, most of the stage, still to come
        self.assertIn('Installing the Android SDK · the NDK (4 of 5)', p.text(5))

    def test_unplanned_stages_are_the_last_ones(self):   # a PC with the command-line tools already: n=2 after the JDK means the packages alone are left
        p = self.run_events({'ev': 'stage', 'k': 1, 'n': 2, 'name': 'JDK 21'})
        self.assertEqual(p.planned(), ['JDK 21', 'Android SDK packages'])

    def test_time_left_once_there_is_a_rate(self):
        p = self.run_events({'ev': 'stage', 'k': 1, 'n': 1, 'name': 'JDK 21'}, {'ev': 'progress', 'k': 1, 'step': 'downloading the JDK', 'done': 50, 'total': 100, 'unit': 'B'})
        self.assertNotIn('left', p.text(5))   # too soon to say
        self.assertIn('about 4 minutes left', p.text(240))
        self.assertIn('less than a minute left', p.text(40))


class Count(unittest.TestCase):
    def test_counts_the_done_steps(self):
        steps = [fr.keys_step(''), fr.tools_step(rows(), True), fr.phone_step([], True, seen=True), fr.game_step([], [], False)]
        self.assertEqual(fr.ready_count(steps), 2)


if __name__ == '__main__':
    unittest.main()
