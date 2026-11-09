#version 450
#extension GL_ARB_shader_stencil_export : require
// A depth-stencil copy in one draw (gfx_vk_dscopy.inc): the source texel's depth as the fragment's depth, its stencil as the value the stencil test writes
// (VK_EXT_shader_stencil_export)
layout(set = 0, binding = 0) uniform sampler2D src_depth;
layout(set = 0, binding = 1) uniform usampler2D src_stencil;
layout(push_constant) uniform Map { vec2 off, scale; ivec2 lo, hi; int bit; } map;   // the source texel of a fragment: off + frag × scale, clamped to [lo, hi]
void main() {
    ivec2 s = clamp(ivec2(floor(map.off + gl_FragCoord.xy * map.scale)), map.lo, map.hi);
    gl_FragDepth = texelFetch(src_depth, s, 0).r;
    gl_FragStencilRefARB = int(texelFetch(src_stencil, s, 0).r);
}
