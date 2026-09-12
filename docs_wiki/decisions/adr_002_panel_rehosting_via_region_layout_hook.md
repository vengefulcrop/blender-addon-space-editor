---
type: decision
title: "ADR 002: Panel Re-Hosting via Region Layout Hook vs a New Drawing Stack"
description: "Reuse ED_region_panels_layout_ex with a filtered panel-type list, instead of building a new UI-block drawing stack"
tags: [decision, addon-editor, panels, region-layout]
last_updated: 2026-09-12
---

# ADR 002: Panel Re-Hosting via Region Layout Hook vs a New Drawing Stack

## Context

The original design (`legacy/custom_python_space_types_architecture.md`) proposed a new editor module.
That module wraps region rendering with `ui::block_begin` / `ui::block_end` and invokes Python `draw(context)` directly.
Estimated at ~450-600 LOC, this was the largest single item in that design total.

## Decision

Pass a filtered `ListBaseT<PanelType>` of the chosen add-on panels into the existing `ED_region_panels_layout_ex()`, at `area.cc:3439-3367`.
This function already accepts an arbitrary panel-type list rather than being hard-wired to a region list.
The Properties editor already exploits this to swap panel sets per tab (`space_buttons.cc:315`).

## Alternatives considered

- **A new drawing stack invoking Python `draw()` directly**: rejected.
  Would require manually reimplementing panel headers, open/closed state, drag-to-reorder, category tabs, panel search, and background drawing — all of which `ED_region_panels_layout_ex` already supplies.

## Consequences

- No manual UI-block management and no direct Python `draw()` invocation.
  The editor inherits panel headers, open/closed state, drag-to-reorder, category tabs, panel search, and background drawing for free.

- Panel types cannot be re-linked into a second list without corrupting the first.
  `ED_region_panels_layout_ex` walks its list through `PanelType` own `next`/`prev` fields.
  Every registered type is already a member of its home region type list.
  The editor keeps private shallow copies instead, invalidated via `BKE_paneltypes_tag_changed()`/`BKE_paneltypes_state_get()`.

- The developer split layout and draw into separate callbacks (`art->layout` vs `art->draw`), matching the Properties editor.
  Otherwise, widgets render at the wrong size using stale bounds from the previous frame.

- This collapsed the largest line-count item in the original estimate by roughly two-thirds.

## Related

- [Panel Hosting](../architecture/panel_hosting.md)
- [ADR 001](./adr_001_addon_space_type_vs_dynamic_registration.md)
