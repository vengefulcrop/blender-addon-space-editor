---
type: decision
title: "ADR 009: Bundled Add-ons as an Opt-In Preference, Not a Filter"
description: "Show Blender's own bundled add-ons in the picker only when the user opts in, instead of hiding them because they appear to draw nothing"
tags: [decision, addon-editor, ux, picker, preferences]
last_updated: 2026-09-12
---

# ADR 009: Bundled Add-ons as an Opt-In Preference, Not a Filter

## Context

The picker initially showed only Blender's own bundled tooling (Cycles,
Pose Library, format importers/exporters). None of them appeared to draw
anything when picked. One option was to exclude these from the picker
entirely, the same way the fork already excludes panel-less and disabled
add-ons.

Before building that filter, the fork checked `CyclesButtonsPanel`. It
gates on `COMPAT_ENGINES = {'CYCLES'}` (`cycles/ui.py:60-64`). Its panels
only draw when Cycles is the active render engine, unrelated to which
editor is open. Pose Library has its own scene-state `poll()`, with the
same behavior. Both are legitimate, working `poll()` conditions. Switching
the render engine to Cycles and watching the panels draw confirmed this
directly.

## Decision

`Preferences.show_addon_editor_bundled`, off by default, with a checkbox in
`USERPREF_PT_addon_editors`, adds the opt-in. The distinction is not "will
this draw right now", since that is unanswerable without predicting
`poll()`. The distinction is "is this Blender's own bundled tooling or
something the user installed", a deterministic, path-based fact.
`_addon_is_bundled()` checks whether the module's `__file__` contains
`addons_core`, versus a user's Extensions or legacy add-ons directory.
`cycles`, `pose_library`, and `io_scene_gltf2` resolve under
`.../5.3/scripts/addons_core/...`. Installed Extensions resolve under
`%APPDATA%\...\extensions\<repository>\...` and always import as
`bl_ext.*`, never `addons_core`.

## Alternatives considered

- **Excluding bundled add-ons from the picker permanently**: rejected. It
  would discard real, functioning capability for anyone who wants to host
  Cycles or Pose Library. It would discard that capability over a
  condition, which render engine is active, that this fork has no business
  predicting. The same reasoning keeps dynamic, poll()-probing context
  routing absent from
  [Context Delegation](../architecture/context_delegation.md#should-context-routing-be-dynamic-resolved-per-operator-call).

## Consequences

- No `poll()` prediction happens anywhere in the filter. The distinction is
  purely path-based and deterministic.
- The `UserDef` struct shape does not change. The flag reuses an existing
  spare bit, `eUserpref_UI_Flag2`'s `USER_UIFLAG2_UNUSED_2`, renamed to
  `USER_ADDON_EDITOR_SHOW_BUNDLED`, at the same bit position, instead of
  adding a new field. `uiflag` was already full at 32 of 32 bits before
  this fork touched it.
- Residual risk exists. If a future upstream Blender version claims this
  same bit for an unrelated flag, a rebase produces a visible merge
  conflict on that one enum line. The conflict is ordinary, not silent
  corruption.
- Bundled add-ons, when enabled, are listed at the very top of the picker.

## Related

- [UX: Editor-Type Picker](../design/ux_addon_picker.md)
- [Persistence and Compatibility](../architecture/persistence_and_compatibility.md#the-user_addon_editor_show_bundled-bit-and-its-own-dna-cost)
