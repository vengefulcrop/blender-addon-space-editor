---
type: research
title: "Object-Mode Snapping"
description: "How Blender's shared snap state machine drives object-mode transforms, from mouse move to applied delta"
tags: [snapping, transform, object-mode]
last_updated: 2026-09-12
---

# Object-Mode Snapping

See [Synthesis and Verdict](./synthesis_and_verdict.md) for context, and
[Edit-Mode Snapping](./edit_mode_snapping.md) for how edit mode differs.

Files: `source/blender/editors/transform/transform_snap*.cc/hh` (paths
relative to the repo root).

One shared state machine, `TransInfo::tsnap`, drives every snap-capable
operator in the 3D viewport. Object mode is the plain configuration of this
machine: whole objects, or the 3D cursor through the same `TransData`
pipeline, are the thing being moved. The target-search half of the system
walks the rest of the scene through a generic multi-object BVH query engine
shared with edit mode, the ruler tool, and the cursor-snap operators.

## Data flow, mouse move to applied delta

1. **Invoke.** `initSnapping()` (`transform_snap.cc:1054`) reads
   `ToolSettings.snap_mode`/`snap_flag`, computes the `eSnapTargetOP` filter
   (object mode defaults to `NOT_SELECTED | NOT_ACTIVE`), and creates a
   `SnapObjectContext`.
2. **Every about 10 ms** (explicitly throttled). `transform_snap_mixed_apply()`
   (`transform_snap.cc:594`) calls `snap_target_view3d_fn()`, which calls
   `snapObjectsTransform()`, which calls the public
   `snap_object_project_view3d_ex()` entry point.
3. **Target search.** A raycast finds what is directly under the cursor, also
   yielding an occlusion plane that hides back-facing candidates. If vertex,
   edge, or face-midpoint modes are active, `snapObjectsRay()` walks every
   visible object with `iter_snap_objects()`, dispatching to a per-type
   provider.
4. **Source point.** Computed independently, in transform-side code, from
   `ToolSettings.snap_target` (closest, center, median, or active
   bounding-box corner), not from the `SnapObjectContext` at all.
5. **Apply.** Once both source and target resolve, `ApplySnapTranslation()`,
   or the rotate/resize equivalents, overwrites the transform delta with
   `snap_target - snap_source`, subject to any active axis or plane constraint
   intersecting the snapped edge or face rather than naively projecting onto
   it.

The 3D cursor is not a special code path. `transform_convert_cursor.cc` builds
one `TransData` representing the cursor and pushes it through the identical
`tsnap` machinery. `view3d_cursor_snap.cc`, the non-modal "Snap Cursor to..."
operators, calls the same public `snap_object_project_view3d()` API.

## Key structures

| Struct or enum | Location | Role |
|---|---|---|
| `TransSnap` | `transform.hh:541-575` | Per-operator state: mode, source/target ops, resolved world-space points, callbacks. |
| `SnapObjectContext` | `transform_snap_object.hh:42-120` | The generic query engine: editmesh cache map, callback filters, grid cache, per-call runtime scratch, output. |
| `SnapObjectParams` | `ED_transform_snap_object_context.hh:61-83` | Caller-facing query knobs: target filter, edit-mode geometry choice, occlusion test, backface culling. |
| `SnapData` / `SnapData_Mesh` | `transform_snap_object.hh:139-180` | Base class for screen-projected nearest tests; per-type accessors overridden per geometry kind. |
| `eSnapMode` | `DNA_scene_types.h:1928-1961` | Bitflags: point, edge-midpoint, edge-endpoint, edge-perpendicular, edge, face, face-midpoint, volume, grid, increment. |
| `eSnapTargetOP` | `DNA_scene_types.h:1917-1925` | Which objects are eligible targets: not-selected, not-active, not-edited, only-selectable. |
| `eSnapSourceOP` | `DNA_scene_types.h:1904-1909` | Which point on the moving thing is aligned: closest, center, median, active. |

## How each snap mode finds its target

| Mode | Mechanism | Notes |
|---|---|---|
| Vertex | Screen-projected BVH nearest (`BLI_bvhtree_find_nearest_projected`) | Loose verts plus triangle corners. Per-type loops for armature, curve, lattice, and camera. Object-origin fallback for empties. |
| Edge | Same projected-nearest query against edge segments | Loose edges plus triangle edges. Bones count as edges. |
| Edge midpoint / perpendicular | Re-tested after a plain edge hit | `snap_edge_points_impl` divides the edge into zones so endpoint, midpoint, and perpendicular do not compete for the same click. |
| Face | True 3D raycast (`BLI_bvhtree_ray_cast`) | Object-local ray transform, bounding-box pre-test, optional backface culling. |
| Face midpoint | Projected-nearest against computed face centers | Centers are computed on the fly, not cached per face. |
| Face nearest (closest surface) | True nearest-point-on-surface (`BLI_bvhtree_find_nearest`) | Multi-step raymarch (`face_nearest_steps`) prevents tunneling across concave geometry. This is the closest existing analogue to architectural surface placement. |
| Grid | Round ray/view-plane intersection to a cached grid cell | Distinct from increment, which rounds the transform delta itself. |
| Volume | All-hits raycast, midpoint of entry/exit | Only reached if nothing else matched. Supports peeling through stacked objects. |

## BVH caching and performance

Evaluated meshes cache their corner-tri and loose-vert/edge BVH trees inside
`Mesh::runtime` itself. These survive across the whole modal drag and across
operators, and only rebuild when topology actually changes. Vertex, edge, and
face-midpoint queries measure distance in projected screen space
(`DistProjectedAABBPrecalc`), which is why snapping feels consistent
regardless of view distance. Occlusion is approximated cheaply: the face
raycast hit synthesizes a clip plane rather than depth-testing a G-buffer.
Dupli-instances are expanded fresh on every single query, a real cost in
heavily-instanced scenes that is not cached across mouse moves.

The most CAD-relevant existing mechanism is constraint interaction:
`transform_constraint_get_nearest()` (`transform_constraints.cc:374-460`)
solves for the intersection of a constrained axis with a snapped edge
(ray-ray) or face (ray-plane), rather than snapping fully onto it. Pressing
`G X` near an edge slides along X until it crosses that edge. This is the
primitive the tier-3 "inferred intersection" feature (see
[Synthesis and Verdict](./synthesis_and_verdict.md)) would generalize.
