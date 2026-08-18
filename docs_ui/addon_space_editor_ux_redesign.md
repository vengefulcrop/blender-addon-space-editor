# Add-on Space Editor — Sidebar / Bookmarks UX Redesign

**Status**: design discussion only, not scoped into the implementation plan, not started.
**Related**: [addon_space_editor_plan.md](addon_space_editor_plan.md),
[punch_list.md](punch_list.md)

---

## 1. The idea

A left-side sidebar, in the style of the File Browser's bookmarks column, with two
vertically stacked panels:

- **Bookmarks** — flat, searchable list of specific add-on panels the user has pinned.
- **Addons** — hierarchical listing of every add-on that registers any panels at all;
  each add-on entry expands to show the panel arrangements it offers per space type.

**Intended flow**: the Addons panel lists all valid add-ons. Expanding one shows its
panel sets, broken down by which editor type each set targets (the same
`bl_space_type` grouping already computed by `_addon_top_level_panel_space_types()` in
`space_addon.py`). Selecting a specific one hosts it in the main region, same as today's
editor-type dropdown, but discoverable as a browsable tree instead of a flat enum.
Bookmarks are scoped to a *specific panel set of a specific add-on*, not the whole
add-on - a user can bookmark just the one editor-type entry they use, not all of them.
Bookmarks persist.

---

## 2. What needs a C recompile, and what doesn't

Established by tracing the actual accessor/registration boundaries, not assumed from
general Blender-addon-dev experience:

### Needs C, one-time

- **The sidebar region itself.** A new, left-aligned `ARegionType` (`RGN_ALIGN_LEFT`),
  registered in `space_addon.cc` the same way the existing header/main regions are
  (`art = MEM_new_zeroed<ARegionType>(...)`). Regions are structural - there is no way
  to add a new one from Python. Same class of change as the header registration
  already in the file.
- **Persisted bookmarks.** File Browser bookmarks aren't DNA at all (a
  `bookmarks.txt` path list loaded via `fsmenu.cc`), but ours need to be scoped to
  `(addon_id, panel_set_space_type)` pairs, not paths - structured data. The closest
  precedent already in this codebase is `bAddonEditor` (the curated add-on list in
  `UserDef` backing the existing editor-type picker) - a sibling collection storing
  bookmark entries is the natural shape. New DNA struct, RNA registration, and a
  versioning bump (same pattern as the existing `bAddonEditor` subversion work).

### Pure Python once that scaffolding exists - no recompile per iteration

- The two panels themselves (`Panel` subclasses targeting the new region type, same
  shape as `ADDON_PT_empty_state` today).
- Search-as-you-type filtering (`UIList` `bl_filter_flag` / `filter_items()`).
- Bookmark toggle UI (star/pin icon per row) once the underlying storage property
  exists.
- Iteration on all of the above happens at the fast reload-and-see pace already
  established for this feature's Python side - edit `.py`, reload the script in an
  already-running Blender, no rebuild.

---

## 3. The hierarchy widget: `AbstractTreeView`, not a hand-rolled flat list

First instinct was to fake hierarchy in Python - flatten the addon/panel-set tree into
rows with a depth value, draw with indentation in a `UIList`, track expand/collapse as
a property, filter visible rows accordingly. This is a real, precedented pattern
(Grease Pencil's layers panel does exactly this), and would work.

**But it's not the better choice here, and shouldn't be the default.** Blender has a
mature, well-precedented C++ tree-view widget - `AbstractTreeView` /
`AbstractTreeViewItem` (`UI_tree_view.hh`) - with 15 real call sites already in this
tree (`asset_catalog_tree_view.cc`, Grease Pencil's own layer tree template, bone
collections, node-tree interface sockets, the asset shelf catalog selector, and more).
One of those, `editors/space_file/asset_catalog_tree_view.cc`, is close to a direct
analog: a hierarchical, expandable catalog tree in a browser sidebar. That's the file
to clone from, not the Outliner (far more generic/complex than this needs).

Given the sidebar region and bookmark DNA already require one C round, subclassing
`AbstractTreeView` for the Addons panel specifically costs little extra relative to
that round, and buys real native tree behavior for free: expand/collapse with
animation, correct indentation, keyboard navigation, drag support if ever wanted,
visual consistency with every other hierarchical browser in Blender - none of which
the hand-rolled flat-list approach gets without deliberately rebuilding it.

**The real tradeoff, stated plainly**: `AbstractTreeView` has no RNA/Python bridge.
All 15 existing usages are pure C++. Using it means the Addons tree's row content
(add-on entries, their per-space-type panel-set children) and interaction
(select-to-host, bookmark-toggle) are written as a C++ `AbstractTreeViewItem`
subclass, not a Python `Panel`. That specific panel loses the fast Python
edit-reload-see loop - every tweak to its rows or behavior costs a rebuild, same cost
class as the region/DNA scaffolding itself. The **Bookmarks** panel (flat, searchable,
no hierarchy) is unaffected and stays pure Python/`UIList` regardless - only the
**Addons** tree specifically moves into C++.

---

## 4. Recommended shape

Build together, as the one C round:

1. Sidebar `ARegionType` (left-aligned).
2. Bookmark DNA (new `UserDef`-sibling collection to `bAddonEditor`, storing
   `(addon_id, panel_set_space_type)` entries) + RNA + versioning bump.
3. Addons tree, cloned from `asset_catalog_tree_view.cc`'s structure, sourcing its
   rows from the same add-on/panel-set data `_addon_top_level_panel_space_types()`
   already computes in Python (would need a C-side equivalent, or exposing that
   computation to C++ - not yet decided which).

Then iterate in Python afterward on:

- The Bookmarks panel's layout, search, and toggle UX.
- Header/empty-state polish informed by whatever the sidebar surfaces.

Treat the Addons tree's row layout and interaction model as something to settle
*before* committing to the C round, since - unlike the rest of this feature - changes
to it cost a rebuild rather than a reload.

---

## 5. Open questions, not yet resolved

- Exact bookmark DNA shape: reuse/extend `bAddonEditor` itself, or a new sibling
  struct? `bAddonEditor` currently represents "a curated add-on the picker offers,"
  which is a different concept from "a user's pinned panel-set shortcut" - probably
  wants its own struct, but not decided.
- Where does the add-on/panel-set enumeration the tree needs get computed - C++ native
  (duplicating `_addon_top_level_panel_space_types()`'s logic, worsening the
  triplication already tracked as punch-list item 1), or exposed from Python to C++
  somehow (no existing precedent for that direction in this codebase)?
- Whether selecting a tree row should switch the *current* area's hosted add-on
  in-place (today's behavior) or support opening in a new area/split - not discussed
  yet.
- Interaction with the still-unbuilt per-panel delegate resolution (punch-list item 9)
  - the tree makes the mixed-editor case (an add-on with panel sets for two different
    space types) directly visible/selectable for the first time, which increases the
    pressure to solve item 9 rather than leave it deferred.
