---
type: research
title: "Precision and Inference Snapping: Synthesis and Verdict"
description: "What Blender already has for CAD-style inference snapping, what is missing, and the recommended build order"
tags: [snapping, transform, cad, research]
last_updated: 2026-09-12
---

# Precision and Inference Snapping: Synthesis and Verdict

Research dossier. Read-only investigation, no source changes. Branch:
`pyareas/compositor-viewport-camera`. Scope: object mode, edit mode, four
external CAD systems, two Blender add-ons. Compiled 2026-08-10.

**Verification status.** The verification pass re-checked about 65 source
citations against the working tree on 2026-08-10. It confirmed accurate
function and struct line numbers in the following files:

- `transform_snap.cc`
- `transform_snap_object.cc`
- `transform_constraints.cc`
- `transform.hh`
- `transform_snap_object.hh`
- `ED_transform_snap_object_context.hh`
- `DNA_scene_types.h`
- `editmesh_knife.cc`
- `transform_convert_mesh.cc`
- `bvhutils.cc`

It also confirmed the quoted `transform_constraint_snap_axis_to_edge`/`_face`
excerpt.

The verification found and corrected four issues:

- A missing `eSnapMode` bit-budget constraint. See below.
- A wrong claim about `SCE_SNAP_TARGET_NOT_EDITED` scope. See
  [Edit-mode snapping](./edit_mode_snapping.md).
- An unmentioned `T_MODAL` settings-override block. See
  [Edit-mode snapping](./edit_mode_snapping.md).
- A narrow description of the slide-mode snap exemption. See
  [Edit-mode snapping](./edit_mode_snapping.md).

An HTML version of this dossier, with a sticky table of contents, is saved
at `docs_cad/precision-snapping-dossier.html`.

This section covers:

- [Object-mode snapping](./object_mode_snapping.md)
- [Edit-mode snapping](./edit_mode_snapping.md)
- [External prior art](./external_prior_art.md) (FreeCAD, SketchUp, AutoCAD,
  Rhino, the Ashlar patent)
- [`sl_ct` add-on teardown](./slct_addon_teardown.md)
- [`Construction Lines` add-on teardown](./construction_lines_addon_teardown.md)
- [Extension-point map](./extension_point_map.md)

## Verdict

Blender already has almost every low-level primitive that a SketchUp- or
FreeCAD-style inference engine needs:

- screen-projected BVH nearest queries
- ray/edge and ray/plane intersection against a snapped target
- per-mesh cached BVH trees
- a working precedent for topology-driven constrained motion in edge slide

It lacks one layer: synthesis of temporary, non-existent geometry, for
example extended edges, alignment guides, and inferred intersections, as
snap candidates. That layer is the single mechanism that separates "snap to
what is there" from "snap to what you obviously mean." Bier's 1986
Snap-Dragging paper and the 1992 Ashlar Geometric Inference patent document
this mechanism well. Every modern CAD tool surveyed, FreeCAD Draft,
SketchUp, AutoCAD, and Rhino, is a variation on it.

**Recommended entry strategy.** Extend the shared object-snap engine
(`transform_snap_object.cc`) with a new provider function alongside
`snap_object_mesh`/`snapArmature`. The new function returns synthetic
points and lines, not scene geometry, computed from whatever the cursor is
near. Feed its output through the existing `SnapData::snap_point`/`snap_edge`
primitives, so screen-distance ranking, clip planes, and occlusion work
without change. Layer the constrained-axis intersection already used by
edge slide (`transform_constraints.cc:290-318`) on top, for the behavior
that extends along an axis until it meets another edge.

## Hard constraint discovered during verification

`eSnapMode` is a signed `short` with bit 11 as its highest used bit. Only 3
free bits remain (12, 13, 14). The tiers below imply four or more new snap
categories, so they cannot each get their own mode bit without widening the
DNA type. Prefer a single `SCE_SNAP_TO_INFERRED` bit with sub-options in a
separate flag field. Details and alternatives are in the
[Extension-point map](./extension_point_map.md).

## Ranked build order

| Tier | Feature | Why here | Hooks |
|---|---|---|---|
| 1, easy | Edge extension and near-line snapping | Relax the parametric `[0,1]` bound already used to test a hovered edge, so the line counts as infinite for candidacy. This changes an existing test. It does not add a new spatial query. | `SnapData::snap_edge`, `transform_snap_object.cc:193-223` |
| 1, easy | Perpendicular-foot and tangent inference | Given a remembered anchor point, project it onto nearby edge or curve candidates already returned by the BVH query. It uses pure vector math over data the engine already fetches. | `snap_edge_points_impl`, `transform_snap_object.cc:225-301` |
| 2, medium | Reference-edge parallel/perpendicular guide lines | Track one "active reference edge" (last-touched or modifier-held), the way the knife tool tracks `snap_ref_edge`. Test the in-progress motion's angle against it, and draw a dashed guide when aligned. This is SketchUp's magenta-axis behavior. | `editmesh_knife.cc:3447-3542` (pattern), plus a new draw callback beside the snap gizmo in `transform_snap.cc` |
| 2, medium | Axis-aligned inference guides from a dwelled vertex | Cache the last vertex the cursor dwelled on. Emit world-axis alignment rays through it. Snap when the cursor crosses one. This needs a small dwell timer to avoid noise, the same way SketchUp resolves this. | New state on `TransSnap`, `transform.hh:541-575` |
| 3, hard | Inferred intersection of two synthesized lines | This is the highest-value, highest-effort feature. It generates a short list of temporary infinite lines per frame (extensions, guides, acquired points) and intersects pairs against the cursor. This matches the `snapTestIObj` layer from the 1992 Ashlar patent, and lets a user click where two walls would meet. This remains genuinely unsolved prior art. See [Construction Lines teardown](./construction_lines_addon_teardown.md), sections 4 and 7. The closest add-on only intersects persistent, user-named guide pairs, never two ad-hoc extension rays. | New provider dispatched from `snap_obj_fn()`, `transform_snap_object.cc:947-1012`, plus line-line math with BLI's existing `isect_line_line_v3` |
| 2, medium | Rotated or oriented construction grid | `sl_ct` already proves this is tractable in Python: bind the grid to the active transform orientation matrix instead of world XY. | `transform_snap_object.cc:1032-1078` (`snap_grid()`) |
| 4, not recommended | Live constraint-graph AutoConstraint (FreeCAD Sketcher-style) | Coincidence, tangency, and symmetry constraints solved into a persistent graph are a different subsystem from cursor-to-point snapping. Out of scope unless a dedicated precision-sketch tool is added later. | Not applicable |

See [Construction Lines teardown](./construction_lines_addon_teardown.md) for
how that add-on's line-intersection engine bears directly on tier 3.
