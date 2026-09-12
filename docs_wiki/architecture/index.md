---
type: architecture
title: "Architecture Index"
description: "Section index for the Add-on Editor architecture concepts"
tags: [architecture, addon-editor, index]
last_updated: 2026-09-13
---

# Architecture

The Add-on Editor lets a user host an add-on's panels as a full editor area,
selectable from the standard editor-type dropdown. This section documents
the C++ and Python architecture that makes this work.

- [Add-on Space Type](./addon_space_type.md) — How
  `SPACE_ADDON` registers as a subtype-based editor and how it stores which
  add-on an area hosts.

- [Panel Hosting](./panel_hosting.md) — How the editor
  re-hosts an add-on's unmodified `Panel` classes through
  `ED_region_panels_layout_ex`, and the layout/draw split that panel sizing
  depends on.

- [Context Delegation](./context_delegation.md) — How a
  re-hosted panel resolves `context.space_data` and related members by
  borrowing a real editor elsewhere in the screen, and where that borrowing
  still fails.

- [Add-on Panel Attribution](./addon_panel_attribution.md) — How the editor decides which add-on owns a given `PanelType`.

- [Persistence and Compatibility](./persistence_and_compatibility.md) —
  How the hosted-area state survives `.blend` save/load, and what happens
  when an unmodified Blender build opens or resaves a fork-saved file.

- [Sidebar Tree View](./sidebar_tree_view.md) — How developers
  construct the Addons/Bookmarks sidebar, and why the Addons tree uses a C++
  `AbstractTreeView` rather than a Python widget.

- [Multi-Window Context Search](./multiwindow_context_search.md) — How Blender's own engine searches for an area across
  windows, and the gap this leaves for the Add-on Editor today.

- [Upstream Fragility](./upstream_fragility.md) — The two changes that break
  silently when upstream moves, and what to check on every rebase.

- [Fork Mergeability](./fork_mergeability.md) — Which
  upstream files this fork touches, how risky each touch is on rebase, and
  why the design stays close to upstream mechanisms.
