"""The uniform slots' size table (c25, DMSH_CBSIZE_BANK) taken out of the translator's Android SPIR-V: dm/android/tools/spv_android.py cbsize_strip, on modules assembled
with spirv-as in the form the translator writes a bounded uniform read (Instructions.cs TryGenerateBoundedUniformLoad: a fixed offset and a computed one), and its
main(), which also drops the table from res.json. Checked with spirv-val where there is one. Skipped without spirv-as (the NDK's shader-tools, else one on PATH)."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'dm/android/tools'))
import spv_android as sa   # noqa: E402  # pyright: ignore[reportMissingImports]  # dm/android/tools, put on the path above
from test_spv_readonly import NDK, HOST, EXE   # noqa: E402


def tool(name):
    return (NDK / 'shader-tools' / HOST / f'{name}{EXE}') if NDK and (NDK / 'shader-tools' / HOST / f'{name}{EXE}').exists() else shutil.which(name)


SPV_AS, SPV_VAL = tool('spirv-as'), tool('spirv-val')

# a vertex shader reading uniform slot c4 (binding 1, vec4[4096] as the translator declares it) bounded by the size table (binding 2, uvec4[4]): word 68 (c4[17].x) at a
# fixed offset, and the word the input's bits give at a computed one, as the translator writes them; %USE% is more code after them
MODULE = '''OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %main "main" %in %out %pos
OpName %c25 "vp_c25"
OpDecorate %in Location 0
OpDecorate %out Location 0
OpDecorate %pos BuiltIn Position
OpDecorate %arr ArrayStride 16
OpDecorate %blk Block
OpMemberDecorate %blk 0 Offset 0
OpDecorate %c4 DescriptorSet 0
OpDecorate %c4 Binding 1
OpDecorate %arr4 ArrayStride 16
OpDecorate %blk4 Block
OpMemberDecorate %blk4 0 Offset 0
OpDecorate %c25 DescriptorSet 0
OpDecorate %c25 Binding 2
%void = OpTypeVoid
%fn = OpTypeFunction %void
%f32 = OpTypeFloat 32
%v4 = OpTypeVector %f32 4
%i32 = OpTypeInt 32 1
%u32 = OpTypeInt 32 0
%uv4 = OpTypeVector %u32 4
%bool = OpTypeBool
%n4096 = OpConstant %u32 4096
%n4 = OpConstant %u32 4
%arr = OpTypeArray %v4 %n4096
%blk = OpTypeStruct %arr
%pblk = OpTypePointer Uniform %blk
%c4 = OpVariable %pblk Uniform
%arr4 = OpTypeArray %uv4 %n4
%blk4 = OpTypeStruct %arr4
%pblk4 = OpTypePointer Uniform %blk4
%c25 = OpVariable %pblk4 Uniform
%pu = OpTypePointer Uniform %u32
%pf = OpTypePointer Uniform %f32
%pin = OpTypePointer Input %f32
%in = OpVariable %pin Input
%pout = OpTypePointer Output %v4
%out = OpVariable %pout Output
%pos = OpVariable %pout Output
%i0 = OpConstant %i32 0
%i1 = OpConstant %i32 1
%i3 = OpConstant %i32 3
%i17 = OpConstant %i32 17
%u2 = OpConstant %u32 2
%u68 = OpConstant %u32 68
%f0 = OpConstant %f32 0
%main = OpFunction %void None %fn
%entry = OpLabel
%sp = OpAccessChain %pu %c25 %i0 %i0 %i1
%s = OpLoad %u32 %sp
%sw = OpShiftRightLogical %u32 %s %u2
%ok = OpULessThan %bool %u68 %sw
%fp = OpAccessChain %pf %c4 %i0 %i17 %i0
%fv = OpLoad %f32 %fp
%fixed = OpSelect %f32 %ok %fv %f0
%x = OpLoad %f32 %in
%xb = OpBitcast %i32 %x
%vec = OpShiftRightLogical %i32 %xb %i3
%elem = OpBitwiseAnd %i32 %xb %i3
%sp2 = OpAccessChain %pu %c25 %i0 %i0 %i1
%s2 = OpLoad %u32 %sp2
%sw2 = OpShiftRightLogical %u32 %s2 %u2
%vu = OpBitcast %u32 %vec
%vs = OpShiftLeftLogical %u32 %vu %u2
%eu = OpBitcast %u32 %elem
%word = OpBitwiseOr %u32 %vs %eu
%ok2 = OpULessThan %bool %word %sw2
%row = OpSelect %i32 %ok2 %vec %i0
%cp = OpAccessChain %pf %c4 %i0 %row %elem
%cv = OpLoad %f32 %cp
%computed = OpSelect %f32 %ok2 %cv %f0
%o = OpCompositeConstruct %v4 %fixed %computed %f0 %f0
OpStore %out %o
OpStore %pos %o
%USE%
OpReturn
OpFunctionEnd
'''


@unittest.skipIf(SPV_AS is None, 'no spirv-as')
class CbsizeStrip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def asm(self, use=''):
        p = self.tmp / 't.spvasm'; p.write_text(MODULE.replace('%USE%', use))
        subprocess.run([str(SPV_AS), '--target-env', 'vulkan1.1', '--preserve-numeric-ids', str(p), '-o', str(self.tmp / 't.spv')], check=True)
        return (self.tmp / 't.spv').read_bytes()

    def valid(self, data):
        if not SPV_VAL: return
        (self.tmp / 'v.spv').write_bytes(data)
        r = subprocess.run([str(SPV_VAL), '--target-env', 'vulkan1.1', str(self.tmp / 'v.spv')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_reads_lose_the_table_and_their_bound_tests(self):
        hdr, ins = sa.parse(self.asm())
        var = next(x[1] for x in ins if x[0] == sa.OP_DECORATE and x[2] == sa.DEC_BINDING and x[3] == 2)
        hdr, ins, done = sa.cbsize_strip(hdr, ins, 2)
        self.assertIs(done, True)
        self.assertFalse([x for x in ins if (x[0] in (sa.OP_DECORATE, sa.OP_NAME) and x[1] == var) or (x[0] == sa.OP_VARIABLE and x[2] == var)], 'the table is still there')
        self.assertFalse([x for x in ins if x[0] == sa.OP_ULESSTHAN], 'a bound test is left')
        ctrue = {x[2] for x in ins if x[0] == sa.OP_CONSTANTTRUE}
        sels = [x for x in ins if x[0] == sa.OP_SELECT]
        self.assertEqual(len(sels), 3)
        conds = {x[2]: x[3] for x in ins if x[0] == sa.OP_COPYOBJECT}
        self.assertTrue(all(conds.get(x[3]) in ctrue for x in sels), 'a select does not take the constant true')   # each select now picks the read: the old form's
        self.valid(sa.emit(hdr, ins))

    def test_other_use_of_the_table_is_left(self):   # a chain stored as a value would not be the translator's: the module is left as it is
        data = self.asm('%cp3 = OpAccessChain %pu %c25 %i0 %i1 %i1\n%cl = OpCopyObject %pu %cp3')
        hdr, ins = sa.parse(data)
        h2, i2, done = sa.cbsize_strip(hdr, ins, 2)
        self.assertIs(done, False)
        self.assertEqual(sa.emit(h2, i2), data)

    def test_no_table(self):
        hdr, ins = sa.parse(self.asm())
        h2, i2, done = sa.cbsize_strip(hdr, ins, 7)
        self.assertIsNone(done)
        self.assertEqual(i2, ins)

    def test_main_drops_it_from_res_json_and_the_variants(self):
        src, dst = self.tmp / 'src', self.tmp / 'dst'; src.mkdir(exist_ok=True)
        (src / 'k.spv').write_bytes(self.asm())
        (src / 'k.res.json').write_text(json.dumps({'key': 'k', 'stage': 'vs', 'cbufs': [{'binding': 1, 'slot': 4}, {'binding': 2, 'slot': 25}]}))
        subprocess.run([sys.executable, str(REPO / 'dm/android/tools/spv_android.py'), str(src), str(dst)], check=True, capture_output=True)
        res = json.loads((dst / 'k.res.json').read_text())
        self.assertEqual([c['slot'] for c in res['cbufs']], [4])
        self.assertEqual(res['android'], sa.ANDROID_CHAIN)
        self.assertEqual(res['cbufs'][0]['ext'], 0)   # the computed offset can be any word of c4
        for f in ('k.spv', 'k.z.spv'):
            ins = sa.parse((dst / f).read_bytes())[1]
            self.assertFalse([x for x in ins if x[0] == sa.OP_DECORATE and x[2] == sa.DEC_BINDING and x[3] == 2], f)
            self.valid((dst / f).read_bytes())


if __name__ == '__main__':
    unittest.main()
