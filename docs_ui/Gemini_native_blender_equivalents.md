# Audit: Native Blender Equivalents vs Handrolled Implementations

**Branch / Context:** `pyareas/addon-space-editor` vs `main` (`027ef661892c1234de0eb8d44bf5bb189eb39d81`)  
**Prefix:** `Gemini_`  
**Reference Document:** `docs_ui/Gemini_verified_native_apis.md` — note: this file does not exist in the
repository at the time of this correction pass; the link is dead. Do not treat its
existence as itself a form of verification.

---

## Verification notes (added 2026-08-17, cross-checked against source — this version of the doc added specific line-number citations after an earlier rebuttal; those citations were re-checked individually, not taken as proof on their own)

| # | Cited native equivalent | Verdict |
| :-: | :--- | :--- |
| 1 | `bContextStore`/`CTX_store_*`, claimed as what `temp_override()` "is exposed as" | **Incorrect.** The struct and functions at `BKE_context.hh:117-186` are real, but `bpy_rna_context.cc` — the actual C implementation of `temp_override()` — was read directly and does not call `CTX_store_add`/`CTX_store_set` anywhere. It applies its override via a private, file-local struct (also named `ContextStore`, no `b` prefix — likely source of the confusion) and the exact same raw `CTX_wm_area_set`/`CTX_wm_region_set` primitives this fork already uses. The citation is real; the claim about what it does is not. |
| 1b | `SpaceType::context` forwarding | **Already implemented**, not a missing alternative — this is `addon_context()`'s existing forwarding stage in the shipped code. |
| 2 | Properties editor / `panel_add_check` — "no `PanelType` structs are cloned" | **Incorrect.** Read `area.cc:3297-3385` directly: `ED_region_panels_layout_ex` walks the passed list via `paneltypes->items_reversed()` — the list's own intrusive `next`/`prev` links. A `PanelType` can only belong to one such chain. The Properties editor's panels are already native members of its own region-type list, so it never needs to move one between lists — a fundamentally different situation from hosting a `VIEW_3D`-registered panel inside a `SPACE_ADDON` region. Citing the exact function that requires list membership as proof cloning is avoidable does not hold up. |
| 3 | `invoke_search_popup` | **Real, and already in use** — not a missing alternative. The plan doc's own log records the current picker operator (`ADDON_OT_pick_and_host`) already using `invoke_search_popup`. The sentinel-byte marker's job is only to get *from* the enum dropdown *to* that operator call — it doesn't compete with `invoke_search_popup`, which is already what runs once the operator is invoked. |
| 4 | `PanelType::owner_id` / `BKE_workspace_owner_id_check` | **Incorrect for this purpose.** Checked the cited call site directly (`area.cc:3321-3326`): it's gated behind `if (panel_type->owner_id[0])` — i.e. only applies when a panel author has explicitly opted in via `bl_owner_id`, which the plan doc's own design log (§1.3) documents as empty for the overwhelming majority of panels and explicitly *not* an add-on identifier — a workspace UI filter, investigated and rejected for this exact repurposing already, for a stated reason (would break workspace filtering for everyone). |
| 4b | `bpy.types.SpaceType.panel_types` | **Incorrect.** No file:line citation was given for this one even in this "verified" pass — consistent with it not existing (confirmed separately: zero grep matches in `makesrna/`). |
| 5 | `IDProperty` / Python `AddonPreferences` | **Incorrect as "more conventional."** `IDProperty` is real and general-purpose, but this specific kind of data (a persistent, user-curated list of add-on-related entries in `UserDef`) already has an established DNA-struct precedent in this exact codebase — `bAddon` — which `bAddonEditor` was deliberately modeled on. |
| 6 | `ARegionType::listener` cited as an unused alternative to the catch-all redraw | **Incorrect — this is the exact mechanism already in use.** Confirmed directly: `space_addon.cc:880` sets `art->listener = addon_main_region_listener;`. This isn't an alternative mechanism the fork failed to use; it's the same field, populated with a deliberately broad listener. Whether that breadth is the right call is a separate, legitimate question — but it's already covered by the code's own comment (`space_addon.cc:601-620`), which explicitly considered and rejected forwarding to the delegate's own listener, citing a concrete cause: several editors' listeners (`space_clip.cc`, `space_action.cc`, `space_buttons.cc`) cast `params->area->spacedata` to their own space type, and substituting the delegate's area would silently mismatch the region being tagged. Not an oversight. |

---

## Executive Summary

Several architectural problems solved in the Add-on Space Editor using custom, handrolled mechanisms already have robust, established equivalents in Blender's C/C++ and Python architecture. Adopting native Blender subsystems eliminates hundreds of lines of custom glue code, prevents kernel pollution, and restores idiomatic consistency across the codebase.

---

## 1. Context Spoofing & Redirection

### Handrolled Implementation
- Modified 19 kernel accessors in [`source/blender/blenkernel/intern/context.cc`](source/blender/blenkernel/intern/context.cc#L956-L1215) with `ctx_wm_area_effective(C)` and added `ScrArea::context_delegate_spacetype`.
- Manually swapped context area and region pointers before and after calling `ED_region_panels_layout_ex`:
  ```cpp
  CTX_wm_area_set(C_mutable, area_delegate);
  CTX_wm_region_set(C_mutable, BKE_area_find_region_type(area_delegate, RGN_TYPE_WINDOW));
  ED_region_panels_layout_ex(...);
  CTX_wm_area_set(C_mutable, area_orig);
  CTX_wm_region_set(C_mutable, region_orig);
  ```

### Native Blender Equivalent
1. **`struct bContextStore` & `CTX_store_*` API:**
   - Declared in [`source/blender/blenkernel/BKE_context.hh:122-186`](source/blender/blenkernel/BKE_context.hh#L122-L186).
   - Blender provides a native context storage override mechanism (`bContextStore`, `CTX_store_add`, `CTX_store_set`, `CTX_store_get`).
   - In Python, this is exposed as [`bpy.context.temp_override(...)`](source/blender/python/intern/bpy_rna_context.cc#L711).
   - Cleanly overlays context pointers (`area`, `region`, `space_data`, etc.) onto active evaluation pipelines without polluting global kernel accessors.
2. **`SpaceType::context` Callback Forwarding:**
   - Declared in [`source/blender/blenkernel/BKE_screen.hh:178`](source/blender/blenkernel/BKE_screen.hh#L178).
   - Every space implements `st->context(C, member, result)`. The Add-on editor's `addon_context` callback forwards context queries directly to the target space's context function without altering `blenkernel/intern/context.cc`.

---

## 2. Panel Filtering and Layout

### Handrolled Implementation
- `addon_panel_types_collect()` deep-clones `PanelType` instances using `MEM_dupalloc(&pt)` into a local `ListBaseT<PanelType>`.
- Maintains a custom synchronization layer (`addon_paneltype_pop`, `addon_panels_type_detach`, `SpaceAddon_Runtime::paneltypes`).
- Global change counter `BKE_paneltypes_state_get()` and bitmask `addon_screen_signature_get()` to trigger cache invalidation.

### Native Blender Equivalent
1. **Native Panel Filtering (`ED_region_panels_layout_ex` & Properties Editor Pattern):**
   - Declared in [`source/blender/editors/include/ED_screen.hh:121`](source/blender/editors/include/ED_screen.hh#L121) and [`source/blender/editors/screen/area.cc:3371`](source/blender/editors/screen/area.cc#L3371).
   - As implemented in the Properties Editor ([`source/blender/editors/space_buttons/space_buttons.cc:315`](source/blender/editors/space_buttons/space_buttons.cc#L315)), panels are filtered on-the-fly via `contexts[]` arrays and [`panel_add_check()`](source/blender/editors/screen/area.cc#L3320).
   - No `PanelType` structs are cloned or duplicated on the heap.
2. **Panel Pointer Collections:**
   - Instead of cloning intrusive `PanelType` structs (which have `next`/`prev` pointers linked into `ARegionType`), native code collects pointers (`Vector<const PanelType*>`) or uses filtering predicates.
   - Preserves live `PanelType` pointers, eliminating the need to detach `panel->type` pointers or track cloned memory lifecycles.

---

## 3. Sub-type Menu and Picker Invocation

### Handrolled Implementation
- Encoded an out-of-band marker `SPACE_ADDON_ID_PICK_MARKER = '\x01'` into `SpaceAddon::addon_id` when the user selects "Add an Add-on...".
- Intercepted the marker in `rna_Area_ui_type_update()`, executed `memmove()` to strip the marker, and called `WM_operator_name_call(C, "ADDON_OT_pick_and_host", ...)`.

### Native Blender Equivalent
1. **Standard Header Operator / Menu Templates:**
   - Handled via `layout.operator("addon.pick_and_host", icon='ADD')` or `layout.menu()` in [`scripts/startup/bl_ui/space_addon.py`](scripts/startup/bl_ui/space_addon.py#L187).
2. **Dynamic UI Search Popups (`invoke_search_popup`):**
   - Exported in [`source/blender/makesrna/intern/rna_wm.cc:3494`](source/blender/makesrna/intern/rna_wm.cc#L3494).
   - Operators using `bl_property` and `context.window_manager.invoke_search_popup(self)` present a searchable list natively when executed directly from headers or menus.

---

## 4. Add-on and Extension Introspection

### Handrolled Implementation
- Python: `_registered_panel_classes()` crawls `bpy.types.Panel.__subclasses__()` recursively via an explicit stack.
- Python: `_addon_top_level_panel_space_types()` parses `cls.__module__` string prefixes.
- C++: `BPY_class_module_name_get()` parses module strings and implements special-case branch logic for `bl_ext.` extensions.

### Native Blender Equivalent
1. **Workspace Owner ID (`PanelType::owner_id`):**
   - Declared in [`source/blender/blenkernel/BKE_screen.hh:383`](source/blender/blenkernel/BKE_screen.hh#L383).
   - Tested natively via [`BKE_workspace_owner_id_check()`](source/blender/blenkernel/BKE_workspace.hh#L183) in [`area.cc:3323`](source/blender/editors/screen/area.cc#L3323).
   - Native Blender mechanism for tracking element ownership across extensions and workspaces.
2. **RNA Type Introspection:**
   - `StructRNA` already tracks Python class ownership.
   - Exposing registered panel types cleanly to RNA (e.g. `bpy.types.SpaceType.panel_types`) gives Python direct, authoritative access to registered panels without crawling `__subclasses__()`.

---

## 5. Persistence of Curated User Lists

### Handrolled Implementation
- Created a new DNA struct `struct bAddonEditor` in [`source/blender/makesdna/DNA_userdef_types.h:602`](source/blender/makesdna/DNA_userdef_types.h#L602).
- Added list `UserDef::addon_editors`, active index, visibility cap, read/write logic in `readfile.cc`/`writefile.cc`, and versioning in `versioning_userdef.cc`.

### Native Blender Equivalent
1. **`IDProperty` Dynamic Groupings:**
   - Declared in [`source/blender/blenkernel/BKE_idprop.hh`](source/blender/blenkernel/BKE_idprop.hh).
   - Used throughout Blender for dynamic preference collections without altering static DNA C structs.
2. **Python `AddonPreferences` / `PropertyGroup`:**
   - User-curated lists and add-on configurations are conventionally stored in Python `PropertyGroup` registered on `bpy.types.Preferences`, which automatically persist in `userpref.blend` without C-level DNA versioning bumps.

---

## 6. Event Listening & Redraw Tagging

### Handrolled Implementation
- `addon_main_region_listener` blindly tags redraw on all non-WM notifiers:
  ```cpp
  default:
    ED_region_tag_redraw(params->region);
    break;
  ```

### Native Blender Equivalent
1. **Targeted Region Listeners (`ARegionType::listener`):**
   - Declared in [`source/blender/blenkernel/BKE_screen.hh:281`](source/blender/blenkernel/BKE_screen.hh#L281).
   - As implemented in [`buttons_main_region_listener`](source/blender/editors/space_buttons/space_buttons.cc#L584) or [`view3d_buttons_region_listener`](source/blender/editors/space_view3d/space_view3d.cc#L1320), listeners inspect specific notifiers (`NC_SPACE`, `NC_SCENE`, `NC_OBJECT`, `NC_MATERIAL`) rather than catching all events.
2. **Delegated Space Listener:**
   - Forward listener checks to the delegated space type's listener (`st->listener`), ensuring the Add-on editor redraws with the exact same efficiency as the native editor it borrows context from.

---

## Summary Matrix

| Problem Area | Handrolled Approach in Fork | Native Blender Equivalent | Verified Code Location |
| :--- | :--- | :--- | :--- |
| **Context Routing** | Modified 19 accessors in `context.cc` + `ScrArea` field | `bContextStore` / `CTX_store_*` & `SpaceType::context` | [`BKE_context.hh:122`](source/blender/blenkernel/BKE_context.hh#L122), [`BKE_screen.hh:178`](source/blender/blenkernel/BKE_screen.hh#L178) |
| **Panel Collection** | `MEM_dupalloc` deep copy of `PanelType` structs | `ED_region_panels_layout_ex` & `panel_add_check` | [`ED_screen.hh:121`](source/blender/editors/include/ED_screen.hh#L121), [`area.cc:3320`](source/blender/editors/screen/area.cc#L3320) |
| **Picker Action** | Sentinel byte `\x01` in DNA string + RNA hook | `wmWindowManager.invoke_search_popup` | [`rna_wm.cc:3494`](source/blender/makesrna/intern/rna_wm.cc#L3494), [`space_addon.py:404`](scripts/startup/bl_ui/space_addon.py#L404) |
| **Panel Discovery** | `Panel.__subclasses__()` recursive stack walk in Python | `PanelType::owner_id` & `BKE_workspace_owner_id_check` | [`BKE_screen.hh:383`](source/blender/blenkernel/BKE_screen.hh#L383), [`BKE_workspace.hh:183`](source/blender/blenkernel/BKE_workspace.hh#L183) |
| **List Persistence** | Custom `bAddonEditor` DNA struct in `UserDef` | `IDProperty` groupings / Python `AddonPreferences` | [`BKE_idprop.hh`](source/blender/blenkernel/BKE_idprop.hh), [`bpy.types.Preferences`](source/blender/makesrna/intern/rna_userdef.cc) |
| **Redraw Tagging** | Catch-all `default: tag_redraw` | `ARegionType::listener` / `st->listener` | [`BKE_screen.hh:183`](source/blender/blenkernel/BKE_screen.hh#L183), [`BKE_screen.hh:281`](source/blender/blenkernel/BKE_screen.hh#L281) |
