"""scripts/android_oodle.py: the batching of the game's Oodle decodes into phone jobs. Rounds answers uepak.py's decode(compressed, raw length) from what the phone decoded so far, records
the rest, and runs again until nothing new is asked: a stream made from an earlier round's zeros is never sent (it does not start as Oodle's streams do), so a two-layer container
(blocks, then the shader groups inside them) settles in two rounds. The job file and its output keep hle_oodle.c's formats. Oodle stands in here as a header byte and zlib"""
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'scripts'))
import android_oodle as ao   # noqa: E402  # pyright: ignore[reportMissingImports]  (scripts/, put on the path here)


def stream(raw):   # an "Oodle" stream: a header byte whose low six bits are 0x0C, then zlib
    return b'\x8c' + zlib.compress(raw)


class FakePhone:
    def __init__(self): self.jobs = []
    def __call__(self, records):
        self.jobs.append(len(records))
        out = []
        for c, n in records:
            try: d = zlib.decompress(c[1:]) if ao.oodle_head(c) else None
            except zlib.error: d = None   # the job's record fails (decoded length 0), as hle_oodle.c reports one
            out.append(d if d is not None and len(d) == n else None)
        return out


class RoundsTest(unittest.TestCase):
    def test_two_layers_settle_in_two_rounds(self):
        inner = [b'shader %d ' % i * 50 for i in range(3)]
        outer = [stream(stream(x)) for x in inner]   # a block holding a compressed group
        sizes = [len(stream(x)) for x in inner]
        r, phone = ao.Rounds(), FakePhone()
        def fn():   # as io_archive: each block decoded, then the group inside it
            return [r(r(o, s), len(x)) for o, s, x in zip(outer, sizes, inner)]
        res, rounds = r.run(fn, phone)
        self.assertEqual(res, inner)
        self.assertEqual(rounds, 2)
        self.assertEqual(phone.jobs, [3, 3])   # the blocks, then the groups: no zeros sent

    def test_a_damaged_file_still_raises(self):
        r = ao.Rounds()
        def fn(): raise ValueError('damaged')
        with self.assertRaises(ValueError): r.run(fn, FakePhone())

    def test_parse_errors_on_zeros_are_rounds(self):   # uepak's checks fail on a round's zeros; the next round has the real bytes
        r, phone = ao.Rounds(), FakePhone()
        head = struct.pack('<I', 1) + b'table'
        def fn():
            d = r(stream(head), len(head))
            if struct.unpack_from('<I', d)[0] != 1: raise ValueError('not version 1')
            return d
        res, rounds = r.run(fn, phone)
        self.assertEqual((res, rounds), (head, 1))

    def test_a_failed_stream_is_named(self):
        r = ao.Rounds()
        with self.assertRaises(ValueError) as e: r.run(lambda: r(b'\x8c' + b'not zlib', 10), FakePhone())
        self.assertIn('failed on a', str(e.exception))


class FormatTest(unittest.TestCase):
    def test_job_and_output(self):
        recs = [(stream(b'abc'), 3), (stream(b'x' * 100), 100)]
        b = ao.job_bytes(recs)
        self.assertEqual(b[:8], b'DMOJ' + struct.pack('<I', 2))
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'job.out'
            p.write_bytes(b'DMOR' + struct.pack('<I', 2) + struct.pack('<I', 3) + b'abc' + struct.pack('<I', 0))
            self.assertEqual(ao.read_out(p, 2), [b'abc', None])
            with self.assertRaises(ValueError): ao.read_out(p, 3)

    def test_oodle_head(self):
        for b in (0x0C, 0x4C, 0x8C, 0xCC): self.assertTrue(ao.oodle_head(bytes([b, 0x0A])))
        for b in (0x00, 0x1C, 0x8D, 0x78): self.assertFalse(ao.oodle_head(bytes([b, 0x0A])))


if __name__ == '__main__':
    unittest.main()
