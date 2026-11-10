// Which words of a SPIR-V instruction are literals, for the scans that look for an id among the operands (gfx_vk_draw.inc spv_b0_used, spv_limits.h).
// Self-contained (no libc), like spv_limits.h, which dm/tools/tests/test_spv_limits.py builds alone.
#pragma once
#include <stdint.h>

// Word q of an instruction with opcode op in a function body is a literal, never an id: an extended instruction's number, a function's or a merge's control,
// a variable's storage class, memory operands, composite indices and shuffle components, branch weights, switch cases, image operand masks, group operations, line numbers.
// An opcode not listed has every word taken for an id, so that an unknown one can only make a use seen where there is none (safe), never hide one
static int spv_lit(uint32_t op, uint32_t q) {
    switch (op) {
        case 8: case 317: return 1;   // OpLine (an OpString and two numbers), OpNoLine
        case 12: return q == 4;   // OpExtInst type result set instruction operands...
        case 54: case 59: return q == 3;   // OpFunction type result control ftype; OpVariable type result class [initializer]
        case 61: return q >= 4;   // OpLoad type result pointer [memory operands]
        case 62: case 63: return q >= 3;   // OpStore pointer object [memory operands]; OpCopyMemory target source [memory operands]
        case 64: return q >= 4;   // OpCopyMemorySized
        case 79: case 82: return q >= 5;   // OpVectorShuffle type result v1 v2 components...; OpCompositeInsert type result object composite indices...
        case 81: return q >= 4;   // OpCompositeExtract type result composite indices...
        case 246: return q >= 3;   // OpLoopMerge merge continue control [parameters]
        case 247: case 256: case 257: return q == 2;   // OpSelectionMerge merge control; OpLifetimeStart / Stop pointer size
        case 250: return q >= 4;   // OpBranchConditional condition true false [weights]
        case 251: return q >= 3;   // OpSwitch selector default (literal label)...: the labels are no variables
        case 87: case 88: case 91: case 92: case 95: case 98: case 305: case 306: case 309: case 310: case 313: case 320: return q == 5;   // image sample / fetch / read: the operand mask
        case 89: case 90: case 93: case 94: case 96: case 97: case 307: case 308: case 311: case 312: case 314: case 315: return q == 6;   // with a depth reference or a gather component
        case 99: return q == 4;   // OpImageWrite image coordinate texel [mask]
        case 342: return q == 4;   // OpGroupNonUniformBallotBitCount type result scope operation value
        default: return op >= 349 && op <= 364 && q == 4;   // OpGroupNonUniform arithmetic / bitwise / logical: the group operation
    }
}
