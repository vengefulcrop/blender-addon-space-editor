---
type: spec
title: "UX: Empty State and Header"
description: "How the editor explains why a panel is not drawing, and what the header shows about the current delegate"
tags: [design, addon-editor, ux, empty-state, header]
last_updated: 2026-09-12
---

# UX: Empty State and Header

## Supported-editor info: header button when drawn, in-region block when empty

A simple version of "open the editor for me," without a
split-and-collapse mechanic (see
[Horizontal Panel Layout](#horizontal-panel-layout-scoped-out) below):
show which editor types a hosted add-on's panels need, as a header info
button when at least one panel draws, or as an in-region information
block when none are. The sidebar tree later took over the header button's
job. See the note below.

**Single source of truth, in Python, used by the in-region block below and,
before removal, by the header button.**

`_addon_supported_spaces(addon_id)` in `space_addon.py` walks
`bpy.types.Panel.__subclasses__()` filtered to the add-on's module
(matched by prefix, mirroring the C-side attribution rule).

It collects the
distinct `bl_space_type` values of its top-level `UI`/`WINDOW` panels, and
resolves each to Blender's own display name via
`bpy.types.Area.bl_rna.properties["type"].enum_items` — reusing Blender's
existing curated names.

**Header button, now removed.** `ADDON_OT_supported_editors_info`, an
inert `INTERNAL` operator, does not exist any more. Its `description()`
classmethod had returned the joined list as its tooltip — the standard
Blender idiom for a hover-only info affordance. The sidebar tree replaced
it: `ADDON_HT_header.draw()` (`space_addon.py:108`) now only shows the
hosted add-on's display name, since the tree already lists every add-on
with its editor types as expandable rows. See
[Sidebar Tree View](../architecture/sidebar_tree_view.md).

**Panel collection check**: `bpy.types.Region.panels` does not exist. The
first version of the button-vs-block condition checked
`len(region.panels) > 0`, which raises `AttributeError`.

The implementation replaces this with
`_addon_has_open_delegate()`. This helper checks whether any editor type the add-on's
panels need is currently open in the screen — not a guarantee any specific
panel will draw, but the best signal available from Python.

It shares its panel/space-type filtering with `_addon_supported_spaces()` via one
extracted helper, `_addon_top_level_panel_space_types()`.

**In-region block.** The implementation removed the previous single-value mechanism
(`SpaceAddon_Runtime::missing_spacetype`, drawn via raw `BLF` calls), and did not extend it. A single value could not express "needs one of
several editor types."

The implementation replaces it with `ADDON_PT_empty_state`, an ordinary
Python `Panel` (`bl_space_type = 'ADDON'`, `HIDE_HEADER`). C++ injects this panel
into the collected panel list precisely when that list would otherwise be
empty, via `addon_empty_state_paneltype_find()`. Blender draws it through the
ordinary `ED_region_panels_draw` path like any other panel.

## Wording

The block's message contains an explicit two-line lead-in ("This add-on's panels
require one of the following / editor types to be present in the
workspace:") followed by a blank separator and one bulleted line per
editor name, using `•` rather than a literal `-`.

Developers considered and withdrew a proposal to split `IMAGE_EDITOR` into two display entries, "UV Editor"
and "Image Editor." Blender has only the one
`Area.type` identifier, displayed as "UV/Image Editor," and showing
Blender's own combined name is correct, since UV editing is a mode of the
Image Editor, not a distinct area type.

The override point (in
`_addon_supported_spaces()`, next to the `type_enum.get(space_type)`
lookup) remains available if developers ever request a similar split for a
specific, deliberately chosen editor.

The two-line lead-in is a deliberate hard split — two separate
`col.label()` calls, not one label wrapped by a narrow region.
`UILayout.label()` never auto-wraps. It clips with an ellipsis on a narrow
region instead.

## When a set preference is not open

`ADDON_PT_empty_state` checks specifically for the case where a set
`preferred_delegate_spacetype` matches no open area, and names that one
editor specifically ("This panel requires the following to be open in the
workspace:"), rather than the generic "one of the following" list kept for
the no-preference (Auto) case. See
[Context Delegation](../architecture/context_delegation.md#user-chosen-delegate-preference).

## Icons on the supported-editors list

`_addon_supported_spaces()` returns `(space_type, name, icon)` triples via
a shared `_space_type_icon_name()` helper.

This function is the one place both the header
and the empty-state block resolve an editor type to what the user sees, so
they cannot disagree on icon or name. The empty-state's bulleted list
became icon-labeled entries instead of a bare bullet.

## Header showed the raw add-on id, not its display name

`ADDON_HT_header.draw()` fell back to `layout.label(text=space.addon_id)`
when nothing else was drawn — written before `bAddonEditor.name` existed.

This was harmless for older add-ons, whose module id already reads as a name. For
extensions, `addon_id` is the full `bl_ext.<repository>.<addon>` import
path.

Fixed by looking up the curated `bAddonEditor` entry matching
`addon_id` and showing its `name`, falling back to `addon_id` only if no
curated entry exists.

## Horizontal panel layout: scoped out {#horizontal-panel-layout-scoped-out}

Requested: let a hosted add-on's collapsible sections arrange
left-to-right instead of top-to-bottom, with an aspect-ratio-based default
and a manual override.

Assessed and deliberately not started. `ED_region_panels_layout_ex` has no
horizontal concept anywhere in it: panels accumulate strictly by Y-offset
at a fixed width, and that assumption is load-bearing throughout
(collapse/expand height bookkeeping, drag-reorder, and the View2D scroll
lock).

True horizontal columns requires forking that layout function's
internals or reimplementing panel headers, collapse state, and drag
interaction independently. This work represents a larger task than everything else in
this plan combined, and its maintenance cost conflicts with staying
a rebasable patch series.

**Cheaper adjacent option**: the category-tab system already active in
this editor (inherited from `ED_region_panels_layout_ex`, the
same one N-panels use for `bl_category`) gives one-section-at-a-time
navigation via edge tabs — not simultaneous side-by-side columns, but a
practical answer to "many collapsible sections are unwieldy as one long
scroll," buildable without touching panel layout internals at all.

Recorded as a candidate future item, intentionally out of scope.

## Related

- [Panel Hosting](../architecture/panel_hosting.md)
- [Context Delegation](../architecture/context_delegation.md)
- [Open UX Requests and Known Issues](./open_ux_requests.md)

