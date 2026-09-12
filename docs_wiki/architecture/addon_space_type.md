---
type: architecture
title: "Add-on Space Type"
description: "How SPACE_ADDON registers as a subtype-based editor and stores which add-on an area hosts"
tags: [architecture, addon-editor, space-type, dna]
last_updated: 2026-09-12
---

# Add-on Space Type

## Goal

The Add-on Editor lets a user turn any enabled add-on or extension into an
editor. The intended user flow is:

1. The user clicks the editor-type button in an area header.
2. The editor-type list has an "Add-ons" heading and curated entries.
3. The user picks an add-on. The area switches to it and shows its panels.
4. The area draws that add-on's panels as a full editor.

The fork publishes its diff so a user can compile a personal build. This
constrains the design: it must stay a small, reviewable, rebasable patch
series against upstream `main`, not a large refactor.

## One space type, one menu entry

`SPACE_ADDON` is one registered space type. It appears in the editor type
menu as a single "Add-on" entry, like every other editor. The sidebar
Add-ons tree chooses which add-on an area hosts.

`space_addon.cc:719-725` sets no `SpaceType::space_subtype_get`,
`space_subtype_set`, or `space_subtype_item_extend`. Leaving
`space_subtype_item_extend` unset is what puts the plain "Add-on" entry in
the menu, because defining that callback suppresses a space type's own
entry in `rna_Area_ui_type_itemf`.

`AddonTreeView::build_tree` (`addon_tree_view.cc:139`) lists every enabled
add-on that registers panels. Activating a row writes the module name into
`SpaceAddon::addon_id` (`addon_tree_view.cc:118`).

**Superseded design.** An earlier version used the `SpaceType` subtype
mechanism at `BKE_screen.hh:156-158`, and added one menu entry per curated
add-on plus an "Add an Add-on..." picker. The enum packed as
`space_type << 16 | subtype`, the way the Node Editor presents Shader,
Compositor, and Geometry Nodes. The tree view replaced all of it. The
symbols `ADDON_SUBTYPE_PICK` and `addon_space_subtype_get` no longer exist.
See [ADR-006](../decisions/adr_006_curated_addon_list_vs_auto_derived.md)
and [ADR-007](../decisions/adr_007_native_tree_view_vs_flat_list.md).

The system creates or destroys no `SpaceType` at runtime, and there is no
dynamic registration, no runtime `SpaceType` allocation, and no
unregister-crash risk from a torn-down space type. See
[Add-on Space Type vs Dynamic Registration](../decisions/adr_001_addon_space_type_vs_dynamic_registration.md)
for the rejected alternative.

## DNA

| Field | Location | Purpose |
|---|---|---|
| `SPACE_ADDON` enum value | `makesdna/DNA_space_enums.h` | The one registered space type. |
| `SpaceAddon { SpaceLink; char addon_id[128]; }` | `makesdna/DNA_space_types.h` | Per-area state: which add-on this area hosts. |
| `UserDef.addon_editors` (`bAddonEditor` list) | `makesdna/DNA_userdef_types.h` | A display name override, read only. Nothing fills it now. The picker that filled it was removed. |

The DNA stores the add-on's module name as a string,
`SpaceAddon::addon_id`, not an index. An index is not stable across
sessions, because enabling or disabling an add-on reorders them. This
mirrors the Node Editor, which stores the node tree type idname in
`SpaceNode`.

## Data flow

```
editor type menu (rna_Area_ui_type_itemf)
  └─ one plain "Add-on" entry, no subtypes

sidebar Add-ons tree (AddonTreeView::build_tree)
  └─ every enabled add-on in U.addons that registers panels
       └─ activating a row writes SpaceAddon::addon_id

ScrArea (spacetype = SPACE_ADDON)
  └─ SpaceAddon.addon_id = "mytool"          ← stored in DNA, not the index

window region draw
  └─ addon_panel_types_collect("mytool")     ← filtered ListBaseT<PanelType>
       └─ ED_region_panels_layout_ex(C, region, list, …)

context lookup
  screen layer  → object / scene / mode / …   already works
  area  layer   → space_data / region_data    delegated to a real editor
```

See [Panel Hosting](./panel_hosting.md) for the panel-collection and layout
step, and [Context Delegation](./context_delegation.md) for the context
step.

## Registration files

| # | Change | File(s) |
|:-:|:---|:---|
| 1 | `SPACE_ADDON` enum value; `SpaceAddon` struct | `makesdna/DNA_space_enums.h`, `makesdna/DNA_space_types.h` |
| 2 | `UserDef.addon_editors` list and RNA | `makesdna/DNA_userdef_types.h`, `makesrna/intern/rna_userdef.cc` |
| 3 | Editor module: space callbacks, region init/draw, context delegation | `editors/space_addon/` (new) |
| 4 | Sidebar tree view that chooses the hosted add-on | `editors/space_addon/addon_tree_view.cc` (new) |
| 5 | Register the new space type in the editor init table | `editors/include/ED_space_api.hh`, `editors/space_api/spacetypes.cc` |

## Sentinel values, superseded

This section described the enum packing of the removed subtype mechanism.
The code no longer packs `space_type << 16 | subtype` for this editor, and
`ADDON_SUBTYPE_PICK` no longer exists. The record stays because the
`0x7FFF` sentinel caused a shipped defect, and the reasoning applies to any
future use of the packing.

- `ScrArea::butspacetype_subtype == -1` is Blender's own reserved value
  meaning "not yet determined, call `space_subtype_get()`"
  (`area.cc:2961`). A picker entry must not reuse `-1`. It would never
  reach the `set` callback, and the packing leaves its bit pattern
  unchanged, which decodes back to an invalid space type.
- The fork used `0x7FFF`, the maximum a `short` subtype field holds, for
  the "Add an Add-on..." entry.
- The `get` fallback returned that sentinel rather than `0`. Returning `0`
  collided with the "Add-ons" heading item, which also decodes to `0` after
  the `SPACE_ADDON << 16` OR, and produced a blank area type button icon.

## Extension module identity

`BPY_class_module_name_get` originally kept only the first dot-segment of
`__module__`. Extensions (Blender's package system) import as
`bl_ext.<repository>.<addon>...`, so every extension-installed add-on
collapsed to the identity `"bl_ext"`. The `bl_` prefix exclusion then
filtered it out, hiding every extension add-on from the picker. Fixed to
keep three segments specifically for that prefix.

## Curated list, not an auto-derived list

The editor dropdown does not list every add-on that happens to register
panels. It shows `UserDef.addon_editors`, a persistent, user-curated list.
See
[Curated Add-on List vs Auto-Derived List](../decisions/adr_006_curated_addon_list_vs_auto_derived.md)
for why, and [UX: Editor-Type Picker](../design/ux_addon_picker.md) for the
picker UX itself.

## Related

- [Panel Hosting](./panel_hosting.md)
- [Context Delegation](./context_delegation.md)
- [Persistence and Compatibility](./persistence_and_compatibility.md)
