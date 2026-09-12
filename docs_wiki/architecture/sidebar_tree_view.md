---
type: architecture
title: "Sidebar Tree View"
description: "How the Addons/Bookmarks sidebar is built, and why the Addons tree is a native C++ tree view"
tags: [architecture, addon-editor, sidebar, tree-view, bookmarks]
last_updated: 2026-09-12
---

# Sidebar Tree View

## Shape

A left-side sidebar, styled like the File Browser's bookmarks column,
holds two vertically stacked panels:

- **Bookmarks** — a flat, searchable list of specific add-on panel sets
  the user pinned.
- **Addons** — a hierarchical listing of every add-on that registers any
  panels. Each entry expands to show the panel sets it offers per space
  type (the same `bl_space_type` grouping
  `_addon_top_level_panel_space_types()` computes in `space_addon.py`).

Selecting a specific panel set hosts it in the main region, the same as
the editor-type dropdown does, but the tree makes it browsable. Bookmarks
are scoped to a specific panel set of a specific add-on, not to the whole
add-on, and they persist across sessions.

## What needed a C++ recompile, and what did not

### Needed C++, one time

- **The sidebar region**: a new, left-aligned `ARegionType`
  (`RGN_ALIGN_LEFT`), registered in `space_addon.cc` the same way as the
  header and main regions. Regions are structural. Python cannot add one.
- **Persisted bookmarks**: `bAddonBookmark` DNA and
  `UserDef::addon_bookmarks` store `(addon_id, panel_set_space_type)`
  pairs. The closest precedent is `bAddonEditor` (see
  [Add-on Space Type](./addon_space_type.md)). A sibling collection was
  the natural shape. This needed a new DNA struct, RNA registration, and
  a versioning bump.

### Pure Python once that scaffolding exists

- The two panels themselves (`Panel` subclasses on the new region type,
  the same shape as `ADDON_PT_empty_state`).
- Search-as-you-type filtering (`UIList` `bl_filter_flag` /
  `filter_items()`).
- The bookmark toggle UI (a star/pin icon per row), once the underlying
  storage property exists.

## The Addons tree: `AbstractTreeView`, not a hand-rolled flat list

Blender has a mature, precedented C++ tree-view widget,
`AbstractTreeView` / `AbstractTreeViewItem` (`UI_tree_view.hh`), with 15
real call sites in the tree. These include `asset_catalog_tree_view.cc`,
Grease Pencil's layer tree template, bone collections, and node-tree
interface sockets. `asset_catalog_tree_view.cc` is close to a direct
analog: a hierarchical, expandable catalog tree in a browser sidebar. It
is the file the Addons tree clones from. See
[Native Tree View vs Hand-Rolled Flat List](../decisions/adr_007_native_tree_view_vs_flat_list.md)
for the full tradeoff.

## What was built (first pass)

- `bAddonBookmark` DNA plus `UserDef::addon_bookmarks`, RNA
  (`AddonBookmark`, an `addon_bookmarks` collection with `new`/`remove`),
  and blend-file list I/O.
- A left `RGN_TYPE_TOOLS` region on `SPACE_ADDON` (`prefsizex` 240,
  `RGN_ALIGN_LEFT`), using the stock `ED_region_panels_init/layout/draw`
  triple, so C-native and Python panels coexist in it.
- `addon_tree_view.cc` — `AddonTreeView : ui::AbstractTreeView`, cloned
  from `asset_catalog_tree_view.cc`, trimmed to `build_tree()` plus
  `BasicTreeViewItem` rows plus `set_on_activate_fn`. It has no
  drag/drop, rename, or context menu.
- `BPY_addon_module_info_get()` (`bpy_rna.cc` / `BPY_extern.hh`) resolves
  a module's `bl_info["name"]` and whether it is bundled. A C-drawn list
  cannot show extension module ids (`bl_ext.<repo>.<addon>` is an import
  path, not a name), and only Python can resolve either fact. This
  mirrors `_addon_label()`/`_addon_is_bundled()`, so the tree and the
  Python-drawn picker agree.
- Versioning (`versioning_530.cc`, subversion 11 to 12) back-fills the
  sidebar region into `SPACE_ADDON` areas saved before it existed.
- `ADDON_PT_bookmarks`, `ADDON_OT_bookmark_toggle`,
  `ADDON_OT_bookmark_activate` (Python).

## Three build-order bugs, all presenting as "the sidebar is not there"

1. `/t:blender` builds the executable but does not copy `scripts/` into
   the runtime tree. Blender ran a 13-day-old `space_addon.py`, so the
   Python panel never registered. The `INSTALL` project
   (`INSTALL.vcxproj`, not `/t:INSTALL` on the solution) syncs scripts.
2. Region order matters. `region_rect_recursive` carves the area up in
   region-list order, and the `RGN_ALIGN_NONE` main region claims the
   remainder. Appending the sidebar after the main region gave it 1
   pixel. Every space type adds its main region last, and
   `addon_create()` now does too.
3. `ED_area_newspace` reuses a cached, non-empty region list rather than
   calling `create()` again, so existing areas never gain a newly added
   region. This is why the versioning bump exists.

## Follow-up fixes

- `set_default_rows(8)` on the tree. This is not cosmetic: it is the
  only public way to give the view a custom height, and it gates the
  entire scrollable-list treatment (`tree_view.cc`,
  `if (tree_view.custom_height_)`). This treatment covers the scroll
  bar, the drag-to-resize grip, the search field, and the alphabetical
  sort toggle. Without it the tree drew every row of every installed
  add-on at full length with no way to filter.
- Real add-on names in the tree via `BPY_addon_module_info_get()`, plus
  the bundled-add-on filter the picker already applies.
- Two upstream search-behavior bugs fixed in shared tree-view code:
  clearing a search now restores the collapse state it found, and a
  match on a parent row now reveals that row's children instead of
  appearing to empty it.
- Context delegation narrowed to the panel callbacks themselves (see
  [Context Delegation](./context_delegation.md#context-delegation-narrowed-to-panel-callbacks)).
  This fixed hosted panels not reflowing when one is resized.

## Not built yet

- Search/filter in the Bookmarks panel (planned `UIList`
  `filter_items()`). The Addons tree already has search via
  `set_default_rows()`.
- Bookmarks draws as a plain operator list, not a searchable `UIList`.
- Tree rows have no pin/bookmark affordance. Bookmarking is only
  possible from the Bookmarks panel header, for whatever the area
  currently hosts.
- No active-row highlight exists in the tree (`set_is_active_fn` is
  unused).
- The versioning back-fill is untested against a real pre-sidebar
  `.blend`.
- Tree row identity relies on default label-based `matches_single()`.
  Two add-ons sharing a display name would alias their expand state.
- `BPY_addon_module_info_get()` runs per add-on on every `build_tree()`,
  that is, on every sidebar redraw, and each call takes the GIL.
  `addon_utils.module_bl_info()` early-returns once an add-on is warmed
  up but still rebuilds its `_bl_info_basis()` dict every time, so the
  cost stays small but real and scales with the number of enabled
  add-ons. This pass deliberately skips a static label cache, to avoid a
  second process-wide mutable cache. If this needs a fix, the consistent
  invalidation signal is `BKE_paneltypes_state_get()`.

## Open questions

- Exact bookmark DNA shape: reuse or extend `bAddonEditor` itself, or add
  a new sibling struct. `bAddonEditor` represents "a curated add-on the
  picker offers," a different concept from "a user's pinned panel-set
  shortcut."
- Where the add-on/panel-set enumeration the tree needs gets computed: as
  C++ native code (duplicating `_addon_top_level_panel_space_types()`'s
  logic), or exposed from Python to C++ (no existing precedent for that
  direction in this codebase).
- Whether selecting a tree row should switch the current area's hosted
  add-on in place (today's behavior) or support opening in a new area or
  split.
- Interaction with the still-unbuilt per-panel delegate resolution (see
  [Context Delegation](./context_delegation.md#single-delegate-per-area-not-per-panel)):
  the tree makes the mixed-editor case directly visible and selectable
  for the first time, which increases the pressure to solve it.

## Related

- [Add-on Space Type](./addon_space_type.md)
- [Context Delegation](./context_delegation.md)
- [UX: Sidebar and Bookmarks](../design/ux_sidebar_bookmarks.md)
