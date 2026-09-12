---
type: research
title: "Verified Native Blender APIs and Subsystems"
description: "Code-level verification of native Blender APIs proposed as replacements for hand-rolled mechanisms in the Add-on Space Editor"
tags: [audit, addon-editor, native-apis, reference]
last_updated: 2026-09-12
sources:
  - id: external-verified-apis
    resource: docs_ui/audits/Gemini_verified_native_apis.md
    title: "External reference: verified native Blender APIs"
    author: gemini/reviewer
---

# Verified Native Blender APIs and Subsystems

Source: `Gemini_verified_native_apis.md`. Gemini wrote it as an external
reviewer. It holds a direct code-level verification of every native Blender API, struct, and function
proposed as a drop-in replacement for hand-rolled mechanisms in
`pyareas/addon-space-editor`. See
[`native_equivalents.md`](./native_equivalents.md) for the verdicts on whether
each API is actually a valid replacement in context.

## 1. Context overrides and data forwarding

### `struct bContextStore` / `CTX_store_*`

- Definition header: `source/blender/blenkernel/BKE_context.hh:122`
- Implementation: `source/blender/blenkernel/intern/context.cc:173-200`
- Signatures:

```cpp
bContextStore *CTX_store_add(Vector<std::unique_ptr<bContextStore>> &contexts,
                             const char *name,
                             const PointerRNA *ptr);
const bContextStore *CTX_store_get(const bContext *C);
void CTX_store_set(bContext *C, const bContextStore *store);
const PointerRNA *CTX_store_ptr_lookup(const bContextStore *store, const char *name);
```

- Used in `source/blender/editors/interface/interface.cc:5780` and UI layout
  execution, to attach temporary pointer overrides (`area`, `region`,
  `space_data`, `material`, and others) to a context query without altering
  global accessors.

Note: `native_equivalents.md` #3 confirms `bpy.context.temp_override()` does
**not** call into this struct, despite sharing conceptual purpose. Treat the two
as separate mechanisms.

### `bpy.context.temp_override(...)`

- Definition file: `source/blender/python/intern/bpy_rna_context.cc:711`
- Method export: `source/blender/python/intern/bpy_rna_context.cc:844`
  (`BPY_rna_context_temp_override_method_def`)
- Signature: `temp_override(*, window=None, screen=None, area=None,
  region=None, **keywords)`
- Used as a standard context manager in Python operators and UI scripts, to
  override the active area and region without global state pollution.

### `SpaceType::context` callback

- Definition struct: `source/blender/blenkernel/BKE_screen.hh:178`
  (`SpaceType`)
- Signature: `int (*context)(const bContext *C, const char *member,
  bContextDataResult *result)`
- Registered per space type, for example `node_context` in `space_node.cc`,
  `sequencer_context` in `space_sequencer.cc`. Fulfills space-specific context
  properties directly.

## 2. Panel layout and filtering

### `ED_region_panels_layout_ex` and `ED_region_panels_layout`

- Header: `source/blender/editors/include/ED_screen.hh:121-137`
- Implementation: `source/blender/editors/screen/area.cc:3371-3665`
- Signatures:

```cpp
void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBaseT<PanelType> *paneltypes,
                                wm::OpCallContext op_context,
                                const char *contexts[],
                                const char *category_override);

void ED_region_panels_layout(const bContext *C, ARegion *region);
```

- Used in `space_buttons.cc:315` (Properties Editor), `space_view3d.cc:1311`,
  and `space_image.cc:969`, to draw filtered lists of panels without cloning
  `PanelType` structs. Note: `ED_region_panels_layout_ex` walks the passed
  `paneltypes` list via its own intrusive `next`/`prev` links, which is why the
  Add-on Space Editor still needs to clone `PanelType` structs for panels it did
  not register natively. See `native_equivalents.md` #9.

### `panel_add_check`

- Definition file: `source/blender/editors/screen/area.cc:3320`
- Signature: `static bool panel_add_check(const bContext *C, WorkSpace
  *workspace, const char *contexts[], const char *category_override, PanelType
  *panel_type)`
- Handles context array matching, workspace owner checks, and RNA poll
  evaluation in one centralized location.

### Category tabs: `ui::panel_category_tabs_draw_all` / `ui::panel_category_*`

- Header: `source/blender/editors/include/UI_interface_c.hh:2240-2256`
- Implementation: `source/blender/editors/interface/interface_panel.cc:1458`
- Signatures:

```cpp
bool panel_category_is_visible(const ARegion *region);
bool panel_category_tabs_is_visible(const ARegion *region);
void panel_category_tabs_draw_all(const bContext *C, ARegion *region, const char *active_category);
```

- Used by `area.cc:3710` to draw tab strips on sidebar and window regions
  natively.

## 3. UI operators and search popups

### `wmWindowManager.invoke_search_popup`

- Python/RNA binding: `scripts/startup/bl_ui/space_addon.py:404`
- C implementation: `source/blender/makesrna/intern/rna_wm.cc:3494`
  (`rna_WindowManager_invoke_search_popup`)
- Triggers a search popup over an operator's `bl_property` enum, standard
  across search menus. Already used by the Add-on Space Editor's own
  `ADDON_OT_pick_and_host` operator.

### Header menu and operator rendering (`Layout::menu` / `menutype_draw`)

- Implementation: `source/blender/editors/interface/interface_layout.cc:3066-6244`
- Area headers invoke operators and dynamic menus directly with
  `layout.operator()` or `layout.menu()`, without intercepting string
  assignments.

## 4. Metadata and add-on attribution

### `PanelType::owner_id`

- Definition: `source/blender/blenkernel/BKE_screen.hh:383`
- Declaration: `char owner_id[128];`
- Identifies the owning add-on, extension, or workspace tag on `PanelType`,
  `MenuType`, and `wmKeyMap`. See `native_equivalents.md` #11 for why this field
  is not a suitable add-on identity mechanism for the Add-on Space Editor: it is
  gated behind an opt-in `bl_owner_id`, empty for most panels.

### `BKE_workspace_owner_id_check`

- Header: `source/blender/blenkernel/BKE_workspace.hh:183`
- Implementation: `source/blender/blenkernel/intern/workspace.cc:613`
- Signature: `bool BKE_workspace_owner_id_check(const WorkSpace *workspace,
  const char *owner_id);`
- Verified in `area.cc:3323` and `wm_menu_type.cc:99`, to check whether a
  registered UI element belongs to an active workspace or extension.

## 5. Event listeners and redraw notifiers

### `SpaceType::listener` and `ARegionType::listener`

- Definitions: `source/blender/blenkernel/BKE_screen.hh:183` (`SpaceType`),
  `source/blender/blenkernel/BKE_screen.hh:281` (`ARegionType`)
- Signatures:

```cpp
void (*listener)(const wmSpaceTypeListenerParams *params);
void (*listener)(const wmRegionListenerParams *params);
```

- Implemented across all editor regions, for example
  `buttons_main_region_listener` (`space_buttons.cc:584`) and
  `view3d_buttons_region_listener` (`space_view3d.cc:1320`). These inspect
  `params->notifier->category` for targeted redraws. Note:
  `native_equivalents.md` #12 confirms the Add-on Space Editor already uses this
  exact field (`space_addon.cc:880`), with a deliberately broad listener body,
  not a missing mechanism.
