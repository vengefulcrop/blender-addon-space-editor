# Verified Native Blender APIs and Subsystems

**Prefix:** `Gemini_`  
**Purpose:** Direct code-level verification of all native Blender APIs, structs, and functions proposed as drop-in replacements for handrolled mechanisms in `pyareas/addon-space-editor`.

---

## 1. Context Overrides and Data Forwarding

### `struct bContextStore` / `CTX_store_*`
- **Definition Header:** [`source/blender/blenkernel/BKE_context.hh:122`](source/blender/blenkernel/BKE_context.hh#L122)
- **Implementation File:** [`source/blender/blenkernel/intern/context.cc:173-200`](source/blender/blenkernel/intern/context.cc#L173-L200)
- **Exact Signatures:**
  ```cpp
  bContextStore *CTX_store_add(Vector<std::unique_ptr<bContextStore>> &contexts,
                               const char *name,
                               const PointerRNA *ptr);
  const bContextStore *CTX_store_get(const bContext *C);
  void CTX_store_set(bContext *C, const bContextStore *store);
  const PointerRNA *CTX_store_ptr_lookup(const bContextStore *store, const char *name);
  ```
- **Usage in Codebase:** Used in [`source/blender/editors/interface/interface.cc:5780`](source/blender/editors/interface/interface.cc#L5780) and UI layout execution to cleanly attach temporary pointer overrides (`area`, `region`, `space_data`, `material`, etc.) to a context query without altering global accessors.

### `bpy.context.temp_override(...)`
- **Definition File:** [`source/blender/python/intern/bpy_rna_context.cc:711`](source/blender/python/intern/bpy_rna_context.cc#L711)
- **Method Export:** [`source/blender/python/intern/bpy_rna_context.cc:844`](source/blender/python/intern/bpy_rna_context.cc#L844) (`BPY_rna_context_temp_override_method_def`)
- **Exact Signature:** `temp_override(*, window=None, screen=None, area=None, region=None, **keywords)`
- **Usage in Codebase:** Standard context manager in Python operators and UI scripts for overriding active area and region without global state pollution.

### `SpaceType::context` Callback
- **Definition Struct:** [`source/blender/blenkernel/BKE_screen.hh:178`](source/blender/blenkernel/BKE_screen.hh#L178) (`SpaceType`)
- **Signature:** `int (*context)(const bContext *C, const char *member, bContextDataResult *result)`
- **Usage in Codebase:** Registered per space type (e.g. `node_context` in [`space_node.cc`](source/blender/editors/space_node/space_node.cc), `sequencer_context` in [`space_sequencer.cc`](source/blender/editors/space_sequencer/space_sequencer.cc)). Fulfills space-specific context properties directly.

---

## 2. Panel Layout and Filtering

### `ED_region_panels_layout_ex` & `ED_region_panels_layout`
- **Header:** [`source/blender/editors/include/ED_screen.hh:121-137`](source/blender/editors/include/ED_screen.hh#L121-L137)
- **Implementation:** [`source/blender/editors/screen/area.cc:3371-3665`](source/blender/editors/screen/area.cc#L3371-L3665)
- **Exact Signatures:**
  ```cpp
  void ED_region_panels_layout_ex(const bContext *C,
                                  ARegion *region,
                                  ListBaseT<PanelType> *paneltypes,
                                  wm::OpCallContext op_context,
                                  const char *contexts[],
                                  const char *category_override);

  void ED_region_panels_layout(const bContext *C, ARegion *region);
  ```
- **Usage in Codebase:** Used in [`space_buttons.cc:315`](source/blender/editors/space_buttons/space_buttons.cc#L315) (Properties Editor), [`space_view3d.cc:1311`](source/blender/editors/space_view3d/space_view3d.cc#L1311), [`space_image.cc:969`](source/blender/editors/space_image/space_image.cc#L969) to draw filtered lists of panels without cloning `PanelType` structs.

### `panel_add_check`
- **Definition File:** [`source/blender/editors/screen/area.cc:3320`](source/blender/editors/screen/area.cc#L3320)
- **Exact Signature:** `static bool panel_add_check(const bContext *C, WorkSpace *workspace, const char *contexts[], const char *category_override, PanelType *panel_type)`
- **Role:** Handles context array matching, workspace owner checks, and RNA poll evaluation in one centralized location.

### Category Tabs: `ui::panel_category_tabs_draw_all` / `ui::panel_category_*`
- **Header:** [`source/blender/editors/include/UI_interface_c.hh:2240-2256`](source/blender/editors/include/UI_interface_c.hh#L2240-L2256)
- **Implementation:** [`source/blender/editors/interface/interface_panel.cc:1458`](source/blender/editors/interface/interface_panel.cc#L1458)
- **Exact Signatures:**
  ```cpp
  bool panel_category_is_visible(const ARegion *region);
  bool panel_category_tabs_is_visible(const ARegion *region);
  void panel_category_tabs_draw_all(const bContext *C, ARegion *region, const char *active_category);
  ```
- **Usage in Codebase:** Used by [`area.cc:3710`](source/blender/editors/screen/area.cc#L3710) to draw tab strips on sidebar and window regions natively.

---

## 3. UI Operators and Search Popups

### `wmWindowManager.invoke_search_popup`
- **Python / RNA Binding:** [`scripts/startup/bl_ui/space_addon.py:404`](scripts/startup/bl_ui/space_addon.py#L404)
- **C Implementation:** [`source/blender/makesrna/intern/rna_wm.cc:3494`](source/blender/makesrna/intern/rna_wm.cc#L3494) (`rna_WindowManager_invoke_search_popup`)
- **Usage in Codebase:** Triggers search popups over an operator's `bl_property` enum, standard across search menus.

### Header Menu and Operator Rendering (`Layout::menu` / `menutype_draw`)
- **Implementation:** [`source/blender/editors/interface/interface_layout.cc:3066-6244`](source/blender/editors/interface/interface_layout.cc#L3066-L6244)
- **Usage in Codebase:** Area headers invoke operators and dynamic menus directly using `layout.operator()` or `layout.menu()` without intercepting string assignments.

---

## 4. Metadata and Add-on Attribution

### `PanelType::owner_id`
- **Definition:** [`source/blender/blenkernel/BKE_screen.hh:383`](source/blender/blenkernel/BKE_screen.hh#L383)
- **Declaration:** `char owner_id[128];`
- **Role:** Identifies the owning add-on/extension/workspace tag on `PanelType`, `MenuType`, and `wmKeyMap`.

### `BKE_workspace_owner_id_check`
- **Header:** [`source/blender/blenkernel/BKE_workspace.hh:183`](source/blender/blenkernel/BKE_workspace.hh#L183)
- **Implementation:** [`source/blender/blenkernel/intern/workspace.cc:613`](source/blender/blenkernel/intern/workspace.cc#L613)
- **Exact Signature:** `bool BKE_workspace_owner_id_check(const WorkSpace *workspace, const char *owner_id);`
- **Usage in Codebase:** Verified in [`area.cc:3323`](source/blender/editors/screen/area.cc#L3323) and [`wm_menu_type.cc:99`](source/blender/windowmanager/intern/wm_menu_type.cc#L99) to check if a registered UI element belongs to an active workspace or extension.

---

## 5. Event Listeners & Redraw Notifiers

### `SpaceType::listener` & `ARegionType::listener`
- **Definitions:** [`source/blender/blenkernel/BKE_screen.hh:183`](source/blender/blenkernel/BKE_screen.hh#L183) (`SpaceType`), [`source/blender/blenkernel/BKE_screen.hh:281`](source/blender/blenkernel/BKE_screen.hh#L281) (`ARegionType`)
- **Exact Signatures:**
  ```cpp
  void (*listener)(const wmSpaceTypeListenerParams *params);
  void (*listener)(const wmRegionListenerParams *params);
  ```
- **Usage in Codebase:** Implemented across all editor regions (e.g. [`buttons_main_region_listener`](source/blender/editors/space_buttons/space_buttons.cc#L584), [`view3d_buttons_region_listener`](source/blender/editors/space_view3d/space_view3d.cc#L1320)). Inspects `params->notifier->category` for targeted redraws.
