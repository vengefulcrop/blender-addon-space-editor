# Viewport Camera Data in the Compositor

**Branch**: `pyareas/compositor-viewport-camera`, branched from `main` (not from
`pyareas/addon-space-editor`) - deliberately, so this can be submitted upstream
independently of the Add-on Editor work. See "Repository conventions" in
`docs_ui/addon_space_editor_plan.md` §8 for the same branch-per-feature discipline
applied there; the same rule applies here: `main` is never committed to, this branch's
own `git format-patch main..pyareas/compositor-viewport-camera` is the publishable
diff, and it should stay small and focused enough that dropping it (or the Add-on
Editor branch) from consideration never requires touching the other.

## 1. Motivation

Blender's Compositor already exposes `Camera Info` and `Object Info` nodes (dual-
registered from Geometry Nodes, see §2) that read a real `Object`/`Camera` ID-block.
There is no way to drive the same compositor effects from the live 3D Viewport's own
camera - a freely-navigated viewport has no backing `Object` at all, only a
`RegionView3D`. The goal is to make `Camera Info` and `Object Info`, when unconnected
**inside the Compositor specifically**, source their data from whichever 3D Viewport is
currently evaluating the live compositor preview, with **zero behavior change to
Geometry Nodes**.

## 2. Current architecture (researched, not assumed)

### 2.1 Camera Info / Object Info are one node type, two independent exec paths

- `source/blender/nodes/geometry/nodes/node_geo_camera_info.cc`
- `source/blender/nodes/geometry/nodes/node_geo_object_info.cc`

Each registers a single `bNodeType` with two unrelated callbacks:

```cpp
ntype.geometry_node_execute = node_geo_exec;         // Geometry Nodes path
ntype.get_compositor_operation = get_compositor_operation;  // Compositor path, returns
                                                              // a NodeOperation subclass
                                                              // whose execute() runs
                                                              // Camera Info: `CameraInfoOperation`
                                                              // Object Info: `ObjectInfoOperation`
```

A shared `poll()` (`geo_cmp_node_poll_default`, `node_geometry_util.cc:146`) allows the
node in exactly `"GeometryNodeTree"` or `"CompositorNodeTree"`, nothing else. This
structural separation is why the planned change (§5) can touch only the compositor
path: `node_geo_exec()` and `*Operation::execute()` are already different functions:
editing one cannot leak into the other.

### 2.2 Current unconnected-input behavior differs between the two paths

Both exec paths follow the same shape - `if (!object) { <default output>; return; }` -
but the "default output" mechanism is different per path, and produces **different
values for the same node**:

| Output type | Geometry Nodes default (`construct_socket_default_value`, `node_socket.cc:1429`) | Compositor default (`Result::allocate_invalid()`, `result.cc:553-555`) |
| :--- | :--- | :--- |
| Matrix (`Projection Matrix`, `Transform`) | `float4x4::identity()` | `float4x4::zero()` |
| Bool | `false` | `false` |
| Float / Vector | `0.0` / zero vector | `0.0` / zero vector |
| Object (ID pointer) | `nullptr` | `nullptr` |

This is a pre-existing inconsistency, unrelated to this feature - noted here because
the planned change replaces the *compositor* zero-matrix branch specifically, which is
already the less useful of the two (a zero projection matrix is degenerate/non-
invertible; identity is at least a harmless no-op). Not proposing to fix the GN/
compositor discrepancy itself; out of scope.

### 2.3 The camera-equivalence math already exists for viewports

`BKE_camera_params_from_view3d()` (`blenkernel/intern/camera.cc:414`) already converts
a `View3D`/`RegionView3D` pair into a full `CameraParams` - the *same* struct
`get_camera_parameters()` builds from a real `Object` in `node_geo_camera_info.cc`.
Verified field-by-field:

- `lens` - `v3d->lens`, the same physical quantity as `Camera.lens` (both computed
  against `DEFAULT_SENSOR_WIDTH = 36.0f`, confirmed via
  `view3d_navigate_zoom_border.cc:118`: `dist_new *= (v3d->lens / DEFAULT_SENSOR_WIDTH)`).
  This is exactly the field read by `bpy.data.screens[...].areas[...].spaces[0].lens`.
- `clip_start`/`clip_end` - `v3d->clip_start`/`clip_end`, direct fields.
- `is_ortho` - `rv3d->persp == RV3D_ORTHO`, direct.
- `sensor_x`/`sensor_y` - default to `DEFAULT_SENSOR_WIDTH`/`HEIGHT` (36x24mm) via
  `BKE_camera_params_init()` when not camera-locked; not a hack, Blender's own standing
  default for "no physical camera" already used elsewhere.
- `shiftx`/`shifty` - stay zero outside camera-locked view; correct (freeform navigation
  has no lens-shift concept), not a gap.
- `ortho_scale` - real formula already implemented: `rv3d->dist * sensor_size / v3d->lens`.
- **Gap**: no `focus_distance` field on `CameraParams` at all - `Camera Info`'s DOF
  output comes from a separate call, `BKE_camera_object_dof_distance(camera_obj)`,
  which needs a real `Object`. No principled viewport-derived value exists; needs an
  arbitrary placeholder constant.
- **Also handles the camera-locked case**: when `rv3d->persp == RV3D_CAMOB`, the
  function delegates straight to `BKE_camera_params_from_object()` on `v3d->camera` -
  so calling this one function is correct whether the viewport is free-navigating or
  looking through a real camera.

`view3d_winmatrix_set()` (`editors/space_view3d/view3d_view.cc:317`) is the existing
call site building `rv3d->winmat` this same way, for reference.

### 2.4 Multi-viewport evaluation is already independent per viewport

Researched via the DRW viewport-compositor engine
(`source/blender/draw/engines/compositor/compositor_engine.cc`). The live compositor
preview is a standard DRW draw engine, instantiated **once per viewport** that enables
it - the same per-`GPUViewport` model EEVEE/Workbench use. Confirmed:

- `Instance::draw()` (`compositor_engine.cc:422-460`) builds a **fresh `Context`
  object every single draw call**, one per viewport, never shared or cached across
  viewports:
  ```cpp
  Context context(cache_manager_, DEG_get_bmain(...), DRW_context_get()->scene, this->info);
  ```
- That code already calls `DRW_context_get()` throughout (lines 97, 131, 309, 340, 374),
  which carries `rv3d`/`region`/`v3d` for *whichever viewport is currently drawing* -
  and existing evaluation already branches on it per viewport today
  (`get_compositing_domain()` checks `draw_ctx->rv3d->persp == RV3D_CAMOB`;
  `get_view_name()` reads `v3d->multiview_eye`).

**Conclusion**: "each live-preview viewport supplies its own camera, same node tree
evaluated independently per instance" is not new architecture to build - it already
falls out for free once viewport data is exposed through `Context`'s interface, because
each viewport already gets its own `Context` and its own `DRW_context_get()` per draw.

## 3. Rejected alternative: a hidden/transient `Object`

Considered: fabricate a hidden `Object`+`Camera` (in an isolated `Main`, invisible to
Outliner/undo, mirroring `BKE_main_new()` scratch-database pattern already used by
`editors/render/render_preview.cc:892` for icon/material preview rendering) that a new
"Viewport Camera" node could output through a real `decl::Object` socket - satisfying
"connects to Camera Info/Object Info the same way Active Camera does" literally, for
both camera-locked and free-navigation viewports.

**Rejected for now.** The precedent (`object_preview_render()`) confirms the technique
is legitimate, but it is fundamentally heavyweight: full `BKE_main_new()` +
`DEG_graph_build_from_view_layer()` + `DEG_evaluate_on_refresh()`, run **per preview
job**, cached, off the interactive path - not per-draw-call, not per-viewport-per-frame.
Adapting it to real-time use would require:

- A persistent (not per-frame) scratch `Main`/`Object`/`Camera`, mutated in place per
  draw rather than rebuilt - open question whether one shared instance is safe under
  potentially-interleaved multi-viewport draws, or whether one instance per viewport is
  needed (extra lifecycle: create on compositor-preview-enable, destroy on disable/area
  close).
- Auditing whether in-place field mutation outside the normal ID-recalc/depsgraph-tag
  path stays correctly in sync with the compositor's own per-viewport
  `StaticCacheManager` (confirmed persistent, `compositor_engine.cc:409`).

Both problems are specific to the hidden-object approach; neither exists for the direct
fallback in §5, which only reads floats and does arithmetic - no ID identity, no
lifecycle, no cache-correctness question. The delivered values would be identical
either way (same `BKE_camera_params_from_view3d()` result), so the hidden-object route
only buys graph-editing UX (an explicit node vs. an implicit unconnected-socket
behavior), not any new capability. Not pursued unless that UX is later judged worth the
added engineering risk.

## 4. Design constraint, explicit

**This must only ever affect the Compositor exec path.** Geometry Nodes' `node_geo_exec()`
in both files must not be touched at all. Structurally guaranteed already (§2.1) - the
implementation in §5 only edits `CameraInfoOperation::execute()` /
`ObjectInfoOperation::execute()` and the `Context` accessor those call into.

## 5. Implementation plan (converged, not yet started)

1. **`source/blender/compositor/COM_context.hh`**: add one virtual accessor, default
   `nullptr`:
   ```cpp
   virtual const RegionView3D *get_viewport_region_data() const { return nullptr; }
   ```
2. **`source/blender/draw/engines/compositor/compositor_engine.cc`**: override it in the
   viewport-preview `Context` subclass, returning `DRW_context_get()->rv3d` - data
   already fetched there for other purposes (§2.4).
3. **F12-render `Context` subclass**: no override - inherits `nullptr`, correctly
   meaning "no viewport during a render." This is the honest answer to "what does
   viewport mean during a headless/background render" flagged early in this
   investigation: there is none, and the accessor says so.
4. **`node_geo_camera_info.cc`, `CameraInfoOperation::execute()`**: when
   `!camera_object`, check `this->context().get_viewport_region_data()` before falling
   to `allocate_default_remaining_outputs()`. If non-null, build `CameraParams` via
   `BKE_camera_params_from_view3d()` and run the *same* downstream math the connected-
   object branch already uses (`compute_sensor_size`, `compute_lens_shift`,
   `BKE_camera_params_compute_matrix`) - no new math, reusing what is already there.
   `Focus Distance` gets a fixed placeholder constant (§2.3 gap).
5. **`node_geo_object_info.cc`, `ObjectInfoOperation::execute()`**: same idea for
   `Transform`/`Location`/`Rotation`/`Scale`, derived from `inverse(rv3d->viewmat)` -
   the standard camera-world-matrix-from-view-matrix relationship, the same one
   `obmat_to_viewmat()` (`view3d_view.cc:381`) uses in the opposite direction. `Geometry`
   output stays at its existing default (empty) - a viewport has no mesh, correctly
   nothing to output there either way.

No new node type is required for this core mechanism. A separate "Viewport Camera"
node (real `Object` output, mirroring `node_geo_input_active_camera.cc`, only
meaningful for the camera-locked-viewport case since free navigation has no `Object` to
give regardless) remains a distinct, optional, not-yet-decided addition - see §3 for why
it does not change what data is actually delivered.
