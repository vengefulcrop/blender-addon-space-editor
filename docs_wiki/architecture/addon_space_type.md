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

## One space type, many subtypes

`SPACE_ADDON` is one registered space type. Every hosted add-on is a
subtype of it, not a separate space type. `SpaceType` already carries a
subtype mechanism, at `BKE_screen.hh:156-158`:

```cpp
int  (*space_subtype_get)(ScrArea *area);
void (*space_subtype_set)(ScrArea *area, int value);
void (*space_subtype_item_extend)(bContext *C, EnumPropertyItem **item, int *totitem);
```

`rna_Area_ui_type_itemf` folds subtypes into the editor dropdown, at
`rna_screen.cc:216-233`. The code packs the enum value as
`space_type << 16 | subtype`. This is the same mechanism the Node Editor
uses to present Shader, Compositor, and Geometry Nodes as three separate
dropdown entries while being one registered space type.

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
| `UserDef.addon_editors` (`bAddonEditor` list) | `makesdna/DNA_userdef_types.h` | Persistent, user-curated list of add-ons offered in the editor dropdown. Mirrors the existing `bAddon` pattern. |

`ScrArea::spacetype` subtype indices are not stable across sessions,
because add-on enable/disable reorders them. The index is a view concern
only. The DNA stores the add-on's module name as a string,
`SpaceAddon::addon_id`. This mirrors the Node Editor, which stores the
node-tree type idname in `SpaceNode` and resolves it to an index in
`space_subtype_get`.

## Data flow

```
UserDef.addon_editors            persistent, survives .blend files
  └─ "mytool", "node_wrangler"   add-ons the user activated as editors

editor dropdown  (rna_Area_ui_type_itemf)
  └─ addon_space_subtype_item_extend()
       ├─ one entry per UserDef.addon_editors
       └─ "Add an Add-on..."  → opens a search popup over installed add-ons

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
| 4 | `space_subtype_get` / `_set` / `_item_extend`, including the "Add an Add-on..." entry | `editors/space_addon/space_addon.cc` |
| 5 | Register the new space type in the editor init table | `editors/include/ED_space_api.hh`, `editors/space_api/spacetypes.cc` |

## Sentinel values

The editor-type dropdown packs `space_type << 16 | subtype` into one enum
value (see above). Two sentinel values matter for this packing:

- `ScrArea::butspacetype_subtype == -1` is Blender's own reserved value
  meaning "not yet determined, call `space_subtype_get()`"
  (`area.cc:2952`). The "Add an Add-on..." entry must not reuse `-1`: it
  would never reach the `set` callback, and the packing leaves its bit
  pattern unchanged, which would decode back to an invalid space type.
- The fork uses `0x7FFF` — the maximum value the `short` subtype field can
  hold — for the "Add an Add-on..." entry (`ADDON_SUBTYPE_PICK`), verified
  against both the packing and unpacking code.
- `addon_space_subtype_get()`'s fallback (nothing selected, or the
  filtering rules remove the selected entry) also returns
  `ADDON_SUBTYPE_PICK` rather than `0`. Returning `0` collided with the
  "Add-ons" heading item, which also decodes to value `0` after the
  `SPACE_ADDON << 16` OR, and produced a blank area-type button icon.
  `ADDON_SUBTYPE_PICK` has a real icon (`ICON_ADD`) and is collision-free.

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
