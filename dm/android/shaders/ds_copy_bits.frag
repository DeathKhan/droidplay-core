#version 450
// A depth-stencil copy without stencil export (gfx_vk_dscopy.inc): the source texel's depth as the fragment's depth; with bit >= 0, the fragment is kept only where
// the source's stencil has that bit, and the draw writes that bit alone (write mask 1 << bit, reference 0xff)
layout(set = 0, binding = 0) uniform sampler2D src_depth;
layout(set = 0, binding = 1) uniform usampler2D src_stencil;
layout(push_constant) uniform Map { vec2 off, scale; ivec2 lo, hi; int bit; } map;
void main() {
    ivec2 s = clamp(ivec2(floor(map.off + gl_FragCoord.xy * map.scale)), map.lo, map.hi);
    gl_FragDepth = texelFetch(src_depth, s, 0).r;
    if (map.bit >= 0 && ((texelFetch(src_stencil, s, 0).r >> uint(map.bit)) & 1u) == 0u) discard;
}
