---
type: decision
title: "ADR 003: Panel Attribution via Registration-Time Module Capture vs a Python-Push Registry"
description: "Derive add-on ownership of a panel on demand from the panel's own Python class, instead of maintaining a pushed C++ registry"
tags: [decision, addon-editor, panels, attribution]
last_updated: 2026-09-12
---

# ADR 003: Panel Attribution via Registration-Time Capture vs a Python-Push Registry

## Context

`PanelType::owner_id` is the workspace-filter ID (`wmOwnerID`), not an
add-on identifier. Nothing can repurpose it without breaking workspace
filtering. The reliable identity is the Python class's `__module__`, which
is Python-side data. The original plan concluded that C++ had to compute
this mapping in Python and push it into a small C++ registry
(`addon_panel_registry.cc`, about 120 LOC), plus an RNA push API. Python
could then also read extension manifests for display names.

## Decision

C++ derives attribution on demand, without a Python-side push. The first
implementation added a hook to `rna_Panel_register`
(`makesrna/intern/rna_ui.cc`). The hook called `BPY_class_module_name_get()`
for every panel registered anywhere in Blender, and stored the result in
`PanelType::addon_id`. The final implementation has no hook and no stored
field. `space_addon.cc`'s `addon_panel_owner_get()` calls
`BPY_class_module_name_get()` directly on `PanelType::rna_ext.data`. It
does this only while it collects panels for the Add-on Editor's own use.

## Alternatives considered

- **Python-push registry with a cross-language RNA API**: rejected once it
  became clear that `makesrna` already links Python under `WITH_PYTHON`.
  The cross-language plumbing was not necessary.
- **Registration-time hook in `rna_Panel_register`**: shipped first, then
  reverted. It taught a core, shared code path (panel registration for
  every panel in Blender) the Add-on Editor's own bookkeeping need. The
  whole panel system paid this cost for one editor's benefit.

## Consequences

- This removed the C++ registry and the Python push API entirely. It
  eliminated a class of staleness bugs, since a pushed mapping could
  otherwise drift from the live class table.
- This removed the `rna_Panel_register` hook. `blenkernel` and `makesrna`
  no longer mention this editor's bookkeeping at all. `rna_Panel_register`
  does exactly what it did before this fork touched it.
- Attribution now costs time only when the Add-on Editor collects panels,
  not on every panel registration in Blender.
- A full rebuild produced zero new compiler errors or warnings. The
  resolution order and fallback semantics of context delegation, which
  depend on attribution, stayed unchanged by either refactor.

## Related

- [Add-on Panel Attribution](../architecture/addon_panel_attribution.md)
- [Fork Mergeability](../architecture/fork_mergeability.md)
