"""dm/android/rc_pend.h (conditional rendering's pending timestamp reports, gfx_vk.c rc_note / rc_value), built with the system's C compiler and called through
ctypes: however many occlusion reports come after a timestamp report, the timestamp is still known as 0 while its report is in flight, as nvn_mtl.m's table keeps
it (dm/gfx/tests/t_sbw.m); once its report has landed the record reads memory (a CPU rewrite included), as the Mac's g_rc_complete does; a full list lets landed
entries go first and keeps a record noted again; and the index answers as a walk of the list with the same rules. Skipped without a C compiler."""
import ctypes
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
_CC = shutil.which('clang') or shutil.which('cc') or shutil.which('gcc')
CC = ['xcrun', 'clang'] if sys.platform == 'darwin' and shutil.which('xcrun') else [_CC] if _CC else []   # empty: no C compiler
SRC = '''#include "rc_pend.h"
#define X __attribute__((visibility("default")))
static rc_pend g_l;
X void g_reset(void) { memset(&g_l, 0, sizeof g_l); }
X void g_note(uint64_t a, int timestamp, uint64_t seq, uint64_t landed) { rcp_note(&g_l, a, timestamp, seq, landed); }
X int g_find(uint64_t a, uint64_t landed) { return rcp_find(&g_l, a, landed); }
X int g_n(void) { return g_l.n; }
X uint64_t g_dropped(void) { return g_l.dropped; }
X uint64_t g_expired(void) { return g_l.expired; }
X int g_cap(void) { return RC_PEND; }
'''
TIMESTAMP, UNKNOWN = 1, 2
REC = 0x7100000000   # report records 16 bytes apart, as a game's are
QUERY = 0x7200000000


class Walk:   # the same rules as a plain walk of the list, for the index to answer as
    def __init__(self, cap): self.e, self.cap, self.at = [], cap, 0
    def note(self, a, ts, seq, landed):
        self.at += 1
        for x in self.e:
            if x[0] == a: x[1:] = [TIMESTAMP if ts else UNKNOWN, seq, self.at]; return
        if not ts: return
        if len(self.e) == self.cap:
            self.e = [x for x in self.e if x[2] > landed]
            if len(self.e) == self.cap: self.e = [x for x in self.e if x[3] >= sorted(y[3] for y in self.e)[self.cap // 2]]
        self.e.append([a, TIMESTAMP, seq, self.at])
    def find(self, a, landed):
        for x in self.e:
            if x[0] == a: return x[1] if x[2] > landed else 0
        return 0


@unittest.skipIf(os.name == 'nt' or not CC, 'a C compiler is needed (POSIX)')
class RcPend(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())
        (cls.tmp / 'g.c').write_text(SRC)
        subprocess.run([*CC, '-shared', '-fPIC', '-O1', '-Wall', '-Wextra', '-Werror', '-I', REPO / 'dm/android', cls.tmp / 'g.c', '-o', cls.tmp / 'g.so'], check=True)
        L = cls.lib = ctypes.CDLL(str(cls.tmp / 'g.so'))
        L.g_note.argtypes = [ctypes.c_uint64, ctypes.c_int, ctypes.c_uint64, ctypes.c_uint64]
        L.g_find.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
        L.g_dropped.restype = L.g_expired.restype = ctypes.c_uint64

    @classmethod
    def tearDownClass(cls): shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self): self.lib.g_reset(); self.seq = 0

    def note(self, a, ts, landed=0):   # each report its own serial, as post gives them
        self.seq += 1; self.lib.g_note(a, ts, self.seq, landed); return self.seq

    def find(self, a, landed=0): return self.lib.g_find(a, landed)

    def queries(self, n, base=QUERY, landed=0):
        for i in range(n): self.note(base + 16 * i, 0, landed)

    def test_a_timestamp_before_a_frame_of_occlusion_reports_is_still_known(self):   # Ocarina of Time's refraction clear: one report, then ~530 queries, then the condition
        self.note(REC, 1)
        self.queries(9000)
        self.assertEqual(self.find(REC), TIMESTAMP)
        self.assertEqual(self.lib.g_n(), 1)

    def test_a_timestamp_after_them_is_known(self):   # t_sbw.m's case
        self.queries(9000); self.note(REC, 1)
        self.assertEqual(self.find(REC), TIMESTAMP)

    def test_an_occlusion_report_takes_no_entry(self):   # the counter report address set answers for it
        self.queries(3)
        self.assertEqual(self.find(QUERY), 0); self.assertEqual(self.lib.g_n(), 0)

    def test_a_query_after_a_timestamp_to_one_address_is_unknown(self):
        self.note(REC, 1); self.note(REC, 0)
        self.assertEqual(self.find(REC), UNKNOWN)
        self.note(REC, 1)   # and a timestamp after that is known again
        self.assertEqual(self.find(REC), TIMESTAMP); self.assertEqual(self.lib.g_n(), 1)

    def test_a_record_with_no_report_has_no_entry(self):
        self.note(REC, 1)
        self.assertEqual(self.find(REC + 16), 0)

    def test_a_timestamp_whose_report_landed_reads_memory(self):   # a CPU rewrite after it (a target that needs a clear again) is then seen
        s = self.note(REC, 1)
        self.assertEqual(self.find(REC, landed=s - 1), TIMESTAMP)
        self.assertEqual(self.find(REC, landed=s), 0)
        self.queries(9000)   # the review's probe: 9,000 queries after it changed nothing before, the entry stayed known 0 for good
        self.assertEqual(self.find(REC, landed=s), 0)
        u = self.note(REC, 0)   # a query to it after: unknown while that one is in flight, then the address set's
        self.assertEqual(self.find(REC, landed=u - 1), UNKNOWN); self.assertEqual(self.find(REC, landed=u), 0)
        t = self.note(REC, 1)   # and a timestamp report again: known 0 until it lands
        self.assertEqual(self.find(REC, landed=u), TIMESTAMP); self.assertEqual(self.find(REC, landed=t), 0)

    def test_a_timestamp_noted_again_survives_a_refill(self):   # the review's probe: a per-target record of frame 1 noted again in this submission
        cap = self.lib.g_cap()
        self.note(REC, 1)
        for i in range(1, cap): self.note(REC + 16 * i, 1)   # 255 others, none landed
        self.note(REC, 1)   # the record again
        self.note(REC + 16 * cap, 1)   # one more: the list is full and nothing has landed
        self.assertEqual(self.lib.g_dropped(), cap // 2)
        self.assertEqual(self.find(REC), TIMESTAMP)   # it was the least recently noted by position, not by note
        self.assertEqual(self.find(REC + 16), 0)   # the least recently noted went
        self.assertEqual(self.find(REC + 16 * cap), TIMESTAMP)

    def test_a_full_list_lets_landed_entries_go_first(self):
        cap = self.lib.g_cap()
        for i in range(cap): self.note(REC + 16 * i, 1)
        landed = 40   # the first 40 reports have landed
        self.note(REC + 16 * cap, 1, landed)
        self.assertEqual(self.lib.g_expired(), 40); self.assertEqual(self.lib.g_dropped(), 0)
        self.assertEqual(self.lib.g_n(), cap - 40 + 1)
        for i in range(40, cap + 1): self.assertEqual(self.find(REC + 16 * i, landed), TIMESTAMP, i)

    def test_more_timestamps_in_flight_than_the_list_holds_keep_the_newest(self):
        cap = self.lib.g_cap(); n = cap + 44
        for i in range(n): self.note(REC + 16 * i, 1)
        self.assertLessEqual(self.lib.g_n(), cap)
        self.assertEqual(self.find(REC + 16 * (n - 1)), TIMESTAMP)
        self.assertEqual(self.find(REC + 16 * (n - cap // 2)), TIMESTAMP)
        self.assertEqual(self.find(REC), 0)   # the oldest went
        self.assertEqual(self.lib.g_dropped(), cap // 2)

    def test_against_the_walk(self):   # recurring report slots, colliding hashes, reports landing behind the replay, against a plain walk with the same rules
        rnd = random.Random(7); ref = Walk(self.lib.g_cap()); landed = 0
        pool = [0x40000000 + 16 * rnd.randrange(4096) for _ in range(900)] + [0x80000000 + (k << 14) for k in range(64)]
        for step in range(60000):
            a = rnd.choice(pool)
            landed = max(landed, self.seq - rnd.randrange(400))   # the completion thread a few hundred reports behind
            if rnd.random() < 0.7:
                ts = int(rnd.random() < 0.6); self.note(a, ts, landed); ref.note(a, ts, self.seq, landed)
            else:
                self.assertEqual(self.find(a, landed), ref.find(a, landed), f'step {step}')
            if step % 997 == 0: self.assertEqual(self.lib.g_n(), len(ref.e))


if __name__ == '__main__':
    unittest.main()
