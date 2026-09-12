---
type: research
title: "sl_ct (CAD Transform) Add-on Teardown"
description: "Architecture teardown of the sl_ct third-party Blender add-on, a fully independent modal transform and snap system"
tags: [snapping, addon-teardown, cad]
last_updated: 2026-09-12
---

# `sl_ct` (CAD Transform) Add-on Teardown

See [External Prior Art](./external_prior_art.md) for the previous topic, and
[`Construction Lines` Add-on Teardown](./construction_lines_addon_teardown.md)
for the next.

Add-on location:
`%APPDATA%\Blender Foundation\Blender\5.3\scripts\addons\sl_ct`

A fully independent modal transform system. It does not call `bpy.ops.transform.*`
or Blender's snap machinery at all. It reimplements move, rotate, scale, and
align as hand-rolled modal operators with their own detection engine, GPU
overlay, and numeric-entry stack. It is registered as toolbar entries placed
`after={"builtin.transform"}`, living alongside stock Blender rather than
inside it.

## 1. Architecture

Entry point: `operators.py`, class `SLCT_main` (`operators.py:149`),
`bl_idname = '%s.transform' % __package__`. Concrete operators
`SLCT_OT_move`, `SLCT_OT_rotate`, `SLCT_OT_scale`, `SLCT_OT_pinhole`,
`SLCT_OT_align`, and `SLCT_OT_adjust` (`operators.py:2383-2916`) all mix in
`SLCT_main`. `SLCT_main.invoke()` (`operators.py:2352`) guards against
re-entrancy, then calls `wm.modal_handler_add(self)`, a classic hand-rolled
modal operator with no cooperation with Blender's transform system.

Per-instance setup (`init_handler`, `operators.py:2299`) creates its own
detection and drawing stack: `self._detector = Detector(context, event)`
(`operators.py:2303`) and `self._handle =
bpy.types.SpaceView3D.draw_handler_add(...)` (`operators.py:2292`), its own
GPU overlay, own numeric-entry state machine (`Keyboard`), and own transform
stack (`Transform`).

Each mouse move dispatches to `mouse_move_handler` (`operators.py:1419`),
which calls `self._detector.detect(context, event)` (`operators.py:1427`)
every frame.

## 2. Detection engine (`engine.py`, `detector.py`)

`Detector` (`detector.py:56`) composes three concrete engines run in sequence
each frame:

```python
self._engines = [self.raycast, self.raster, self.grid_engine]   # detector.py:66-71
```

- **`RayCastDetectEngine`** (`raycast.py`) uses `obj.ray_cast()` to hit mesh
  polygons, casting a center ray plus 6 samples around the cursor in a circle
  (`_cast_samples = 6`, `raycast.py:585`) to catch nearby geometry the center
  ray misses. Within the hit face it evaluates the closest of vertex, edge,
  edge-midpoint, or interior point (`_closest_subs`, `raycast.py:369`), plus
  face-center and whole-face fallback (`_closest_mesh_face`,
  `raycast.py:436`). Edit-mode objects are handled through a temporary
  object-mode duplicate (`create_obj_from_edit`, `raycast.py:137`).
- **`RasterDetectEngine`** (`raster.py`) is a GPU offscreen index-picking pass
  for isolated verts and edges, curves, NURBS, grease pencil, object origins,
  bounding-box corners, the 3D cursor, and edit-mode selection center. It
  renders each `Detectable` batch with a color-encoded index, reads back a
  window around the mouse, and decodes it with a spiral/ring scan
  (`_process_buffer`, `raster.py:569`).
- **`GridEngine`** (`grid_engine.py`) snaps to the nearest visible grid
  intersection. See section 4.

Ranking (`Detector.detect`, `detector.py:175-218`) uses this sort key:

```python
def key(i):
    return SnapItemType.key(i.type), i.dist, i.z     # detector.py:202-203
```

`SnapItemType.key()` (`types.py:318`) encodes a fixed type priority, point
before edge-midpoint before edge before face-center before face, then ties
break by squared pixel distance, then view depth. This is a principled
version of the ranking that Blender's own `eSnapMode` ordering implies but
does not formalize as one sort key.

## 3. `geom.py`, geometric algorithms

Two classes, `Geom2d` (screen-space) and `Geom3d` (world-space). Key `Geom3d`
classmethods:

| Function | Purpose |
|---|---|
| `mouse_to_plane` (`geom.py:608`) | Ray/plane intersect for cursor-to-3D projection, with axis fallbacks for degenerate planes. |
| `intersect_ray_plane_t` / `intersect_ray_plane` (`geom.py:943/970`) | Parametric ray-plane intersection with angle-based degeneracy rejection. |
| `intersect_line_plane` / `intersect_segment_plane` (`geom.py:985/997`) | Unbounded and segment-bounded line/plane intersection. |
| `neareast_point_on_line_t` / `_on_line` (`geom.py:1023/1039`) | Closest point on an infinite line. |
| `neareast_point_ray_line_t` / `_ray_line` (`geom.py:1050/1085`) | Closest point between a view ray and a 3D segment. |
| `neareast_point_line_line_t` / `intersect_line_line` (`geom.py:1099/1125`) | True 3D skew-line closest-approach, including non-coplanar lines. |
| `intersect_ray_tri_t` / `intersect_line_tri` (`geom.py:1139/1155`) | Moller-Trumbore-style ray/triangle intersection. |
| `_safe_vectors`/`safe_matrix`/`matrix_from_normal`/`matrix_from_view`/`matrix_from_3_points` (`geom.py:717-901`) | Construct orthonormal bases from partial direction data, handling degenerate or parallel guide vectors. |

`View` (`geom.py:73`) tracks per-frame camera and mouse state, and provides
pixel-to-world conversions used throughout for proximity scoring.

## 4. `grid_engine.py`, oriented construction grid

`GridEngine.detect()` (`grid_engine.py:64`) projects the mouse onto
`Space.grid`, not the global XY plane, but an arbitrary matrix
(`grid_engine.py:77`). `Space.grid` (`transform.py:79`) is set from the
active transform space, which per `Space.get()` (`transform.py:129`) can be
world, local (object), individual-origin, screen-aligned, or a persisted
user-defined orientation, a genuinely rotated construction plane, a real CAD
feature.

Step size adapts to zoom through `View.grid_scale()` (`geom.py:256`), keeping
about 4 major divisions visible, snapping to 10 or 12 subdivisions depending
on the unit system (base-12 for feet and inches).

## 5. `constraint.py`, constraint-aware analytic snap resolution

`Constraint.apply()` (`constraint.py:230`) is the key fusion point. Both
`to_axis()` and `to_plane()` consult `trs.snapitem`, the currently hovered
snap target, instead of doing plain projection:

- `to_axis()` (`constraint.py:183`). If the snap target is a line, computes
  the true 3D line-line intersection between the constrained axis and the
  target edge. If it is a triangle, intersects the axis ray with the face
  plane. Falls back to closest-point-on-axis only if nothing is snapped.
- `to_plane()` (`constraint.py:69`). If snapping to a line, intersects the
  segment with the constraint plane, or for perpendicular or parallel
  constrained rotation, computes a rotation-arc/plane intersection. If
  snapping to a triangle, intersects the two planes as a line, then further
  intersects with the rotation-arc radius.

So axis or plane constraint is not "clamp mouse-derived point to axis." It is
"intersect the constrained axis or plane analytically with whatever geometry
is currently snapped," giving true CAD-style constrained intersections, for
example "move along X until it hits this edge." This directly generalizes
Blender's own `transform_constraint_snap_axis_to_edge`/`_face` (see
[Edit-Mode Snapping](./edit_mode_snapping.md)).

## 6. Rendering and input

Uses the modern `gpu`/`gpu.types`/`gpu.shader`/`blf` modules, not legacy
`bgl`. `Drawable` and its subclasses `Circle`, `Square`, `Cross`, `Line`,
`Pie`, `Mesh`, `Curve`, `Text`, and `Image` (`drawable.py:215-1085`) build GPU
batches for overlay widgets. It uses a fully custom `POST_PIXEL` draw
handler, none of this rides on Blender's built-in snap gizmo drawing. Input
handling lives in `SLCT_main.modal()` (`operators.py:1930`), an explicit
event-type dispatch table.

## 7. `bmesh_utils.py`

Not the snap-candidate query path, which is inline in `raycast.py`/`raster.py`
through `bmesh.from_edit_mesh()`. Instead it implements post-transform UV
re-projection: `ops_duplicate` (`bmesh_utils.py:45`) clones selected BMesh
geometry for copy-transform, and `correct_face_attributes`
(`bmesh_utils.py:244`) re-maps UV coordinates onto deformed faces after a
transform.

Also worth noting: `snapitem.py`'s `SnapContext` (`snapitem.py:164`) is a
separate, higher-level construction and inference layer where the user
explicitly accumulates multiple snap items and combines them: line-line
intersection (`_as_intersection`, `snapitem.py:315`), closest points between
two lines or a point and a line (`_as_closest`, `snapitem.py:364`), a plane
from 1 to 3 points or two lines (`_as_space`, `snapitem.py:465`), average of
N points (`_as_average`, `snapitem.py:429`), and perpendicular or
point-on-line projections (`_as_point`, `snapitem.py:543`). This is a manual
"construction geometry" toolkit layered above the passive hover-detection
engines.

## 8. Assessment for native porting

| Difficulty | Component | Why |
|---|---|---|
| Easy | Pure-math layer (`geom.py`) | Stock computational geometry. Blender's BLI already mostly has equivalents. Wiring, not invention. |
| Medium | Oriented grid plus constraint-aware analytic resolution | Requires restructuring how snap and transform-orientation code talk to each other. Primitives already exist. |
| Hard | GPU offscreen index-picking (`raster.py`) | A clever Python-side workaround for cheap hit-testing. Native code would more naturally extend the existing BVH path instead of render-and-readback. |
| Hard | Multi-select "construction context" (`SnapContext`) | A genuinely new interaction model not present in Blender's snap paradigm at all. Most valuable UX idea to steal, most work to build. |

Best ideas worth stealing for native work: the type-priority plus
squared-pixel-distance sort as a principled candidate ranking; constrained-axis
or plane analytic intersection with the hovered target instead of naive
projection; the rotated or oriented construction grid bound to the current
transform orientation; and the explicit multi-item "construction context"
(intersection, closest, average, plane-from-3-points) as an optional advanced
inference mode.
