---
type: architecture
title: "Context Delegation"
description: "How a re-hosted panel resolves space_data and related context by borrowing a real editor, and where that borrowing still fails"
tags: [architecture, addon-editor, context, delegation]
last_updated: 2026-09-12
---

# Context Delegation

## Why re-hosting mostly works without changes

The Add-on Editor re-hosts an add-on's existing, unmodified `Panel`
classes. Those panels declare `bl_space_type = 'VIEW_3D'` (or another
type) and read that editor's context. In practice most panels still
work, because Blender resolves `context.X` in layers: screen, then area,
then region.

The screen layer resolves most of what a typical N-panel reads. This
layer, in `screen_context.cc`, is editor-agnostic and works in any area:
`scene`, `object`, `active_object`, `selected_objects`,
`selected_editable_objects`, `editable_objects`, `visible_objects`,
`objects_in_mode`, `edit_object`, `sculpt_object`, `pose_object`,
`active_bone`, `active_pose_bone`, `selected_bones`, `annotation_data`,
`grease_pencil_data`, `active_operator`, and more.

An add-on panel that does `context.object.name` or
`context.scene.my_props` works in the Add-on Editor with zero changes.

## What breaks, and the auto-wiring fix

Only editor-specific lookups fail:

| Breaks | Symptom |
|:---|:---|
| `context.space_data` | Returns `SpaceAddon`; `.overlay`, `.shading`, `.region_3d` fail |
| `context.region_data` | No `RegionView3D` available |
| `poll()` checking `context.space_data.type == 'VIEW_3D'` | Returns `False`, panel silently vanishes |
| `bpy.ops.view3d.*` buttons | Operator poll fails, button greys out |

**Fix — context delegation.** The editor resolves a real editor of the
panel's declared type elsewhere in the screen, in this order:

1. the last editor of that type the user interacted with, if it is
   still present;
2. otherwise the biggest open editor of that type
   (`BKE_screen_find_big_area`);
3. otherwise unresolved.

If no matching editor exists anywhere in the workspace, affected panels
render a "requires a [editor type]" placeholder instead of crashing or
vanishing silently. See
[UX: Empty State and Header](../design/ux_empty_state_and_header.md).

## Three widening stages

The delegation mechanism went through three stages. Each stage proved
insufficient before the next.

### 1. Layout-scoped (rejected)

The original idea was a `SpaceType.context` callback on `SpaceAddon`.
This does not work: `CTX_wm_space_data` reads `area->spacedata.first`
directly with no hook point (`context.cc:959-963`), so a context
callback cannot intercept it.

The first working attempt swapped the context's area and region for a
real editor of the panel's declared type, only around the
`ED_region_panels_layout_ex` call. This made panels draw, but a menu
opened from a panel drew later, in its own popup pass, and any operator
poll run at button-press time saw the un-delegated `SpaceAddon` again
and raised an error.

### 2. Global, DNA-backed

`ScrArea::context_delegate_spacetype` (originally
`SpaceAddon::delegate_spacetype`, later generalized, see below) records
the borrowed editor type. `ctx_wm_area_effective()` in `context.cc`
resolves it by type, never by a stored pointer, so closing the borrowed
editor cannot dangle. All 18 typed space accessors (`CTX_wm_space_node`,
`CTX_wm_view3d`, and so on) route through it. This fixed menus and
C-side operator polls.

### 3. Context members

Editors also supply members like `selected_nodes` through their own
`SpaceType.context` callback, a mechanism separate from `space_data`. A
new `addon_context()` forwards unresolved lookups to the delegate
editor's own callback. That callback itself resolves through stage 2's
accessors and therefore operates on the borrowed editor's real data.

## Made generic: no core code names this editor

`ctx_wm_area_effective()` originally branched on
`area->spacetype != SPACE_ADDON` before reading the delegate field. The
field moved to a generic `ScrArea::context_delegate_spacetype`
(replacing 2 bytes of existing padding, no struct size change), and the
function now reads it directly with no spacetype check. `blenkernel` no
longer mentions `SPACE_ADDON` anywhere except in `CTX_wm_space_addon()`'s
own body, the same one-line pattern every other typed accessor uses for
its own type.

Resolution order and fallback semantics stay unchanged: current area
unless it declares a delegate, resolve by type via
`BKE_screen_find_big_area`, fall back to the area itself if nothing
matches. Only the field's owner and the accessor's knowledge of who uses
it changed. See [Fork Mergeability](./fork_mergeability.md) for why this
refactor mattered for rebase risk.

`ED_area_newspace()` in `editors/screen/area.cc` resets
`context_delegate_spacetype` to `SPACE_EMPTY` whenever an area's type
changes, for any area. Without this reset, switching an area away from
the Add-on Editor to, for example, a Node Editor left the stale delegate
type in place. Context lookups for the new Node Editor area then
silently redirected to an unrelated area of the old delegate type, a
real crash that was fixed generically, not with Add-on-Editor-specific
code.

## `context.space_data` always resolves through the delegate

`CTX_wm_space_data()` (`context.cc:990`), like all 18 typed space
accessors, routes through `ctx_wm_area_effective()` unconditionally, for
every caller, everywhere, not only inside the panel-draw window that the
layout function explicitly swaps. So `context.space_data` resolves to
the delegate's space whenever one is active, regardless of which region
is currently drawing: header, panel, or anything else.

**The rule**: any code that needs `SpaceAddon`'s own state (`addon_id`,
`preferred_delegate_spacetype`), whether the editor's own code or a
hosted add-on's, must read it via `context.area.spaces.active`, never
`context.space_data`, regardless of which region or callback it runs
in. `ADDON_HT_header.draw()` (`space_addon.py:199`) follows this rule.

## Context delegation narrowed to panel callbacks {#context-delegation-narrowed-to-panel-callbacks}

Until this fix, `addon_main_region_layout` swapped `CTX_wm_area` and
`CTX_wm_region` to the delegate around the entire
`ED_region_panels_layout_ex` call. This was too coarse, and it produced
two distinct bugs that both traced back to the same cause.

- **Empty-state crash**: the editor's own fallback panel,
  `ADDON_PT_empty_state`, read `context.area` and got a
  `SpaceNodeEditor` instead of the real `SpaceAddon`. A delegate can
  resolve to a real, open editor even when nothing in the panel list
  ends up needing it. An `only_fallback_panel` special case patched this
  first. The fix below later made the special case unnecessary.
- **Panels not reflowing on resize**: dragging a hosted panel's resize
  grip set `PANEL_ANIM_ALIGN`. `panels_need_realign()` then returns that
  panel, and `panels_end()` calls `panel_activate_state()`, which read
  the region from context, the borrowed one. The re-align animation and
  its redraw started on the delegate's region instead of this editor's
  own region.

Two more targeted fixes were tried and rejected:

1. Making `panel_activate_state()` take the region explicitly. This was
   insufficient on its own: `panel_handle_data_ensure()` registers the
   animation handler through `WM_event_add_ui_handler()`, which
   snapshots `CTX_wm_area`/`CTX_wm_region` into the handler
   (`wm_event_system.cc:5205-5208`). The handler stayed bound to the
   borrowed region regardless.
2. Wrapping the callbacks on the editor's own `PanelType` copies. This
   cannot reach sub-panels: the copies are shallow, so a copy's
   `children` list is the registered type's own children, and
   `ed_panel_draw()` recurses into those registered child types
   directly.

**What was built**: `ED_region_panels_layout_ex()` takes an optional
`PanelDrawContextOverride` (`ED_screen.hh`), an area and/or region
applied only around each panel's `poll`, `draw`, `draw_header`, and
`draw_header_preset`, via a scoped RAII applier that restores what it
saw (re-entrant, so a sub-panel's callback nests inside its parent's).
This override is threaded through `ed_panel_draw`'s child recursion and
through `panel_add_check`, so top-level polls get it too. The parameter
defaults to null, so the five other editors calling this function are
unaffected.

Everything the layout pass does besides running panel callbacks, that
is, panel alignment, region size updates, search-filter lookup, and
handler registration, now runs with the region it was actually called
for. The `only_fallback_panel` special case was removed as a result: the
crash it guarded against became structurally impossible.

## What this still does not fix: modal operators

Modal operators fail at button-invoke time, which the panel-callback
swap never covered: `WM_event_add_modal_handler` snapshots the
un-delegated area and region, so an invoked modal operator (for example,
Transform) finds no `RegionView3D` on the Add-on Editor's own region.

**Precise failure point**: `TRANSFORM_OT_translate`, `.rotate`, and
`.resize` call `convertViewVec()` (`transform.cc:185-231`), which for the
`SPACE_VIEW3D` branch calls
`ED_view3d_win_to_delta(t->region, xy_delta, t->zfac, r_vec)`. This call
needs `t->region`'s real `RegionView3D`, the live view and projection
matrices, to scale a 2D pixel delta into a 3D-space movement correctly
for the current zoom and camera distance. When `t->region`/`t->spacetype`
do not resolve to a genuine `SPACE_VIEW3D` region, execution falls to
the `else` branch and logs `"%s: called in an invalid context\n"`.

**Narrower than "all modal operators break."** The installed DreamUV
add-on's `view3d.dreamuv_uvscale` never reads `context.region`,
`context.space_data`, `context.region_data`, and never calls any
`region_2d_to_*`/`ED_view3d_win_to_*`-style conversion. It computes
everything from raw window-space mouse deltas plus direct bmesh edits,
so which region the click landed in is irrelevant to its correctness.
It works when hosted. A detection rule based on
`wmOperatorType::modal != nullptr` alone would flag DreamUV alongside
Transform, even though only Transform actually breaks. The real
distinguishing signal, whether the operator's `invoke()`/`modal()` body
reads region or view-space state, is not detectable statically.

**Deferred, not solved.** If revisited, the fix scopes the region swap
to operator invocation specifically: a post-layout walk of the region's
`uiBlock`/`uiBut` lists, rebinding qualifying buttons to a small
wrapper, rather than widening the existing accessor. This is the same
missing hook a modal-operator warning feature would need. A scoped swap
applied unconditionally to every button in a hosted panel would be
harmless when unneeded and correct when needed, and it sidesteps the
false-positive problem of any `ot->modal != nullptr` classifier.

## Single delegate per area, not per panel

`addon_delegate_spacetype_find()` resolves one `delegate_spacetype` per
area, from whichever panel's declared type matches an open editor first
(or from the user's explicit preference, see below). That one delegate
then applies to every panel's context resolution for that area.

**Consequence for mixed-editor add-ons.** ucupaint registers panels for
both `VIEW_3D` and `NODE_EDITOR`. A `NODE_EDITOR`-only panel polled
while the delegate happens to be `VIEW_3D` sees a `SpaceView3D` where it
expects a `SpaceNodeEditor`, and its own unguarded attribute access (for
example, `context.space_data.tree_type`) raises an error.

**Not fixed yet.** The recommended approach resolves the delegate per
panel, from that panel's own declared `bl_space_type`, rather than once
per area. This approach is deterministic, since every panel already
declares what it needs.

## User-chosen delegate preference

The automatic "first declared type with an editor open wins" scan gave
the user no way to see or change the outcome. `SpaceAddon` gained
`preferred_delegate_spacetype` (`SPACE_EMPTY` means no preference,
keeping the automatic scan as the fallback).
`BKE_paneltypes_addon_space_types_get()` in `blenkernel` answers "which
editor types does this add-on declare top-level panels for," shared by
the automatic path and the picker UI.

`addon_main_region_layout` resolves the delegate on every layout pass,
not only on a cache miss, and folds the resolved value into the
cache-invalidation check.

**Honoring an explicit choice strictly.** The first version fell back to
the automatic scan whenever the preferred type was not open, silently
substituting a different declared type's panels. This made "the editor
you picked is not open" practically unreachable, and let an explicit
choice be silently overridden. The fix now honors the preference
strictly: not open means the resolved delegate is `SPACE_EMPTY`, full
stop, with no substitution. `ADDON_PT_empty_state` names that one editor
specifically in this case, instead of the generic "one of the
following" list used for the no-preference (Auto) case. See
[UX: Editor-Type Picker](./../design/ux_addon_picker.md) for the picker
UI this preference uses.

## Should context routing be dynamic, resolved per operator call?

This question was raised as a design question, not built. The only
feasible mechanism for "does this operator need editor X's context"
without a manifest is reactive: try a candidate delegate, see if
`poll()` succeeds, otherwise try the next. This is a strict superset of
"resolve per panel from its own declared `bl_space_type`" (above), which
already gets correctness in the common case, deterministically.
Probing would only add value where a panel's `poll()` needs something
other than what its own `bl_space_type` declares, unusual enough to
treat as a fallback layer, not the primary mechanism.

**The safety property to hold**: delegation must only ever resolve to a
real, currently open, on-screen editor, via `BKE_screen_find_big_area`
(`space_addon.cc:242`). No mechanism in this design fabricates or
reconstructs context without a visible backing editor. If nothing
matches, delegation returns null and the operator polls false, the same
as today. Extending delegation to probe multiple real open editors does
not cross this line. Inventing a plausible-looking context without one
would cross it.

## Properties-editor delegates depend on the active tab

The Properties editor builds its context path from
`SpaceProperties::mainb` (the active tab) in `buttons_context_path()`
(`buttons_context.cc:839`). Which members exist (`object`, `mesh`,
`material`, and so on) depends on which tab is open. A panel re-hosted
here borrows the whole editor, including its tab state, so a panel
needing `mesh` draws nothing while the real Properties editor sits on a
different tab. This case is deliberately not worked around: forcing
`mainb` during layout would silently change what another visible editor
shows.

This limitation does not affect panel collection: panels are collected
regardless of tab, because the Add-on Editor calls
`ED_region_panels_layout_ex` with `contexts = nullptr`, so
`panel_add_check`'s context test is skipped. Only the data a panel reads
is tab-dependent, not whether the panel is listed.

## `temp_override` as the closest existing precedent

Vanilla add-on authors solve a related problem with
`bpy.context.temp_override(area=..., region=...)`, wrapped around one
operator call. Tracing the call chain confirms the scoping stays sound
even though a drag continues after the `with` block exits:
`WM_operator_call_py` calls `wm_operator_call_internal`
(`wm_event_system.cc:2002`), which invokes `ot->invoke()` and, for
`RUNNING_MODAL`, registers the modal handler and returns immediately.
`WM_event_add_modal_handler`/`WM_event_add_ui_handler` capture
`handler->context.area`/`region` from `CTX_wm_area(C)`/`CTX_wm_region(C)`
at registration time, synchronously, while the override is still
active. This is a one-time snapshot, not a live read.

**The primitive is not Python-exclusive.** `CTX_wm_window_set`,
`_screen_set`, `_area_set`, and `_region_set` are plain C++ functions.
`temp_override()` is a Python-facing wrapper around them.

**What is actually missing is the interception point, not the
primitive.** A Python author using `temp_override` writes the
`bpy.ops.xxx()` call and chooses where to wrap it. A button in a
re-hosted panel is invoked by Blender's own generic dispatch
(`interface_handlers.cc`'s `ui_apply_but_operator` calling
`wm_handler_operator_call`), which has no concept of "this button lives
in a re-hosted foreign panel" and no existing hook to insert anything
before invocation.

**Why this has to be solved on the editor's side.** A panel only ever
draws inside the editor type it declared in vanilla Blender, so
`context.region`/`context.space_data` are always correct by
construction, and there was never a mismatch to paper over. The Add-on
Editor breaks that guarantee systematically, for unmodified panels
whose authors never anticipated running anywhere but their declared
editor.

## Related

- [Panel Hosting](./panel_hosting.md)
- [Multi-Window Context Search](./multiwindow_context_search.md)
- [Fork Mergeability](./fork_mergeability.md)
- [UX: Empty State and Header](../design/ux_empty_state_and_header.md)
