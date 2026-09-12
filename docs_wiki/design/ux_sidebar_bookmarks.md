---
type: spec
title: "UX: Sidebar and Bookmarks"
description: "The Bookmarks/Addons sidebar design, styled after the File Browser's bookmarks column"
tags: [design, addon-editor, ux, sidebar, bookmarks]
last_updated: 2026-09-12
---

# UX: Sidebar and Bookmarks

**Status**: first pass implemented and functional (2026-08-18). Several
pieces from this document stay unbuilt. See
[Sidebar Tree View](../architecture/sidebar_tree_view.md#not-built-yet).

## The idea

A left-side sidebar, in the style of the File Browser's bookmarks column,
with two vertically stacked panels:

- **Bookmarks** — a flat, searchable list of specific add-on panels the
  user pinned.
- **Addons** — a hierarchical listing of every add-on that registers any
  panel at all. Each entry expands to show the panel arrangements it
  offers per space type.

**Intended flow**: the Addons panel lists all valid add-ons. Expanding one
shows its panel sets, broken down by which editor type each set targets.

Selecting a specific one hosts it in the main region, same as the
editor-type dropdown, but discoverable as a browsable tree instead of a
flat enum.

Bookmarks target a specific panel set of a specific
add-on, not the whole add-on, so a user can bookmark just the one
editor-type entry they use. Bookmarks persist.

## Bookmark behavior

Bookmarks should add regardless of whether the panel is actually
rendering at the moment. Being in the list is the only requirement.

## Recommended build shape

Build together, as one C++ round:

1. Sidebar `ARegionType` (left-aligned).
2. Bookmark DNA (a new `UserDef`-sibling collection to `bAddonEditor`,
   storing `(addon_id, panel_set_space_type)` entries), plus RNA and a
   versioning bump.
3. The Addons tree, cloned from `asset_catalog_tree_view.cc`'s structure,
   sourcing its rows from the same add-on/panel-set data
   `_addon_top_level_panel_space_types()` already computes in Python.

Then iterate in Python on:

- The Bookmarks panel's layout, search, and toggle UX.
- Header/empty-state polish informed by whatever the sidebar surfaces.

Treat the Addons tree's row layout and interaction model as something to
settle before committing to the C++ round, since changes to it cost a
rebuild rather than a script reload. See
[Sidebar Tree View](../architecture/sidebar_tree_view.md) for the
implementation this design produced.

## What the first pass actually built

Functional, verified by launching a real build: the sidebar renders, the
tree lists add-ons with their real display names, expands to per-editor-
type rows, and selecting a row hosts that add-on/space-type pair.

Bookmarks pin, unpin, list, and reopen. See
[Sidebar Tree View](../architecture/sidebar_tree_view.md#what-was-built-first-pass)
for the full built/not-built breakdown.

## Related UX polish from the redesign

- Real add-on names and the bundled-add-on filter, both applied to the
  tree as well as the picker.
- A search field, scroll bar, and alphabetical sort on the Addons tree,
  once `set_default_rows()` gave the tree view a custom, bounded height.

## Related

- [Sidebar Tree View](../architecture/sidebar_tree_view.md)
- [Native Tree View vs Hand-Rolled Flat List](../decisions/adr_007_native_tree_view_vs_flat_list.md)
- [UX: Editor-Type Picker](./ux_addon_picker.md)

