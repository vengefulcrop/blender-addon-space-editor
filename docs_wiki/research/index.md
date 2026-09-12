# Research and References

Audits of native Blender equivalents, a precision-snapping research dossier,
and a sky-rendering study.

- [Audits](./audits/index.md) — merged internal and external audits of the
  Add-on Space Editor branch: hand-rolled code versus native Blender
  primitives, pruning candidates, verified native APIs, and the reconciliation
  of all four source audits.
- [Precision and Inference Snapping](./precision_snapping/index.md) — a
  research dossier on CAD-style precision and inference snapping for
  Blender's viewport transform system, covering object mode, edit mode,
  external CAD prior art, two add-on teardowns, and a native extension-point
  map.
- [Custom GLSL Sky and Cloud Rendering via Add-ons](./sky_glsl_rendering.md) —
  how an add-on can implement a fully custom raymarched sky renderer outside
  Cycles and Eevee, using only the public `bpy`/`gpu` API.
