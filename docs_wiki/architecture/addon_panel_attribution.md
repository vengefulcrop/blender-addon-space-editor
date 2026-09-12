---
type: architecture
title: "Add-on Panel Attribution"
description: "How the editor decides which add-on owns a given PanelType"
tags: [architecture, addon-editor, panels, attribution]
last_updated: 2026-09-13
---

# Add-on Panel Attribution

## The gap

C++ has no built-in way to identify which add-on owns a `Panel`.
`PanelType` has an `owner_id` field at `BKE_screen.hh:383`:

```cpp
/** For work-spaces to selectively show. */
char owner_id[128];
```

This is the workspace filter ID (`wmOwnerID`). The panel author sets it
through `bl_owner_id`, to filter the UI by workspace. Most add-on panels
leave it empty, and it is not an add-on identifier. Reusing it breaks
workspace filtering.

The reliable add-on identity is the Python class's `__module__` — its
top-level package matches the module name `addon_utils` enables.

## Rejected: a Python-push registry

The original plan computed the add-on-to-panels mapping in Python, then
pushed it into a small C++ registry. Python read the extension manifest
for a display name, and called `addon_utils.modules()` for the picker.

That step is unnecessary. `makesrna` already links Python under
`WITH_PYTHON`, so `rna_Panel_register` captures the owning module directly
through `BPY_class_module_name_get()`. This removes the C++ registry and
the Python push API. It also removes a class of staleness bugs. A pushed
mapping records the state at registration time, and then drifts from the
actual class table.

## Attribution moved out of core

`arch~panel-owner-on-demand~1`

Needs: impl

The first working version populated `PanelType::addon_id` by adding a hook
to `rna_Panel_register` in `makesrna/intern/rna_ui.cc`, calling
`BPY_class_module_name_get()` for every panel registered anywhere in
Blender. This was a cost paid by the whole panel system for one editor's
bookkeeping.

The field and the registration-time hook were both removed.
`space_addon.cc` now derives the same attribution on demand, only while
collecting panels for its own use, through `addon_panel_owner_get()`,
which calls the existing `BPY_class_module_name_get()` directly on
`PanelType::rna_ext.data` — the Python class every registered panel
already carries. `rna_Panel_register` is back to doing exactly what it did
before this fork touched it.

Both changes were validated by a full rebuild with zero new compiler
errors or warnings. Neither change alters when or how delegation resolves,
only where the same data is stored and when the same function is called.

## Related

- [Panel Hosting](./panel_hosting.md)
- [Fork Mergeability](./fork_mergeability.md)
