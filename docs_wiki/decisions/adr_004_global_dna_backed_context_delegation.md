---
type: decision
title: "ADR 004: Global, DNA-Backed Context Delegation vs a Layout-Scoped Swap"
description: "Store the delegate space type on ScrArea and route all typed context accessors through it, instead of swapping context only around panel layout"
tags: [decision, addon-editor, context, delegation]
last_updated: 2026-09-12
---

# ADR 004: Global, DNA-Backed Context Delegation vs a Layout-Scoped Swap

## Context

Re-hosted panels declare `bl_space_type = 'VIEW_3D'` (or another type) and
read that editor's context. `context.space_data`, `context.region_data`,
and `poll()` checks against `context.space_data.type` all fail under an
unmodified `SpaceAddon` context.

## Decision, and the path to it

1. **Rejected outright**: a `SpaceType.context` callback on `SpaceAddon`.
   `CTX_wm_space_data` reads `area->spacedata.first` directly with no hook
   point (`context.cc:959-963`), so a context callback cannot intercept it.
2. **Tried, insufficient**: swap the context's area and region for a real
   editor of the panel's declared type, only around
   `ED_region_panels_layout_ex`. This made panels draw, but menus opened
   from a panel, and operator polls run at button-press time, still saw
   the un-delegated `SpaceAddon`.
3. **Shipped**: `ScrArea::context_delegate_spacetype` (a DNA field,
   originally on `SpaceAddon`, later generalized). `ctx_wm_area_effective()`
   in `context.cc` resolves it by type via `BKE_screen_find_big_area`,
   never by a stored pointer, so closing the borrowed editor cannot dangle.
   All 18 typed space accessors route through it.
4. **Added**: a separate `addon_context()` forwards unresolved context
   *members* (for example, `selected_nodes`) to the delegate editor's own
   `SpaceType.context` callback, since these members use a mechanism
   distinct from `space_data`.

## Consequences

- This fixed menus and C-side operator polls, which the layout-scoped swap
  did not cover.
- `ED_area_newspace()` must reset `context_delegate_spacetype` to
  `SPACE_EMPTY` on any area-type change. Otherwise a stale delegate
  silently redirects a newly-switched area's context lookups to an
  unrelated area, which crashed before this fix.
- Every code path that needs `SpaceAddon`'s own state must read
  `context.area.spaces.active`, never `context.space_data`, since the
  latter always resolves through the delegate, in every region and
  callback, not only during panel layout.
- This does not solve modal-operator invocation (deferred, see
  [Context Delegation](../architecture/context_delegation.md#what-this-still-does-not-fix-modal-operators))
  or per-panel delegate resolution for mixed-editor add-ons (deferred, see
  [Context Delegation](../architecture/context_delegation.md#single-delegate-per-area-not-per-panel)).

## Related

- [Context Delegation](../architecture/context_delegation.md)
- [ADR 005](./adr_005_context_delegation_scoped_to_panel_callbacks.md)
