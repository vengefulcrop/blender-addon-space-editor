---
type: spec
title: "Open UX Requests and Known Issues"
description: "Outstanding UX backlog for the Add-on Editor, captured directly from user notes"
tags: [design, addon-editor, ux, backlog]
last_updated: 2026-09-13
---

# Open UX Requests and Known Issues

**Status:** superseded in part by Add-on Editor: remove the curated-list
picker, tree replaces it (`5add1f67281`, 2026-08-19), and by Add-on Editor:
tree row activation, bundled add-ons, and a drawing-nothing notice
(`d66db359820`, 2026-08-19). Two lines under Done no longer match the code:
the picker the "Add an Add-on" entry folded into is gone, and the tree has
no active-row highlight.

Captured directly from working notes on the Add-on Editor UI and UX. An
item marked done is resolved. The rest are open.

## Done

- Removed the addon column. The Add-on Editor now marks itself as "Add-on"
  type in the data-category column.
- Removed the "Add an Add-on" entry as a standalone dropdown item It folds
  into the picker flow instead. See
  [UX: Editor-Type Picker](./ux_addon_picker.md).
- Removed the header enum for panel-context picking.
- Removed the information icon from the header.
- Bundled Blender add-ons are marked with an extra Blender icon (or the
  plugin icon is replaced with the Blender icon) when enabled, and listed
  at the very top.
- Clicking an add-on entry shows the first entry in its list. Before this,
  nothing showed until the user clicked a specific sub-entry.
- Highlighting the currently active panel and its owner in the list is
  already in place.

## Open

- Tooltip on hover showing the full add-on name and its module path.
- Remove the curated list from Preferences, remove "max addons shown" from
  it, and move "show bundled addons" to a more appropriate preferences
  category. See
  [Capping the Editor-Type Menu](../decisions/adr_008_capping_editor_type_menu.md)
  and
  [Bundled Add-ons as Opt-In Preference](../decisions/adr_009_bundled_addons_opt_in.md)
  for the current shape of both preferences.
- Make sure alphabetical re-ordering still respects "Blender bundled
  add-ons at the very top."

## Requested features, not yet built

- A gear icon that opens Add-ons/Extensions preferences quickly.
- UX for opening the Add-on Editor in an area that is too narrow: consider
  opening with the sidebar collapsed, a visible collapse-arrow widget when
  the sidebar is open, or double-clicking an add-on panel to focus it and
  collapse the sidebar.

## Fixed bugs, recorded for context

- Search left all tree entries fully uncollapsed and never restored their
  pre-search state. A follow-up regression showed searched entries by
  add-on name but refused to show sub-entries. Searching by editor name
  worked. An uncollapsed entry found via search also stayed uncollapsed
  once the search cleared. All three are fixed. See
  [Sidebar Tree View](../architecture/sidebar_tree_view.md#follow-up-fixes).
- Resizing a hosted panel (for example, a material panel) vertically via
  its internal gizmo did not make other panels move until the resized
  panel, or another one, was collapsed and reopened. This did not happen
  in the panel's original, native editor. Narrowing context delegation to panel
  callbacks fixed it. See
  [Context Delegation](../architecture/context_delegation.md#context-delegation-narrowed-to-panel-callbacks).
- The extension name on the right side of the header regressed to showing
  its full module path instead of its display name, a regression of an
  earlier fix. See
  [UX: Empty State and Header](./ux_empty_state_and_header.md#header-showed-the-raw-add-on-id-not-its-display-name).

## Open visual note

The top edge of the inner panel sits nearly flush with the header, which
looks slightly off. Not yet clear how much control the editor has over
this spacing.

## Related

- [UX: Editor-Type Picker](./ux_addon_picker.md)
- [UX: Sidebar and Bookmarks](./ux_sidebar_bookmarks.md)
- [UX: Empty State and Header](./ux_empty_state_and_header.md)
