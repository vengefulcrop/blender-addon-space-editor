# Precision and Inference Snapping

A research dossier on CAD-style precision and inference snapping for
Blender's viewport transform system. Read-only investigation, branch
`pyareas/compositor-viewport-camera`, compiled 2026-08-10. Covers object
mode, edit mode, four external CAD systems, and two third-party Blender
add-ons.

An HTML version of this dossier, with a sticky table of contents, is saved at
`docs_cad/precision-snapping-dossier.html`.

- [Synthesis and Verdict](./synthesis_and_verdict.md) — the overall verdict,
  the ranked build order, and the `eSnapMode` bit-budget constraint.
- [Object-Mode Snapping](./object_mode_snapping.md) — the shared
  `TransInfo::tsnap` state machine, from mouse move to applied delta.
- [Edit-Mode Snapping](./edit_mode_snapping.md) — how edit mode differs, and
  the topology-aware inference already in edge slide and the knife tool.
- [External Prior Art](./external_prior_art.md) — FreeCAD, SketchUp, AutoCAD,
  Rhino, Fusion 360, and the foundational Ashlar patent.
- [`sl_ct` Add-on Teardown](./slct_addon_teardown.md) — a fully independent
  modal transform and detection system.
- [`Construction Lines` Add-on Teardown](./construction_lines_addon_teardown.md)
  — a construction-geometry add-on with screen-space guide intersection.
- [Extension-Point Map](./extension_point_map.md) — the concrete files and
  functions a native implementation would touch.
