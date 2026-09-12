---
type: architecture
title: "Architecture Index"
description: "Section index for the Add-on Editor architecture concepts"
tags: [architecture, addon-editor, index]
last_updated: 2026-09-12
---

# Architecture

The Add-on Editor lets a user host an add-on's panels as a full editor area,
selectable from the standard editor-type dropdown. This section documents
the C++ and Python architecture that makes this work.

- [Add-on Space Type](./addon_space_type.md) — read this to learn how
  `SPACE_ADDON` registers as a subtype-based editor and how it stores which
  add-on an area hosts.
- [Panel Hosting](./panel_hosting.md) — read this to learn how the editor
  re-hosts an add-on's unmodified `Panel` classes through
  `ED_region_panels_layout_ex`, and the layout/draw split that panel sizing
  depends on.
- [Context Delegation](./context_delegation.md) — read this to learn how a
  re-hosted panel resolves `context.space_data` and related members by
  borrowing a real editor elsewhere in the screen, and where that borrowing
  still fails.
- [Add-on Panel Attribution](./addon_panel_attribution.md) — read this to
  learn how the editor decides which add-on owns a given `PanelType`.
- [Persistence and Compatibility](./persistence_and_compatibility.md) —
  read this to learn how the curated add-on list and hosted-area state
  survive `.blend` save/load, and what happens when an unmodified Blender
  build opens or resaves a fork-saved file.
- [Sidebar Tree View](./sidebar_tree_view.md) — read this to learn how the
  Addons/Bookmarks sidebar is built, and why the Addons tree is a C++
  `AbstractTreeView` rather than a Python widget.
- [Multi-Window Context Search](./multiwindow_context_search.md) — read
  this to learn how Blender's own engine searches for an area across
  windows, and the gap this leaves for the Add-on Editor today.
- [Upstream Fragility](./upstream_fragility.md) — The two changes that break
  silently when upstream moves, and what to check on every rebase.
- [Fork Mergeability](./fork_mergeability.md) — read this to learn which
  upstream files this fork touches, how risky each touch is on rebase, and
  why the design stays close to upstream mechanisms.
