---
type: decision
title: "ADR 007: Native Tree View vs Hand-Rolled Flat List for the Addons Sidebar"
description: "Use C++ AbstractTreeView for the Addons sidebar tree, trading away the Python edit-reload-see loop for that one panel"
tags: [decision, addon-editor, sidebar, tree-view]
last_updated: 2026-09-12
---

# ADR 007: Native Tree View vs Hand-Rolled Flat List

## Context

The Addons sidebar needs a hierarchical list: add-ons at the top level,
expanding to the per-editor-type panel sets each one offers. A real,
precedented pattern exists for faking this in Python. It flattens the tree
into rows with a depth value, draws with indentation in a `UIList`, tracks
expand and collapse state as a property, and filters visible rows
accordingly. Grease Pencil's layers panel does exactly this.

## Decision

Use Blender's C++ `AbstractTreeView` / `AbstractTreeViewItem`
(`UI_tree_view.hh`) instead, cloned from
`editors/space_file/asset_catalog_tree_view.cc`. This is a hierarchical,
expandable catalog tree in a browser sidebar, the closest direct analog
among its 15 existing call sites. Those sites also include Grease Pencil's
own layer tree template, bone collections, and node-tree interface
sockets.

## Alternatives considered

- **Hand-rolled flat list in Python**: rejected as the default, though
  workable. The sidebar region and bookmark DNA already require one C++
  round for other reasons, so subclassing `AbstractTreeView` costs little
  extra relative to that round.

## Consequences

- This gained, for free: expand and collapse with animation, correct
  indentation, keyboard navigation, drag support if ever wanted, and visual
  consistency with every other hierarchical browser in Blender.
- **The real tradeoff**: `AbstractTreeView` has no RNA/Python bridge. All
  15 existing usages are pure C++. The Addons tree's row content and
  interaction (select-to-host, bookmark-toggle) are written as a C++
  `AbstractTreeViewItem` subclass, not a Python `Panel`. That specific
  panel loses the fast Python edit-reload-see loop. Every tweak to its
  rows or behavior now costs a rebuild, the same cost class as the
  region/DNA scaffolding itself.
- The **Bookmarks** panel (flat, searchable, no hierarchy) is unaffected
  and stays pure Python/`UIList`. Only the Addons tree specifically moved
  into C++.
- `set_default_rows(8)` turned out load-bearing, not cosmetic: it is the
  only public way to give the view a custom height. The scroll bar,
  drag-to-resize grip, search field, and alphabetical sort toggle are all
  gated on `tree_view.custom_height_` being set.

## Related

- [Sidebar Tree View](../architecture/sidebar_tree_view.md)
- [UX: Sidebar and Bookmarks](../design/ux_sidebar_bookmarks.md)
