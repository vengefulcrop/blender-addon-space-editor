---
type: research
title: "Extension-Point Map for Inference Snapping"
description: "Consolidated list of concrete files and functions a native inference-snapping implementation would touch"
tags: [snapping, transform, extension-points]
last_updated: 2026-09-12
---

# Extension-Point Map for Inference Snapping

See
[`Construction Lines` Add-on Teardown](./construction_lines_addon_teardown.md)
for the previous topic, and
[Synthesis and Verdict](./synthesis_and_verdict.md) for the section index.

Consolidated from all research passes. This lists the concrete files and
functions a native implementation would touch, independent of which tiered
feature (see [Synthesis and Verdict](./synthesis_and_verdict.md)) is being
built.

## Bit-budget constraint on `eSnapMode` (verified 2026-08-10)

`eSnapMode` is declared `enum eSnapMode : short` (`DNA_scene_types.h:1928`)
and is stored as such in `ToolSettings` (`snap_mode`, `snap_uv_mode`,
`snap_anim_mode`, `snap_playhead_mode`, `DNA_scene_types.h:2346-2349`). The
highest bit currently used is `SCE_SNAP_TO_FACE_MIDPOINT = (1 << 11)`.

A signed 16-bit `short` safely exposes bits 0 through 14 (bit 15 is the sign
bit), so exactly three free bits remain: 12, 13, and 14.

This directly constrains the tiered plan in
[Synthesis and Verdict](./synthesis_and_verdict.md), which implies at least
four new user-visible snap categories: extension, inferred intersection,
parallel/perpendicular guide, and tangent. Three options:

1. **Group under one bit.** A single `SCE_SNAP_TO_INFERRED` (or
   `..._CONSTRUCTION`) mode bit, with which inference behaviors are enabled
   carried in a separate new `ToolSettings` flag field. Cheapest, no
   widening, and arguably better UX, one toggle for "smart snapping," with
   sub-options in a popover. **Recommended.**
2. **Spend the remaining three bits** on the highest-value modes only
   (extension, intersection, guides), and accept that the enum is then full.
3. **Widen `eSnapMode` to `int`.** Requires DNA versioning for every
   `snap_*_mode` field, RNA enum updates, and care with the
   `SCE_SNAP_TO_VERTEX`/`SCE_SNAP_TO_GEOM` composite macros, which the header
   explicitly warns are value-sensitive
   (`DNA_scene_types.h:1963-1973`: "The exact value here is used in an enum,
   any changes require versioning.").

Note the sibling field `snap_node_mode` is a plain `char`
(`DNA_scene_types.h:2344`), so any bit above 7 is already unavailable there.
Not relevant to 3D-viewport work, but worth knowing before treating
`eSnapMode` as uniformly wide.

## Extension points

| Extension point | Location | Use for |
|---|---|---|
| New `eSnapMode` bit | `DNA_scene_types.h:1928-1961` + `rna_scene.cc:150-186` | Any new snap-target category exposed to UI/Python. Needs DNA versioning, the enum is on-disk. Only 3 bits remain, see the bit-budget constraint above. |
| New provider beside `snap_object_mesh` | `transform_snap_object.cc:947-1012` (dispatch) | Synthetic/construction candidates (extensions, guides, inferred intersections). Reuses visibility filtering and per-object dispatch for free. |
| `SnapData::snap_point` / `snap_edge` | `transform_snap_object.cc:193-223` | Feed any newly-computed 3D point through here to get screen-distance ranking plus clip-plane rejection uniformly. |
| `nearest_world_tree()` | `transform_snap_object.cc:711-766` | Template for offset-surface or multi-step "closest surface" queries, already generalized with raymarch stepping. |
| `transform_constraint_get_nearest()` | `transform_constraints.cc:374-460` | Where "snap along constrained axis until it meets X" logic already lives. Generalize from one resolved hit to several candidates. |
| `TransMode_snapsource` | `transform_mode_snapsource.cc` | Existing UX precedent for "interactively pick your own alignment point," a reusable pattern for custom anchors. |
| Local `BMBVHTree` pattern | `transform_mode_edge_slide.cc:198-260` | Template for any new per-vertex inference that must run at interactive rates during a modal drag, bypassing the scene-wide context. |
| `SnapObjectParams` | `ED_transform_snap_object_context.hh:61-83` | Natural place for new query knobs (offset distance, "include construction lines"), already threaded through every call site. |
| `isect_line_line_v3` / `closest_to_line_v3` (BLI `math_geom.c`) | BLI, existing | Closed-form skew-line closest-point intersection, already ships natively. Both add-ons in [`sl_ct`](./slct_addon_teardown.md) and [`Construction Lines`](./construction_lines_addon_teardown.md) independently reimplement this in Python. Use directly as the core of an "intersect two inferred lines" candidate generator. |
| Brute-force small-N pairwise intersection | New, add-on-precedented in [`Construction Lines`](./construction_lines_addon_teardown.md) | For the handful of inferred/construction lines actually active at once, not scene-wide geometry, an O(n squared) pairwise test against a BVH is unnecessary. Both add-ons confirm this is fine at CAD-inference scale. Reserve BVH/`SnapObjectContext` reuse for inferred-line-versus-real-geometry intersection only. |

## What is still genuinely unsolved

Neither add-on, nor any external system surveyed in
[External Prior Art](./external_prior_art.md), implements the specific tier-3
feature this research is chasing: synthesizing two unbounded inferred lines on
the fly and intersecting them, without the user first placing persistent
construction objects. `Construction Lines` gets close. Its guide-by-guide
intersection ([teardown](./construction_lines_addon_teardown.md), section 4)
does exactly this, but only for guides the user explicitly created and named
beforehand, and its single-ray extension inference never combines two rays
into an intersection candidate. This confirms tier 3 as real design work: the
pieces, the line-line intersection primitive, screen-space ranking merged
with real geometry, and single-ray inference vector math, all have working
prior art to draw on, but their fusion into "ad-hoc dual-ray extension
intersection" does not yet exist anywhere surveyed.
