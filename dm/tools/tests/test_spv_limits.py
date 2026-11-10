"""dm/android/spv_limits.h (subgroup operations rewritten out of the modules of stages where the GPU runs none: fragment shaders on the Adreno 650), built for the PC
and run on modules assembled with the NDK's spirv-as; spirv-val checks each result. Skipped without the NDK.
With DM_SPV_DIR=<a game's packed spv folder>, also run over every module there with the Adreno 650's stages (each result must pass spirv-val).
With DM_TEST_SERIAL=<adb serial>, dm/android/tests/t_quad_shuffle.c is built for Android and run on that device with a rewritten fragment shader: every pixel must read
the right lanes of its quad (the fine derivatives the rewrite relies on, on that GPU)."""
import ctypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_spv_readonly import NDK, HOST, EXE, REPO   # noqa: E402

QUAD, ONE, KEPT = 1, 2, 4   # SPV_LIM_SG_*
ADRENO_650, ADRENO_740 = 0x20, 0x30   # subgroupSupportedStages: compute; fragment and compute

# A fragment shader as the translator writes Maxwell's warp instructions: the lane ID and a mask as built-ins (Flat inputs), SHFL as the four shuffles, VOTE as a
# ballot and all / any / all-equal, a mask's component read through an access chain (Xenoblade Chronicles 3's prefix ballot). No gl_FragCoord: the rewrite adds it
FS = '''OpCapability Shader
OpCapability GroupNonUniformBallot
OpCapability GroupNonUniformShuffle
OpCapability GroupNonUniformShuffleRelative
OpCapability GroupNonUniformVote
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %lane %lt %col
OpExecutionMode %main OriginUpperLeft
OpName %lane "SubgroupLocalInvocationId"
OpDecorate %lane BuiltIn SubgroupLocalInvocationId
OpDecorate %lane Flat
OpDecorate %lt BuiltIn SubgroupLtMask
OpDecorate %lt Flat
OpDecorate %col Location 0
%void = OpTypeVoid
%fn = OpTypeFunction %void
%bool = OpTypeBool
%u32 = OpTypeInt 32 0
%f32 = OpTypeFloat 32
%v4u = OpTypeVector %u32 4
%pu = OpTypePointer Input %u32
%pv4u = OpTypePointer Input %v4u
%lane = OpVariable %pu Input
%lt = OpVariable %pv4u Input
%po = OpTypePointer Output %v4u
%col = OpVariable %po Output
%scope = OpConstant %u32 3
%u0 = OpConstant %u32 0
%u1 = OpConstant %u32 1
%t = OpConstantTrue %bool
%main = OpFunction %void None %fn
%l0 = OpLabel
%l = OpLoad %u32 %lane
%f = OpConvertUToF %f32 %l
%s0 = OpGroupNonUniformShuffle %f32 %scope %f %u1
%s1 = OpGroupNonUniformShuffleXor %f32 %scope %s0 %u1
%s2 = OpGroupNonUniformShuffleUp %f32 %scope %s1 %u1
%s3 = OpGroupNonUniformShuffleDown %f32 %scope %s2 %u1
%b = OpGroupNonUniformBallot %v4u %scope %t
%bx = OpCompositeExtract %u32 %b 0
%ltp = OpAccessChain %pu %lt %u0
%ltx = OpLoad %u32 %ltp
%m = OpBitwiseAnd %u32 %bx %ltx
%al = OpGroupNonUniformAll %bool %scope %t
%an = OpGroupNonUniformAny %bool %scope %al
%eq = OpGroupNonUniformAllEqual %bool %scope %an
%e = OpSelect %u32 %eq %u1 %u0
%r = OpBitcast %u32 %s3
%o = OpCompositeConstruct %v4u %r %m %e %l
OpStore %col %o
OpReturn
OpFunctionEnd
'''
VS = FS.replace('OpEntryPoint Fragment', 'OpEntryPoint Vertex').replace('OpExecutionMode %main OriginUpperLeft\n', '').replace('OpDecorate %lane Flat\n', '').replace('OpDecorate %lt Flat\n', '')

# For the device: a full-screen triangle, and a fragment shader whose four outputs are Shuffle 0, Shuffle 3, ShuffleXor 1 and ShuffleXor 2 of a hash of gl_FragCoord
# (any 32 bits, NaN patterns included); t_quad_shuffle.c computes what each pixel must get
DEV_VS = '''OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main" %vid %pos
OpDecorate %vid BuiltIn VertexIndex
OpDecorate %pos BuiltIn Position
%void = OpTypeVoid
%fn = OpTypeFunction %void
%f32 = OpTypeFloat 32
%v4 = OpTypeVector %f32 4
%i32 = OpTypeInt 32 1
%bool = OpTypeBool
%pi = OpTypePointer Input %i32
%vid = OpVariable %pi Input
%po = OpTypePointer Output %v4
%pos = OpVariable %po Output
%i1 = OpConstant %i32 1
%i2 = OpConstant %i32 2
%f3 = OpConstant %f32 3
%fm1 = OpConstant %f32 -1
%f0 = OpConstant %f32 0
%f1 = OpConstant %f32 1
%main = OpFunction %void None %fn
%l = OpLabel
%v = OpLoad %i32 %vid
%is1 = OpIEqual %bool %v %i1
%is2 = OpIEqual %bool %v %i2
%x = OpSelect %f32 %is1 %f3 %fm1
%y = OpSelect %f32 %is2 %f3 %fm1
%p = OpCompositeConstruct %v4 %x %y %f0 %f1
OpStore %pos %p
OpReturn
OpFunctionEnd
'''
DEV_FS = '''OpCapability Shader
OpCapability GroupNonUniform
OpCapability GroupNonUniformShuffle
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %fc %out
OpExecutionMode %main OriginUpperLeft
OpDecorate %fc BuiltIn FragCoord
OpDecorate %out Location 0
%void = OpTypeVoid
%fn = OpTypeFunction %void
%f32 = OpTypeFloat 32
%v4 = OpTypeVector %f32 4
%u32 = OpTypeInt 32 0
%v4u = OpTypeVector %u32 4
%pi = OpTypePointer Input %v4
%fc = OpVariable %pi Input
%po = OpTypePointer Output %v4u
%out = OpVariable %po Output
%scope = OpConstant %u32 3
%u0 = OpConstant %u32 0
%u1 = OpConstant %u32 1
%u2 = OpConstant %u32 2
%u3 = OpConstant %u32 3
%k1 = OpConstant %u32 2654435761
%k2 = OpConstant %u32 2246822519
%k3 = OpConstant %u32 739982445
%s15 = OpConstant %u32 15
%s12 = OpConstant %u32 12
%main = OpFunction %void None %fn
%l = OpLabel
%c = OpLoad %v4 %fc
%cx = OpCompositeExtract %f32 %c 0
%cy = OpCompositeExtract %f32 %c 1
%x = OpConvertFToU %u32 %cx
%y = OpConvertFToU %u32 %cy
%a = OpIMul %u32 %x %k1
%b = OpIMul %u32 %y %k2
%h0 = OpBitwiseXor %u32 %a %b
%t1 = OpShiftRightLogical %u32 %h0 %s15
%h1 = OpBitwiseXor %u32 %h0 %t1
%h2 = OpIMul %u32 %h1 %k3
%t2 = OpShiftRightLogical %u32 %h2 %s12
%h = OpBitwiseXor %u32 %h2 %t2
%f = OpBitcast %f32 %h
%r0 = OpGroupNonUniformShuffle %f32 %scope %f %u0
%r1 = OpGroupNonUniformShuffle %f32 %scope %f %u3
%r2 = OpGroupNonUniformShuffleXor %f32 %scope %f %u1
%r3 = OpGroupNonUniformShuffleXor %f32 %scope %f %u2
%o0 = OpBitcast %u32 %r0
%o1 = OpBitcast %u32 %r1
%o2 = OpBitcast %u32 %r2
%o3 = OpBitcast %u32 %r3
%o = OpCompositeConstruct %v4u %o0 %o1 %o2 %o3
OpStore %out %o
OpReturn
OpFunctionEnd
'''


def instructions(data):
    w, i = struct.unpack(f'<{len(data) // 4}I', data), 5
    while i < len(w):
        yield w[i] & 0xffff, w[i + 1:i + (w[i] >> 16)]
        i += w[i] >> 16


def caps(data): return {x[0] for op, x in instructions(data) if op == 17}
def builtins(data): return {x[2] for op, x in instructions(data) if op == 71 and len(x) >= 3 and x[1] == 11}
def subgroup_ops(data): return [op for op, _ in instructions(data) if 333 <= op <= 366]
def interface(data): return next(x for op, x in instructions(data) if op == 15)


@unittest.skipIf(NDK is None, 'no Android NDK')
class SpvLimits(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert NDK is not None
        cls.tmp = Path(tempfile.mkdtemp())
        tools = NDK / 'shader-tools' / HOST; cls.spv_as, cls.spv_val = tools / f'spirv-as{EXE}', tools / f'spirv-val{EXE}'
        cls.bin = NDK / 'toolchains/llvm/prebuilt' / HOST / 'bin'
        src = cls.tmp / 'l.c'
        export = '__declspec(dllexport)' if os.name == 'nt' else '__attribute__((visibility("default")))'
        src.write_text(f'#include "spv_limits.h"\n{export} size_t run(const uint32_t *w, size_t n, uint32_t sg, uint32_t *s, uint32_t *o, size_t cap, size_t *need, int *what) '
                       f'{{ return spv_limits(w, n, sg, s, o, cap, need, what); }}\n'
                       f'{export} int wanted(const uint32_t *w, size_t n, uint32_t sg) {{ return spv_lim_wanted(w, n, sg); }}\n')
        env = dict(os.environ, MSYS_NO_PATHCONV='1')
        if os.name == 'nt':   # no C library: a DLL linked by lld alone
            subprocess.run([cls.bin / 'clang.exe', '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-nostdlib', '-O1', '-Wall', '-Werror', '-I', REPO / 'dm/android', '-c', src,
                            '-o', cls.tmp / 'l.obj'], check=True)
            subprocess.run([cls.bin / 'ld.lld.exe', '-flavor', 'link', '-dll', '-noentry', f'-out:{cls.tmp / "l.dll"}', str(cls.tmp / 'l.obj')], check=True, env=env)
            cls.lib = ctypes.CDLL(str(cls.tmp / 'l.dll'))
        else:
            # the system's clang: on a Mac the NDK's is an x86_64 build that makes x86_64 libraries, without the macOS SDK to link them; on Linux it links with the
            # system's C runtime only if it finds it
            cc = ['xcrun', 'clang'] if sys.platform == 'darwin' else [shutil.which('clang') or cls.bin / 'clang']
            subprocess.run([*cc, '-shared', '-fPIC', '-O1', '-Wall', '-Wextra', '-Werror', '-I', REPO / 'dm/android', src, '-o', cls.tmp / 'l.so'], check=True)
            cls.lib = ctypes.CDLL(str(cls.tmp / 'l.so'))
        cls.lib.run.restype = ctypes.c_size_t

    def run_on(self, data, stages=ADRENO_650):   # → (the rewritten module or None, SPV_LIM_* bits)
        n = len(data) // 4; w = (ctypes.c_uint32 * n).from_buffer_copy(data)
        s = (ctypes.c_uint32 * (2 * w[3]))(); need = ctypes.c_size_t(); what = ctypes.c_int()
        self.assertEqual(self.lib.run(w, n, stages, s, None, 0, ctypes.byref(need), ctypes.byref(what)), 0)
        if need.value or what.value: self.assertTrue(self.lib.wanted(w, n, stages))   # the replay's pre-check never passes over a module that needs it
        if not need.value: return None, what.value
        o = (ctypes.c_uint32 * need.value)()
        k = self.lib.run(w, n, stages, s, o, need.value, ctypes.byref(need), ctypes.byref(what))
        self.assertTrue(k)
        return bytes(o)[:4 * k], what.value

    def val(self, data, name):
        p = self.tmp / name; p.write_bytes(data)
        r = subprocess.run([self.spv_val, '--target-env', 'vulkan1.1', p], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def asm(self, text, name):
        p = self.tmp / f'{name}.spvasm'; p.write_text(text)
        subprocess.run([self.spv_as, '--target-env', 'vulkan1.1', p, '-o', self.tmp / f'{name}.spv'], check=True)
        data = (self.tmp / f'{name}.spv').read_bytes(); self.val(data, f'{name}_in.spv')
        return data

    def test_fragment_shader_as_a_quad(self):
        out, what = self.run_on(self.asm(FS, 'fs'))
        self.assertEqual(what, QUAD)
        self.val(out, 'fs_out.spv')
        self.assertEqual(subgroup_ops(out), [])
        self.assertEqual(caps(out), {1, 51})   # Shader and DerivativeControl: the subgroup capabilities went
        self.assertEqual(builtins(out), {15})   # gl_FragCoord in, the lane ID and the mask out
        frag = next(x[0] for op, x in instructions(out) if op == 71 and x[1:3] == (11, 15))
        self.assertIn(frag, interface(out))
        self.assertEqual(sum(op in (210, 211) for op, _ in instructions(out)), 4 * 6)   # 4 shuffles, 2 halves, DPdxFine + 2 DPdyFine

    def test_gpu_with_fragment_subgroups_leaves_it(self):
        data = self.asm(FS, 'fs_740')
        self.assertEqual(self.run_on(data, ADRENO_740), (None, 0))
        self.assertEqual(self.run_on(data, 0x3f), (None, 0))
        w = (ctypes.c_uint32 * (len(data) // 4)).from_buffer_copy(data)
        self.assertEqual((self.lib.wanted(w, len(w), ADRENO_740), self.lib.wanted(w, len(w), ADRENO_650)), (0, 1))

    def test_vertex_shader_as_one_lane(self):   # the Adreno 740 and 830 run none in vertex shaders (Tears of the Kingdom's visibility counter)
        out, what = self.run_on(self.asm(VS, 'vs'), ADRENO_740)
        self.assertEqual(what, ONE)
        self.val(out, 'vs_out.spv')
        self.assertEqual(subgroup_ops(out), [])
        self.assertEqual(caps(out), {1})
        self.assertEqual(builtins(out), set())
        self.assertEqual(self.run_on(self.asm(VS, 'vs_all'), 0x3f), (None, 0))

    def test_module_without_subgroup_use_is_left(self):   # every translated module declares the capabilities: one that uses none stays as it is
        text = FS[:FS.index('%l = OpLoad')] + 'OpStore %col %o\nOpReturn\nOpFunctionEnd\n'
        text = text.replace('%t = OpConstantTrue %bool\n', '%t = OpConstantTrue %bool\n%o = OpConstantComposite %v4u %u0 %u0 %u0 %u0\n')
        self.assertEqual(self.run_on(self.asm(text, 'fs_none')), (None, 0))

    def test_operation_not_covered_is_left(self):
        text = FS.replace('OpCapability GroupNonUniformVote\n', 'OpCapability GroupNonUniformVote\nOpCapability GroupNonUniformArithmetic\n') \
                 .replace('%f = OpConvertUToF %f32 %l\n', '%sum = OpGroupNonUniformIAdd %u32 %scope Reduce %l\n%f = OpConvertUToF %f32 %sum\n')
        self.assertEqual(self.run_on(self.asm(text, 'fs_add')), (None, KEPT))

    def test_gl_fragcoord_already_there(self):
        out, what = self.run_on(self.asm(DEV_FS, 'dev_fs'))
        self.assertEqual(what, QUAD)
        self.val(out, 'dev_fs_out.spv')
        self.assertEqual(builtins(out), {15})
        self.assertEqual(list(interface(out)).count(next(x[0] for op, x in instructions(out) if op == 71 and x[1:3] == (11, 15))), 1)

    def valid(self, data, name):
        p = self.tmp / name; p.write_bytes(data)
        return subprocess.run([self.spv_val, '--target-env', 'vulkan1.1', p], capture_output=True).returncode == 0

    @unittest.skipUnless(os.environ.get('DM_SPV_DIR'), 'DM_SPV_DIR not set')
    def test_a_games_shaders(self):   # each rewritten module that spirv-val takes as translated must pass after
        d = Path(os.environ['DM_SPV_DIR']); n = invalid = 0; got: dict[int, int] = {}
        for sp in sorted(d.glob('*.spv')):
            n += 1; data = sp.read_bytes(); out, what = self.run_on(data)
            got[what] = got.get(what, 0) + 1
            if not out: continue
            if not self.valid(data, 'game_in.spv'): invalid += 1; continue
            self.val(out, f'game_{sp.stem}.spv')
        print(f'{n} modules; by what was done (SPV_LIM_SG_* bits): {dict(sorted(got.items()))}; {invalid} rewritten ones invalid as translated, not checked')
        self.assertTrue(n)

    @unittest.skipUnless(os.environ.get('DM_TEST_SERIAL'), 'DM_TEST_SERIAL not set')
    def test_quad_exchange_on_a_device(self):
        serial = os.environ['DM_TEST_SERIAL']
        adb = shutil.which('adb') or str(Path(os.environ.get('ANDROID_HOME', '')) / 'platform-tools' / f'adb{EXE}')
        exe = self.tmp / 't_quad_shuffle'
        cc = self.bin / ('aarch64-linux-android29-clang' + ('.cmd' if os.name == 'nt' else ''))
        subprocess.run([cc, '-O1', '-Wall', '-Werror', REPO / 'dm/android/tests/t_quad_shuffle.c', '-lvulkan', '-o', exe], check=True)
        out, what = self.run_on(self.asm(DEV_FS, 'dev_fs2')); self.assertEqual(what, QUAD); assert out is not None
        (self.tmp / 'fs.spv').write_bytes(out); (self.tmp / 'vs.spv').write_bytes(self.asm(DEV_VS, 'dev_vs'))
        d = '/data/local/tmp/dm_quad_shuffle'
        subprocess.run([adb, '-s', serial, 'shell', 'mkdir', '-p', d], check=True)
        for f in (exe, self.tmp / 'vs.spv', self.tmp / 'fs.spv'): subprocess.run([adb, '-s', serial, 'push', f, d], check=True, capture_output=True)
        r = subprocess.run([adb, '-s', serial, 'shell', f'cd {d} && chmod 755 t_quad_shuffle && ./t_quad_shuffle vs.spv fs.spv; echo "exit $?"'], capture_output=True, text=True)
        subprocess.run([adb, '-s', serial, 'shell', 'rm', '-r', d], capture_output=True)
        print(r.stdout.strip())
        self.assertTrue(r.stdout.strip().endswith('exit 0'), r.stdout + r.stderr)


if __name__ == '__main__':
    unittest.main()
