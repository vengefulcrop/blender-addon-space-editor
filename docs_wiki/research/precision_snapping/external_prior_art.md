---
type: research
title: "External Prior Art for Inference Snapping"
description: "Survey of FreeCAD, SketchUp, AutoCAD, Rhino, Fusion 360, and the foundational Ashlar patent for CAD-style inference snapping"
tags: [snapping, cad, prior-art]
last_updated: 2026-09-12
---

# External Prior Art for Inference Snapping

See [Edit-Mode Snapping](./edit_mode_snapping.md) for the previous topic, and
[`sl_ct` Add-on Teardown](./slct_addon_teardown.md) for the next.

Sources: FreeCAD wiki, SketchUp documentation, AutoCAD and Rhino
documentation, US Patent 5,123,087.

Four shipping systems and one foundational patent, surveyed for the specific
behaviors worth porting rather than for feature parity. All are variations on
the same idea: gather nearby real geometry, optionally synthesize temporary
construction geometry from it, rank candidates, and communicate the resolved
reason to the user.

## FreeCAD, Draft workbench (general-purpose 3D, not sketch-locked)

14 toggleable modes: Endpoint, Midpoint, Center, Angle, Intersection,
Perpendicular, Extension, Parallel, Special, Near, Ortho, Grid, Working Plane,
and Dimensions. Extension treats a finite edge as an infinite line for snap
purposes. Intersection finds where two edges, or two extensions, or an
extension and an Ortho guide, cross, even where neither exists in model
space. Explicitly designed to compose: Ortho plus Extension synthesizes
intersection candidates from two inferred lines.

Sources: [Draft Workbench](https://wiki.freecadweb.org/Draft_Module),
[Draft Snap Angle](https://wiki.freecadweb.org/Draft_Snap_Angle),
[Draft Perpendicular](https://wiki.freecadweb.org/Draft_Perpendicular),
[Draft Snap Midpoint](https://wiki.freecadweb.org/Draft_Snap_Midpoint),
[Draft Snap Near](https://wiki.freecadweb.org/Draft_Snap_Near),
[Draft Snap Center](https://wiki.freecad.org/Draft_Snap_Center)

## FreeCAD, Sketcher AutoConstraint (2D sketch plane, solver-backed)

Watches cursor proximity while drawing and offers Coincident,
Point-on-object, Horizontal-Vertical, Tangent, and Symmetric. A click both
places the point and commits the constraint into the sketch's live solver
graph. This is a fundamentally heavier mechanism than point-snapping: it
inserts a relationship that gets re-solved, not just a coordinate. Tier 4, out
of scope for this project (see
[Synthesis and Verdict](./synthesis_and_verdict.md)).

Sources: FreeCAD Sketcher Workbench documentation, AutoConstraint section;
GitHub issues [#16956](https://github.com/FreeCAD/FreeCAD/issues/16956) and
[#15494](https://github.com/FreeCAD/FreeCAD/issues/15494)

## SketchUp, Inference Engine (industry benchmark)

Point inferences, Endpoint, Midpoint, Center, On-Face, and Intersection, are
dwell-triggered: the cursor must pause near a feature before secondary points
like face-center are derived, a deliberate noise and performance tradeoff.
Linear inferences snap to world axes and to a magenta parallel or
perpendicular-to-edge axis, drawing a dashed guide line as confirmation. The
signature device: simultaneous multi-inference with an on-screen text label,
for example "Endpoint," "On Face," or "Perpendicular to Edge," so the user
always knows why a point locked, not just that it did.

Sources: [SketchUp Help, Drawing Basics](https://help.sketchup.com/en/sketchup/introducing-drawing-basics-and-concepts),
[MasterSketchUp, Inference System](https://mastersketchup.com/sketchup-inference/),
[3DsHouse, Snap Points and Inference Engine](https://3dshouse.com/how-to-snaps-points-in-sketchup-fastest/)

## AutoCAD OTRACK/Polar Tracking, and Rhino SmartTrack

AutoCAD Polar Tracking restricts the cursor to fixed angular increments from
the last point, with a dashed alignment path and angle tooltip. Object Snap
Tracking is the geometry-aware counterpart: hover to acquire a snap point, no
click needed, then horizontal, vertical, or polar alignment paths project
through it. Intersection or Apparent Intersection OSNAP can resolve where a
tracking path crosses real geometry, and multiple acquired points can be
tracked at once.

Rhino generalizes this into a persistent FIFO stack of accumulated smart
points. Hovering over an OSnap point registers it and draws temporary
infinite SmartLines through it for the duration of the current command.
Several accumulated points' guide lines intersect to pin an exact 3D target
with zero construction geometry ever drawn. SmartTrack is explicitly layered
on top of, and dependent on, OSnap being active.

Sources: [Polar Tracking and PolarSnap](https://help.autodesk.com/view/ACD/2024/ENU/?guid=GUID-7EC3C63D-EA4E-4E65-A676-C3A3627E3F19),
[Tracking Points Automatically Using Object Snaps](https://help.autodesk.com/cloudhelp/2019/ENU/AutoCAD-Core/files/GUID-665DC37F-8C3E-414A-9369-72A13C0BE07A.htm),
[SmartTrack, Rhino for Mac](https://docs.mcneel.com/rhino/mac/help/en-us/options/modeling_aids_smarttrack.htm),
[Rhino 3D Tip: SmartTrack workflow](https://novedge.com/blogs/design-news/rhino-3d-tip-smarttrack-workflow-for-precise-positioning-in-rhino)

## Fusion 360 (brief)

The sketch environment infers Midpoint and Horizontal/Vertical constraints
live in cyan while drawing, with Ctrl or Cmd as a hold-to-suppress modifier.
This is functionally a lighter-weight subset of FreeCAD's AutoConstraint,
tuned for speed over completeness.

Source: [Product Design Online, Fusion 360 Sketch Constraints](https://productdesignonline.com/tips-and-tricks/how-to-manually-add-sketch-constraints-in-fusion-360/)

## The lineage: Snap-Dragging (1986) and US Patent 5,123,087 (1992)

Bier's *Snap-Dragging* (SIGGRAPH '86, Vol. 20 No. 4, pages 233 to 240) is the
academic origin: "gravity-active points and alignment objects." Newell and
Fitzpatrick's *Geometric Inference Engine* patent (Ashlar Inc., filed 1990,
issued 1992) formalizes it as a single `ptSnap(p)` routine that is, point for
point, the algorithm every tool above still implements:

- Gather real geometry and temporary inference geometry inside a screen-space
  hitbox around the cursor (`snapTestObj` / `snapTestIObj`).
- Test candidates in a priority order driven by a user-overridable method
  string, for example `"miqp"` meaning midpoint, then intersection, then
  quadrant, then perpendicular. This is the direct precursor to "hold a
  modifier to force one snap type."
- On a near-miss, synthesize temporary extension lines, parallel and
  perpendicular projections, tangent lines, and offset lines purely to
  generate new candidate intersection points. See `FIG. 17B` and
  `FIG. 20A` through `20T` in the patent for step-by-step pseudocode.
- Display a secondary, decoupled cursor, an "X," at the resolved point, plus a
  text reason such as "align," "on," "tangent," "perpendicular," "intersect,"
  "grid," or "% point", the same label pattern SketchUp and Rhino both still
  use.

Source: [US5123087A on Google Patents](https://patents.google.com/patent/US5123087A)
(full text and figures).

This is the missing layer identified in
[Synthesis and Verdict](./synthesis_and_verdict.md): Blender has the ranking
and the primitive intersections, but nothing that synthesizes temporary
construction geometry as a snap-candidate source. See
[`Construction Lines` addon teardown](./construction_lines_addon_teardown.md)
for a Blender add-on that already implements a version of exactly this
synthesis step in Python.

## Synthesis: implementability tiers

| Tier | Behavior | Systems it comes from |
|---|---|---|
| 1 | Extension snapping, treat edge as infinite line | FreeCAD Draft Extension |
| 1 | Perpendicular-foot snapping to a reference | FreeCAD Draft Perpendicular |
| 1 | Near-miss "on-line" snapping (closest point on infinite/finite edge) | FreeCAD Draft Near, SketchUp edge-hover |
| 2 | Parallel/perpendicular-to-reference-edge guide line | SketchUp magenta axis |
| 2 | Axis-aligned alignment guides from a dwelled vertex | SketchUp red/green/blue dashed guides |
| 2 | Tangent-point inference to circles/arcs | Closed-form tangent-line math |
| 3 | Intersection of two inferred lines | FreeCAD Extension+Ortho, AutoCAD OTRACK+Intersection, Rhino SmartTrack |
| 3 | Object-snap-tracking with acquired-point history | AutoCAD OTRACK, Rhino SmartTrack |
| 4, out of scope | Live AutoConstraint / solver-graph constraints | FreeCAD Sketcher |
