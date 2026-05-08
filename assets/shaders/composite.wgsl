// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  assets/shaders/composite.wgsl
//
//  Single-layer canvas compositor for Phase 1.
//  Renders the full document texture as a screen-aligned quad, applying
//  the pan/zoom view transform supplied by the CPU via a uniform buffer.
//
//  Bindings:
//    group 0, binding 0 — ViewUniform (4×4 matrix, MVP in NDC)
//    group 0, binding 1 — canvasTex  (RGBA8 document texture)
//    group 0, binding 2 — canvasSampler (nearest for zoom < 2×, linear otherwise)
// ─────────────────────────────────────────────────────────────────────────────

struct ViewUniform {
    mvp : mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> view       : ViewUniform;
@group(0) @binding(1) var          canvasTex  : texture_2d<f32>;
@group(0) @binding(2) var          canvasSamp : sampler;

struct VertOut {
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
};

// Unit quad in document space [0,1]×[0,1], two CCW triangles.
var<private> POSITIONS : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0),
);

var<private> UVS : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 0.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(0.0, 1.0),
    vec2<f32>(1.0, 0.0),
    vec2<f32>(1.0, 1.0),
);

@vertex
fn vs_main(@builtin(vertex_index) vi : u32) -> VertOut {
    var out : VertOut;
    out.pos = view.mvp * vec4<f32>(POSITIONS[vi], 0.0, 1.0);
    out.uv  = UVS[vi];
    return out;
}

@fragment
fn fs_main(in : VertOut) -> @location(0) vec4<f32> {
    // Canvas background: checkerboard if alpha < 1, then canvas colour on top.
    let texel = textureSample(canvasTex, canvasSamp, in.uv);

    // Checkerboard pattern for transparent areas (8-pixel cells)
    let cell = vec2<u32>(vec2<i32>(in.pos.xy) / 8);
    let check = (cell.x + cell.y) % 2u;
    let bg = select(vec3<f32>(0.8, 0.8, 0.8), vec3<f32>(1.0, 1.0, 1.0),
                    check == 0u);

    // Composite texel over checkerboard background
    let rgb = texel.rgb * texel.a + bg * (1.0 - texel.a);
    return vec4<f32>(rgb, 1.0);
}
