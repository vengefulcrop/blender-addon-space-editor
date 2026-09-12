---
type: research
title: "Custom GLSL Sky and Cloud Rendering via Add-ons"
description: "How an add-on can implement a fully custom raymarched sky/cloud renderer outside Cycles and Eevee, using only the public bpy/gpu API"
tags: [gpu, glsl, sky-rendering, addon]
last_updated: 2026-09-12
---

# Custom GLSL Sky and Cloud Rendering via Add-ons

**Goal.** Describe how an add-on can implement a fully custom, non-Cycles,
non-Eevee, raymarched sky/cloud renderer, using only the public `bpy`/`gpu`
Python API. Third-party add-ons describe this kind of renderer as "rendered
neither in Cycles nor in EEVEE, but in a dedicated raymarching engine, then
brought to Blender."

**Status.** Research notes. This document verifies all claims about
existing code against this tree, with file:line citations. It does not
propose any core changes. It describes what is already possible today.

## 1. Executive summary

Blender's shading pipelines give a custom raymarching renderer no injection
point into the actual material graph:

- **Eevee** (Legacy and Next) compiles materials with
  `GPU_material_from_nodetree()`. This function calls each shader node's
  static C++ `gpu_fn` callback into one of about 113 GLSL files, bundled
  into the binary at build time. No runtime registration path exists for a
  new GLSL function from Python or an add-on.
- **Cycles** does not use GLSL at all. It is SVM bytecode plus native CUDA,
  OptiX, HIP, Metal, and oneAPI kernels, with OSL script nodes as its one
  user-shader escape hatch. GLSL is irrelevant to it.

Given that, a fully custom GLSL sky renderer must live outside both
pipelines. It reaches Blender through two boundaries that are public and
dynamic:

1. **Viewport preview.** An add-on can draw a raw GLSL fullscreen raymarch
   directly into the 3D viewport's own framebuffer, before Blender draws
   the scene, using the live camera transform. This has no CPU round-trip
   and runs in real time.
2. **Actual renders (Cycles/Eevee).** No equivalent live hook exists on the
   render path. The raymarch result must be baked into a `bpy.types.Image`
   datablock through a GPU-to-CPU readback. The add-on then references it
   normally as a World Environment Texture or a material Image Texture.
   This is the "brought to Blender" step. No cheaper option exists.

## 2. Why the material graph itself is closed to custom GLSL

`GPU_material_from_nodetree()`
(`source/blender/gpu/intern/gpu_material.cc:133`) walks the shader node tree
and calls each node type's `gpu_fn` callback, for example:

```cpp
ntype.gpu_fn = node_shader_gpu_tex_noise;
```

(`source/blender/nodes/shader/nodes/node_shader_tex_noise.cc:507`)

This callback emits a `GPU_stack_link()` call referencing a named GLSL
function, living in one of the static files under
`source/blender/gpu/shaders/material/*.glsl` (for example
`gpu_shader_material_tex_noise.glsl`). The set of callable function names is
fixed at node-registration time, at build time. The following do not exist:

- a C API or RNA hook that lets a node's `gpu_fn` be supplied dynamically
- a "Script" node equivalent for GLSL, unlike Cycles' OSL `ShaderNodeScript`
  (`intern/cycles/scene/shader_nodes.h:1683`)
- a way for the material codegen to splice in externally supplied GLSL
  function bodies

The `gpu` Python module does expose genuine runtime GLSL compilation:
`gpu.types.GPUShader`, `gpu.types.GPUShaderCreateInfo`, and
`gpu.shader.create_from_info()`. These only produce a standalone shader
object that an add-on binds and draws manually. Nothing connects that
object to `GPU_material_from_nodetree`, to any node's `gpu_fn` slot, or to
Eevee Next's deferred closure, AOV, or shadow pipeline. A real "custom GLSL
material node" would need a multi-week-to-multi-month core engineering
effort. Today's Python API cannot reach it.

Given this limitation, only two ways exist for a raymarched sky to become
visible in Blender. Both appear below.

## 3. Realtime viewport path: draw directly, no round-trip

### 3.1 Draw handler stages

`bpy.types.SpaceView3D.draw_handler_add` exposes three callback stages
(`source/blender/python/intern/bpy_rna_callback.cc:41-44`):

| Mode | When it runs |
|---|---|
| `PRE_VIEW` | Before the engine draws world/sky and scene geometry, already inside the 3D view's projection matrix |
| `POST_VIEW` | After scene geometry, still in 3D world-space |
| `POST_PIXEL` | After everything, in 2D screen space (HUD-style) |

The per-redraw sequence, confirmed in
`source/blender/draw/intern/draw_context.cc:1572-1576`:

```
clear depth  ->  PRE_VIEW callback  ->  engine draws world+scene  ->  POST_VIEW  ->  POST_PIXEL
```

The depth buffer is cleared once, before `PRE_VIEW` runs. It is not cleared
again before the engine draws the scene. A `PRE_VIEW` shader that writes
depth at the far clip plane, for every pixel it touches, depth-tests
correctly against scene geometry drawn afterward. Objects occlude the
raymarched sky, and the sky does not occlude objects. This matches the
convention Blender's own sky pass uses internally, depth-equal or
greater-equal against the far plane. A custom `PRE_VIEW` sky shader only
needs to follow the same rule.

### 3.2 Camera transform is live and fully exposed

`RegionView3D` (`rv3d`) exposes, per
`source/blender/makesrna/intern/rna_space.cc:6095-6189`:

- `perspective_matrix` (read-only 4x4, `window_matrix * view_matrix`)
- `window_matrix`, `view_matrix` (4x4; `view_matrix` is also settable)
- `view_location` (float[3]), `view_rotation` (quaternion), `view_distance`,
  `is_perspective`, `view_camera_zoom`, `view_camera_offset`

These fields read directly from the region's live `RegionView3D` struct
fields (`viewmat`, `winmat`, `persmat`). Blender recomputes them every
redraw, before draw handlers fire. A `PRE_VIEW` callback therefore always
sees the current frame's navigation state, not a stale snapshot. This is
sufficient to reconstruct per-pixel camera rays for a raymarcher, either by
inverting `perspective_matrix`, or by building rays directly from FOV and
rotation.

### 3.3 Resulting design

An add-on can draw a fullscreen triangle in `PRE_VIEW`, through
`gpu.types.GPUShader`/`GPUShaderCreateInfo` plus `gpu.types.GPUBatch`,
using a hand-written GLSL fragment shader. That shader raymarches
atmosphere and clouds using `rv3d`-derived camera rays, and writes
far-plane depth. The GPU composites the result directly into the viewport
framebuffer every frame, with zero CPU involvement and full interactivity.

## 4. Render path: bake to an Image through GPU-to-CPU readback

There is no equivalent live hook for Cycles' or Eevee's actual (F12 or
viewport-render) output. Render engines do not expose a "draw before world"
callback the way the interactive 3D view does. The only way to get
raymarched output into a real render is to bake it into a datablock both
renderers already understand: `bpy.types.Image`.

### 4.1 Render-to-texture, fully in Python

- `gpu.types.GPUOffScreen` plus `GPUShader`/`GPUShaderCreateInfo` plus
  `GPUBatch` let an add-on render an arbitrary GLSL fragment shader to an
  offscreen target purely from Python.
- `GPUOffScreen.texture_color` returns a live `gpu.types.GPUTexture`
  wrapping the color attachment.

### 4.2 The mandatory CPU round-trip

- `GPUTexture.read()` (`source/blender/python/gpu/gpu_py_texture.cc:642`,
  wrapping `GPU_texture_read()`) performs a synchronous GPU-to-CPU readback
  into a `gpu.types.Buffer`.
- The add-on then writes that buffer into `Image.pixels`
  (`source/blender/makesrna/intern/rna_image.cc:1360`). This forces a full
  CPU pixel array write, and a re-upload back to GPU on next use.
- `Image.gl_load()`/`gl_free()`
  (`source/blender/makesrna/intern/rna_image.cc:206-239`) go the opposite
  direction (CPU pixels to GL texture, for draw-callback use) and do not
  help here.
- There is no zero-copy or GPU-resident path in the public API to bind a
  `GPUTexture` directly as a material's sampled texture. This round-trip is
  mandatory.

### 4.3 Where the baked Image goes

Once baked, an add-on wires the `Image` datablock in as any normal
texture, most plausibly as:

- a **World shader Environment Texture** (equirectangular bake, the
  natural sampling for sky, visible to both Cycles and Eevee with no
  further setup), or
- an **Image Texture** on a translucent proxy card or volume, for clouds
  specifically.

Either way, this is the one artifact type that both renderers understand
natively. A fully custom renderer must target it, not anything
node-graph-native.

### 4.4 Update cadence

An add-on drives this cadence with `bpy.app.handlers.frame_change_post` or
`depsgraph_update_post`. It re-runs the raymarch, the offscreen render, the
readback, and the `Image.pixels` write whenever the sun angle, camera, or
cloud parameters change. The cost is bandwidth-bound, resolution times
update frequency, not render-bound, because it is a full CPU round-trip
each time. This is likely why such add-ons keep bake resolution and
frequency configurable, and separate from the interactive viewport path.

## 5. Summary table

| | Viewport preview | Actual render (Cycles/Eevee) |
|---|---|---|
| Mechanism | `SpaceView3D.draw_handler_add(..., 'PRE_VIEW')` | `GPUOffScreen` render, then `GPUTexture.read()`, then `Image.pixels` |
| GPU-resident? | Yes, no CPU round-trip | No, mandatory CPU readback each update |
| Camera source | Live `RegionView3D` (`rv3d`) matrices, per frame | Not applicable (baked as environment texture, not view-dependent) |
| Depth interaction | Writes far-plane depth in `PRE_VIEW`, before scene draw. Depth-tests normally against geometry. | Not applicable. Becomes a normal World or Image texture, sampled like any other. |
| Update cost | Free (part of normal viewport redraw) | Bandwidth-bound CPU-to-GPU transfer per bake |

## 6. Missing capability for a first-class workflow

One gap remains in the public API: no mechanism lets a `GPUTexture` bind
directly as a material's sampled texture without the CPU round-trip.
Everything else described above, dynamic GLSL compilation, offscreen
rendering, live camera access, and pre-scene draw injection, already
exists. Real add-ons exercise it today.
