---
type: decision
title: "ADR 005: Narrowing Context Delegation to Panel Callbacks"
description: "Scope the context/area swap to each panel's own poll/draw callbacks via a RAII override, instead of swapping around the whole layout pass"
tags: [decision, addon-editor, context, delegation, region-layout]
last_updated: 2026-09-12
---

# ADR 005: Narrowing Context Delegation to Panel Callbacks

## Context

`addon_main_region_layout` swapped `CTX_wm_area`/`CTX_wm_region` to the
delegate around the entire `ED_region_panels_layout_ex` call. This produced
two bugs traced to the same cause. First, the editor's own
`ADDON_PT_empty_state` fallback panel read `context.area` and got a
foreign space when a delegate resolved but no panel actually needed it.
Second, resizing a hosted panel did not make other panels reflow, because
the re-align animation's handler bound to the borrowed region instead of
the Add-on Editor's own region.

## Alternatives considered and rejected

1. **Make `panel_activate_state()` take the region explicitly.**
   Insufficient alone: `panel_handle_data_ensure()` registers the
   animation handler through `WM_event_add_ui_handler()`, which snapshots
   `CTX_wm_area`/`CTX_wm_region` into the handler at registration time
   (`wm_event_system.cc:5205-5208`). The handler stayed bound to the
   borrowed region regardless.
2. **Wrap the callbacks on the editor's own `PanelType` copies.** This
   cannot reach sub-panels: the copies are shallow, so a copy's `children`
   list still refers to the registered type's own children, and
   `ed_panel_draw()` recurses into those registered types directly.

## Decision

Add an optional `PanelDrawContextOverride` parameter to
`ED_region_panels_layout_ex()` (`ED_screen.hh`): an area and/or region
applied only around each panel's `poll`, `draw`, `draw_header`, and
`draw_header_preset` callbacks, via a scoped RAII applier that restores
what it saw. This threads through `ed_panel_draw`'s child recursion
(covering sub-panels by construction) and through `panel_add_check`
(covering top-level polls). The parameter defaults to null, so the five
other editors that call this function stay unaffected.

## Consequences

- Panel alignment, region size updates, search-filter lookup, and handler
  registration now run with the region they were actually called for. This
  fixes the reflow bug at its root rather than at one call site.
- This retired the `only_fallback_panel` special case entirely: the crash
  it guarded against became structurally impossible, since nothing but a
  borrowing panel's own callback now sees the delegate.
- This does not address modal-operator invocation, which fails at
  button-invoke time, before this swap ever runs, since
  `WM_event_add_modal_handler` snapshots the un-delegated area and region.
  That remains a separate, deferred problem.

## Related

- [Context Delegation](../architecture/context_delegation.md#context-delegation-narrowed-to-panel-callbacks)
- [ADR 004](./adr_004_global_dna_backed_context_delegation.md)
