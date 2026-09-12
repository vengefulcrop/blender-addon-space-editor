---
type: research
title: "Edit-Mode Snapping"
description: "How edit-mode snapping differs from object mode, and the topology-aware inference already present in edge slide and the knife tool"
tags: [snapping, transform, edit-mode]
last_updated: 2026-09-12
---

# Edit-Mode Snapping

See [Object-Mode Snapping](./object_mode_snapping.md) for the shared
infrastructure this section builds on, and
[External Prior Art](./external_prior_art.md) for the next topic.

Files: `transform_snap_object_editmesh.cc`, `transform_mode_edge_slide.cc`,
`transform_mode_vert_slide.cc`, `editmesh_knife.cc`.

The shared infrastructure from object mode is reused almost entirely. Edit
mode's differences are a small number of branch points, plus two genuinely
independent subsystems, edge/vertex slide and the knife tool, that already do
topology-driven geometric inference without touching the generic snap engine
at all. These are the strongest existing precedent in Blender's own codebase
for shape-aware behavior.

## What differs from object mode

- **Self-snap is structurally allowed for mesh edit mode.** Unlike every other
  edit type (curve, armature, lattice; `transform_snap.cc:830-834`) and object
  mode (`:836-839`), which unconditionally add `SCE_SNAP_TARGET_NOT_SELECTED`,
  the mesh branch (`:819-829`) adds nothing by default and instead excludes
  the moving sub-selection at the element level. See
  `snap_target_select_from_spacetype_and_tool_settings`,
  `transform_snap.cc:796-862`.
- **A scene-defaults block then overrides the above for modal transforms**
  (`transform_snap.cc:845-859`). When `T_MODAL` is set, `NOT_ACTIVE`,
  `NOT_EDITED`, `NOT_NONEDITED`, and `ONLY_SELECTABLE` are OR-ed in from
  `ToolSettings.snap_flag`. In practice this adds no exclusion at default
  settings, because `snap_flag` ships as `SCE_SNAP_TO_INCLUDE_EDITED |
  SCE_SNAP_TO_INCLUDE_NONEDITED` (`DNA_scene_types.h:2351`). A user toggling
  "Include Edited" or "Include Non-Edited" changes what any new snap provider
  sees. Account for this when testing new inference code.
- **Slide modes always snap**, independent of the scene's per-mode Translate,
  Rotate, and Scale toggles, because a slide is conceptually always
  geometry-constrained. `TFM_VERT_SLIDE` and `TFM_EDGE_SLIDE` return `true`
  unconditionally. Note the same `ELEM` also covers `TFM_SEQ_SLIDE`,
  `TFM_TIME_TRANSLATE`, and `TFM_TIME_EXTEND`, which are not edit-mesh modes.
  See `transformModeUseSnap`, `transform_snap.cc:155-191` (the slide branch is
  `:180-188`).
- **The edited BMesh is baked into a temporary Mesh** each time its topology
  changes, cached, with invalidation skipped mid-drag as a performance
  workaround, so it can reuse the exact same `snap_object_mesh()` BVH path as
  static objects. See `transform_snap_object_editmesh.cc:60-209`.

## Edge slide and vertex slide: topology-aware constrained motion

Vertex slide walks `BM_EDGES_OF_VERT` to collect neighbor positions one hop
away, pure connectivity, no BVH, then picks whichever neighbor direction's
vector best matches the current drag direction. Edge slide is more elaborate:
it validates the selected loop is manifold or boundary, builds a connectivity
map, and computes two "rail" directions per vertex from the perpendicular loop
edges on either side of the selection. At n-gon corners where the rail is
ambiguous, `bm_loop_calc_opposite_co()` does a line/plane intersection against
the far edge of the face to infer a notional destination point, a small
"infer an implied point on the far side of a face" routine that is directly
relevant prior art.

Both consume ordinary snapping through the same two shared primitives that
generalize cleanly to new inference features:

```cpp
void transform_constraint_snap_axis_to_edge(...)  // ray intersect edge  (ray-ray intersection)
void transform_constraint_snap_axis_to_face(...)  // ray intersect face plane (ray-plane intersection)
// transform_constraints.cc:290-318
```

These already do "intersect the current allowed motion with whatever the snap
system resolved as the target," restricted today to a single free axis, the
slide direction. Generalizing the candidate side, testing several nearby
edges from a local walk instead of just one resolved snap hit, is most of the
work tier-3 intersection-inference needs.

Note the separate acceleration structure: slide modes build their own local
`BMBVHTree` for visibility rather than going through the scene-wide
`SnapObjectContext`, explicitly for performance, avoiding per-vertex scene
raycasts, and correctness, skipping the edge's own connected faces. Any new
per-vertex geometric inference running at interactive rates during a modal
transform should likely follow this pattern rather than the generic
multi-object snap path.

## Knife tool: reference-edge angle inference

Fully independent of `transform_snap.cc`. In "relative" angle-snap mode, the
knife tool raycasts into a BMBVH to find the face under the cursor, derives a
reference vector from the previous cut point's already-connected edge,
confirms that edge and the current face share the same face (angle is only
inferred within one planar face), then snaps the new cut's direction to
rational multiples of that reference angle within the face's own normal
plane. It also supports cycling between multiple candidate reference edges at
a vertex when more than one is plausible.

This is the closest existing Blender code to "pick a construction reference
automatically from adjacent, already-placed geometry, then quantize a new
segment relative to it," the template for tier-2 reference-edge guide lines
(see [Synthesis and Verdict](./synthesis_and_verdict.md)).

## Self-snap exclusion mechanics

Two independent layers: `bm_edge_is_snap_target`/`bm_face_is_snap_target`
callbacks (`transform_snap.cc:653-679`, wired in `snap_object_context_init`,
`:864-884`) exclude any edge or face with a selected vertex from the baked
snap-target mesh, without touching the real mesh's hide state. Slide modes
separately skip selected or hidden elements in their own local BMBVH walk.

**Correction on `SCE_SNAP_TARGET_NOT_EDITED`, verified against source.** This
flag does not exclude the mesh you are currently editing. The test in
`snap_object_is_snappable` is guarded by `!is_active`:

```cpp
if ((snap_target_select & SCE_SNAP_TARGET_NOT_EDITED) && is_edited && !is_active) {
  /* Base is edited, but not active. */
  return false;
}
// transform_snap_object.cc:485-488
```

So under proportional editing, or with "Include Edited" disabled, what gets
excluded is other edited meshes in a multi-object edit session. The active
mesh remains a valid snap target, and its moving geometry is filtered only at
the element level by the BMesh callbacks above. Any new inference provider
inherits the same asymmetry: it will still see the active edited mesh.

There is no mirror-aware exclusion in the snap-target logic. Mirrored geometry
is only handled by auto-merge after the fact, a complementary coarse "weld
what ended up close enough" mechanism worth keeping in mind as a fallback for
any new live-inference feature that lands slightly off target.
