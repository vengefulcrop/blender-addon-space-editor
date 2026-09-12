---
type: architecture
title: "Panel Hosting"
description: "How the Add-on Editor re-hosts an add-on's unmodified panels through ED_region_panels_layout_ex"
tags: [architecture, addon-editor, panels, region-layout]
last_updated: 2026-09-12
---

# Panel Hosting

## The hook, not a new drawing stack

`ED_region_panels_layout_ex` accepts an arbitrary panel-type list. It does
not hard-wire to the region's own list, at `area.cc:3439-3367`:

```cpp
void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBaseT<PanelType> *paneltypes,
                                wm::OpCallContext op_context,
                                const char *contexts[],
                                const char *category_override);
```

The Properties editor already uses this function to swap panel sets per
tab (`space_buttons.cc:315`). The Add-on Editor's window region builds a
filtered `ListBaseT<PanelType>` of the chosen add-on's panels and passes
it in. This gives, at no extra cost: panel headers, open/closed state,
drag-to-reorder, category tabs, panel search, and background drawing.
There is no manual UI block management and no direct Python `draw()`
call. See
[Panel Re-Hosting via Region Layout Hook](../decisions/adr_002_panel_rehosting_via_region_layout_hook.md)
for the rejected alternative: a new drawing stack.

## Panel types must be copied, not re-linked

`ED_region_panels_layout_ex` walks its list through `PanelType`'s own
`next`/`prev` fields. Every registered type already belongs to its home
region type's list. Putting one into a second list corrupts the first
list. The editor therefore keeps a private list of shallow copies in
`SpaceAddon_Runtime`. This is safe because `panel_find_by_type` matches
panels to types by ID name, not by pointer. Sub-panels are reached
through `children`, which still points at the registered types.

Because `Panel::type` points at those copies between frames, the copies
must stay stable. `BKE_paneltypes_tag_changed()` and
`BKE_paneltypes_state_get()` provide a revision counter. The counter
increments on panel registration and removal, so the cache rebuilds only
when the add-on changes or panel types actually change.

## Layout and draw must be separate callbacks

**Symptom**: all widgets in the editor rendered at roughly 0.75 of their
normal size.

**Cause**: `art->draw` performed both layout and drawing. Blender runs
the layout pass separately and earlier. The layout pass computes the
region size and the View2D `tot`/`cur` rectangles from the laid-out
content. Drawing then used bounds from the previous frame, so panel
widths followed stale bounds and everything laid out small.

**Fix**, matching the Properties editor:

```cpp
art->layout = addon_main_region_layout;  /* ED_region_panels_layout_ex */
art->draw = ED_region_panels_draw;       /* drawing only */
```

Widget metrics were measured in pixels after the fix and match the rest
of the UI. Two other changes landed in the same build: it set
`RGN_FLAG_INDICATE_OVERFLOW`, and `keymapflag` changed from
`ED_KEYMAP_UI | ED_KEYMAP_VIEW2D` to `ED_KEYMAP_UI | ED_KEYMAP_FRAMES`.
Both changes stand on their own: the Properties editor sets the same
flags, and `ED_KEYMAP_VIEW2D` would install view-zoom bindings in a
region whose zoom stays locked at 1.0.

`art->prefsizex` stays at 0. This makes `ED_region_panels_layout_ex`
choose `em = 20` rather than the `em = 10` it uses when the field is set.
This is the intended configuration for a full-area panel list rather
than a narrow sidebar.

## Collecting panels

`addon_panel_types_collect` scans the add-on's registered panel types
and builds the filtered list. Two extensions apply to the initial scan:

- **Region type**: the initial scan only covered `RGN_TYPE_UI` regions
  (N-panels). Properties-tab-style add-ons (for example, one panel with
  `bl_space_type = "PROPERTIES"`, `bl_region_type = "WINDOW"`,
  `bl_context = "scene"`) stayed invisible. The scan now also covers
  `RGN_TYPE_WINDOW`. `ED_region_panels_layout_ex` does not check what
  region type registered a given `PanelType`.
- **Accepted limitation**: `bl_context` normally switches
  Properties-style panels by tab, through the `contexts` parameter to
  `panel_add_check`. The Add-on Editor passes `contexts = nullptr` (no
  filtering), so a `bl_context`-heavy add-on shows every tab's panels
  flattened into one stack rather than the native tabbed view. The
  existing `bl_category`-based tab mechanism, already active for
  N-panels, is the natural place to bridge this gap later, by
  synthesizing category grouping from `bl_context` when a panel has no
  `bl_category` of its own.

## Delegate-availability filtering

`addon_panel_types_collect` skips a panel at collection time if no editor
of its declared space type is open anywhere. This matters because some
add-ons assume they only ever run inside their declared editor and read
context members with no defensive `getattr`. For example, the bundled
Cycles add-on's Node-Editor-cloned panels read `context.material`,
`context.light`, and `context.world` directly. With no Node Editor open
anywhere, `poll()` on such a panel would raise an error rather than
return `False`.

This filter depends on which editors are open elsewhere in the screen,
so the panel-type cache key folds in a screen-layout signature: a
bitmask of open space types. Without this signature, opening or closing
an editor elsewhere would not refresh the hosted panel list.

A panel can be excluded for two independent reasons: no add-on is
chosen, or the add-on's panels need an editor that is not open. Because
of this, the main region states which reason applies, instead of
rendering blank. See
[UX: Empty State and Header](../design/ux_empty_state_and_header.md).

## Panel `poll()` is the real compatibility limit

Confirmed against the bundled Node Wrangler add-on, whose panel
inherits:

```python
return (space.type == 'NODE_EDITOR' and space.node_tree is not None ...)
```

The panel is collected correctly but polls `False`, so the editor draws
empty. Panels with no space-specific `poll()` draw correctly, including
sub-panels and screen-level context such as `context.object`. This is
why [Context Delegation](./context_delegation.md) exists. It also had to
be built before the picker shipped: shipping the picker first would let
users select add-ons that then render nothing.

## Mixed-editor add-ons and per-panel context

An add-on can register panels for more than one space type. For example,
ucupaint registers panels for both `VIEW_3D` and `NODE_EDITOR`. Context
delegation resolves one delegate per area, so this case interacts
closely with [Context Delegation](./context_delegation.md). See that
document's "single delegate per area" limitation.

## Reflow after resize

Dragging a hosted panel's internal resize grip changed its height, but
panels below did not move until an unrelated redraw forced a rebuild.
Collapse and re-open worked, because that action runs outside the layout
pass with the real region already in context. The root cause and fix are
context plumbing, not panel-hosting logic. See
[Context Delegation](./context_delegation.md#context-delegation-narrowed-to-panel-callbacks).

## Related

- [Add-on Space Type](./addon_space_type.md)
- [Context Delegation](./context_delegation.md)
- [Add-on Panel Attribution](./addon_panel_attribution.md)
