---
type: research
title: "Construction Lines Add-on Teardown"
description: "Architecture teardown of the Construction Lines third-party Blender add-on, and why its dual-line intersection inference remains unsolved"
tags: [snapping, addon-teardown, cad]
last_updated: 2026-09-12
---

# `Construction Lines` Add-on Teardown

See [`sl_ct` Add-on Teardown](./slct_addon_teardown.md) for the previous
topic, and [Extension-Point Map](./extension_point_map.md) for the next.

Add-on location:
`%APPDATA%\Blender Foundation\Blender\5.3\scripts\addons\Construction Lines`
(Daniel Norris, DN Drawings, GPL, v0.9.965).

A second, independent Blender add-on implementing CAD-style construction
geometry and its own snapping. It is architecturally very different from
[`sl_ct`](./slct_addon_teardown.md), and closer in spirit to the original
research question: auxiliary infinite lines you draw to help place real
geometry.

## 1. Architecture

A single always-on modal operator, `VIEW_OT_ConstructionLines`
(`construction_lines28.py:108`), registered on a global "Window" keymap bound
to `MOUSEMOVE`/`ANY` plus an `Alt+\`` activation shortcut. A class-level
`running` flag ensures only one instance is ever live, and it self-cancels on
file load. On invoke it registers two `SpaceView3D.draw_handler_add`
callbacks, one for 3D overlay geometry (guides, shapes, highlights), one for
the 2D pixel-space HUD. This is a standard "persistent-feeling modal operator
plus draw handlers" add-on technique. There is no native gizmo or
manipulator, and no C-level integration.

## 2. Construction-geometry data model: two independent representations

**a) Guides**, the actual construction-line objects, are not custom mesh data
and not an in-memory-only overlay. Each guide is a pair of real
`bpy.types.Object` Empties (`empty_display_type="PLAIN_AXES"`), created in
`cl_clobject.py:297-320`. The two Empties reference each other through a
custom property, `obj["Pair"] = other.name`, plus `obj["Horizontal"]`/
`StartX/Y/Z`. Object names are prefixed `_CL_` (`cl_utils.py:54`), and guides
are enumerated by filtering scene objects on that prefix. A guide's line is
reconstructed on demand from the two Empties' `.location`. There is no
dedicated guide-object type, just two markers plus a naming and property
convention. Guides persist in the scene and undo stack as ordinary objects.

**b) Shapes** (Line, Circle, Rectangle, Arc tool output) are driven by
`cl_clobject.ConstructLine`, a plain Python object holding a list of
control-point `Vector`s. While dragging, this is pure in-memory state. On
completion it is baked directly into a real mesh object through bmesh
(`cl_meshedit.py:238-353`). Shapes go straight from ephemeral state to
permanent mesh geometry, with no intermediate "construction shape" object
type.

An explicit "Convert To Geometry" (K) context-menu action bridges the two,
promoting a selected guide (Empty pair) into a real mesh line object, the
deliberate, manual "construction line to real geometry" workflow.

**"Infinite" is faked, not real.** A guide is a bounded segment. The default
length is a plain constant (`CL_DEFAULT_H_GUIDE_LENGTH = 100`, user-tunable up
to 10000). There is no shader, far-plane, or geometry-shader trick producing a
genuinely unbounded line. "Infinite" is simulated by making the segment long
enough to look infinite at normal zoom. This is a gap to fix in a native port,
not a technique to copy.

## 3. Intersection detection

`cl_mesh_intersections.py` operates on real bmesh edges, not guide pairs. Its
job is keeping the mesh topologically valid when a user draws a new real edge
across existing geometry:

- `calc_edges_intersect` computes an intersection through
  `mathutils.geometry.intersect_line_line` (closest points on two 3D lines,
  midpoint of the pair), bounds-checked through `intersect_point_line` plus a
  percent-in-`[0, 1.001]` test. This is a closed-form analytic 3D line-line
  intersection (skew-line closest-point method), not a robust exact-intersection
  test.
- `split_edges_at_intersections` is brute-force O(n): it iterates every other
  `BMEdge` in the bmesh, or a caller-supplied subset, collects valid
  intersections, orders them along the new edge, and rebuilds both edges as
  sub-segments through `bmesh.utils.edge_split`. There is no BVH or spatial
  index.
- `cl_graph.mesh_graph` builds an adjacency graph and does recursive DFS
  cycle-finding to auto-generate faces from closed loops of newly-split wire
  edges, SketchUp-style "auto-face on closed loop," but a face-inference
  feature orthogonal to snapping.

Guide-by-guide intersection is a separate code path, in `cl_utils.py:1310-1339`
(`return_guide_intersections`). See section 4.

## 4. Snap logic, the most relevant section

Snapping is entirely screen-space (2D pixel distance), not 3D. Each frame,
`snap()` (`cl_snapto.py:364-429`) gathers real mesh geometry near the cursor
plus all guide geometry converted into candidates (vertex/endpoint,
along-segment, midpoint). Critically, it also calls
`return_guide_intersections(edges, guides)`, a brute-force pairwise loop where
every guide-or-real-edge crosses every guide with the same analytic line-line
primitive as section 3, bounds-checked so the crossing must fall strictly
within both finite segments. Each valid crossing becomes a new vertex-type
snap candidate, merged into the same ranked candidate list as literal
vertices, genuinely first-class, not a lower-priority category.

Ranking is a fixed cascade, each stage on squared 2D pixel distance:
vertex-like candidates first, then edge/line snapping (a circle-versus-
projected-line test recovers where along an extended guide the cursor is),
then global axes (synthesized plus-or-minus-100-unit axis edges through the
origin, reusing the edge-snap code path), then face raycast (compared by true
3D camera distance, the one exception), then grid.

On top of literal snapping, a separate axis-inference or soft-constraint
layer (`return_highlights_and_soft_constraints`) inspects the start geometry
of the in-progress line and, if it started on an edge or guide, adds two
inferred direction candidates: perpendicular-from-edge (closed-form
foot-of-perpendicular construction), and edge-extension
(`±edge.dir.normalized()`, tagged `"EXTD"`, drawn in a distinct purple). These
are ranked against literal geometry by dot-product-with-tolerance, with XYZ
world axes preferred over perpendicular on ties.

**Important gap, not just a gap in Blender.** Extension inference here only
ever produces a direction constraint for the currently-dragged line. It never
synthesizes a full infinite line that a second inference ray can then be
intersected against. Multi-line inferred intersection only happens through
the persistent-guide route (sections 3 and 4), which requires the user to
have deliberately created named guide objects first. The specific "extend two
rays, snap to where they would cross" behavior, the actual feature this
research is chasing (see [External Prior Art](./external_prior_art.md) for
FreeCAD, SketchUp, and Ashlar-patent coverage), is absent even in this
add-on. It is not solved prior art here. It is still net-new design work.

## 5. Extrude and mesh-edit workflow

Two workflows both terminate in `cl_mesh_intersections.py`: draw-and-cut (a
new edge drawn across a face triggers split, auto-face-close, and
face-into-face cut) and push/pull extrude (`cl_extrude.py`, SketchUp-style
face push/pull along its normal with boolean cutting through intervening
geometry, followed by a post-extrude re-intersection pass). Guides feed both
only indirectly, as snap targets while placing points. They are never
automatically consumed by the extrude or cut pipeline, only through the
manual "Convert To Geometry" action.

## 6. Rendering technique for "infinite" lines

Not a special infinite-line technique. `cl_gpushaders.py` defines one custom
dashed-line shader: a per-vertex cumulative `lineLength` attribute, summed
CPU-side, feeds a fragment shader doing `mod(lineLength, u_DashSize) <
u_Spacing` for the dash pattern, with dash scale multiplied by
`region_3d.view_distance` to stay visually consistent under zoom. Guides are
drawn as ordinary finite `LINE_STRIP` batches between their two literal
world-space endpoints through the standard perspective matrix. There is no
far-clip-plane trick, no geometry-shader billboarding, and no clip-space
direction-only projection.

## 7. Assessment, portability to a native `transform_snap_object.cc` provider

| Piece | Portability | Notes |
|---|---|---|
| Data model (guide = pair of Empties plus custom props) | Not portable | Add-on-specific object hack. A native port needs runtime "inferred/synthetic geometry" state held per-modal-operator, analogous to `SnapObjectContext`, not persistent scene objects. |
| Guide "infiniteness" (fixed long finite segment) | Not portable, a gap to fix, not copy | Never solved true infinite-line math or rendering. A native port should do genuine unbounded parametric-line semantics, clipped only by the viewport frustum. |
| Line-line intersection primitive | Directly portable, algorithmically | Closest-points-between-skew-lines plus percent-in-range is exactly `isect_line_line_v3`/`closest_to_line_v3` territory already in Blender's `math_geom.c`. Good template for a native "intersect two inferred lines" candidate generator, trivial to reimplement since the primitive already ships. |
| Brute-force O(n) intersection search | Portable as a starting point | Fine for the small N of user-drawn guides or inference rays actually in play at once. Real-geometry-versus-inferred-line intersection should still reuse the existing `SnapObjectContext`/BVH rather than brute force against all scene edges. |
| Cycle-finding graph (auto-face) | Portable but orthogonal to snapping | A face-inference feature, not a snap-provider feature. Low priority for the stated goal. |
| Snap integration architecture (synthesized intersections merged into the same ranked list as real vertices, no separate "inferred" tier) | Most valuable prior art, re-architect, do not transliterate | Maps directly onto extending `transform_snap_object.cc` so a construction-geometry provider injects temporary line/point records that get ranked identically to BVH-derived real geometry, per the tier-3 recommendation in [Synthesis and Verdict](./synthesis_and_verdict.md). |
| Axis-inference / soft-constraint vector math (perpendicular-foot, extension direction, tolerance ranking) | Directly portable algorithm | About 30-line closed-form functions, no Python-specific dependencies, translate almost line-for-line to C. Good prior art for single-ray inference, but stops short of dual-ray intersection, see section 4. |
| Rendering (dash shader, finite segment batches) | Not relevant | Blender's own overlay and snap-cursor drawing already exceeds this. No infinite-line shader trick to borrow. |

**Bottom line.** This add-on's most transferable idea is architectural:
injecting synthesized intersection points into the same ranked candidate list
as real geometry, not a separate lower-priority path, plus its closed-form
line-line intersection and single-ray inference math, both already close to
native-ready through existing BLI primitives. It does not already solve the
specific feature this project is after: synthesizing unbounded inferred lines
and intersecting two such inferred lines on the fly, without the user first
placing persistent guide objects. Combined with
[`sl_ct`](./slct_addon_teardown.md)'s constraint-aware analytic resolution and
oriented grid, and the [Ashlar patent](./external_prior_art.md)'s
`snapTestIObj` formalization, this confirms tier 3 in
[Synthesis and Verdict](./synthesis_and_verdict.md), dual-inference-line
intersection, as genuinely new design work with no complete off-the-shelf
algorithm to port, only component parts.
