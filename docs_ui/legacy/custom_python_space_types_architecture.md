# Custom Python Space Types Architecture & Implementation Plan

## Overview

This document outlines the technical research, architectural requirements, code volume estimates, and maintenance plan for implementing **custom Python-registered Space Types (Area Types)** in the Blender C++ codebase.

This feature extends the paradigm of `bpy.types.Panel` (n-panel API) to allow custom scripts and add-ons to define entire custom screen areas/editors driven fully by Python and Blender's `UILayout` widget framework.

---

## 1. Technical Architecture & Component Changes

### 1.1 Data Structures & DNA Layer (`makesdna`)
* **Space Type Enum (`DNA_space_enums.h`)**:
  - Add `SPACE_PYTHON = 25` (or a designated range for dynamic spaces) to `eSpace_Type`.
* **Generic Space Struct (`DNA_space_types.h`)**:
  - Define `SpacePython` extending `SpaceLink`:
    ```cpp
    struct SpacePython {
      SpaceLink space_link; // spacetype = SPACE_PYTHON
      char idname[64];      // Stores Python class bl_idname (e.g., "MY_PT_custom_space")
    };
    ```

### 1.2 Kernel Space Registration (`blenkernel`)
* **Dynamic Registration (`BKE_screen.hh` / `screen.cc`)**:
  - Extend `BKE_spacetype_register()` and `BKE_spacetype_from_id()` to manage dynamic runtime space types registered via Python.
  - Implement lifecycle management (creation, cleanup, teardown, and blend-file load fallbacks).

### 1.3 Python RNA Bindings (`makesrna` & `python`)
* **Expose `bpy.types.Space` Base Class (`bpy_rna.cc`)**:
  - Include `bpy.types.Space` in registerable Python types (`BPY_TYPEDEF_REGISTERABLE_DOC`).
* **Registration Callbacks (`rna_space.cc`)**:
  - Implement `rna_Space_register` and `rna_Space_unregister` callbacks.
  - Extract Python class properties (`bl_idname`, `bl_label`, `bl_icon`, `bl_description`, `draw`, `draw_header`, `poll`).
  - Create a C++ `SpaceType` struct and register default regions (`RGN_TYPE_HEADER` and `RGN_TYPE_WINDOW`).

### 1.4 UI Header Integration (`makesrna`)
* **Dynamic Area Selector Dropdown (`rna_screen.cc`)**:
  - Update `rna_Area_ui_type_itemf()` to dynamically query registered space types from `BKE_spacetypes_list()`.
  - Automatically populate custom Python spaces into the top-left area type selector menu across all headers.

### 1.5 Custom Space Editor Module (`editors/space_custom`)
* Create a dedicated editor handler module in `source/blender/editors/space_custom/`:
  - Implement layout callbacks for `RGN_TYPE_WINDOW` and `RGN_TYPE_HEADER`.
  - Wrap region rendering with UI block management (`ui::block_begin` / `ui::block_end`).
  - Invoke Python's `draw(context)` method with a `UILayout` handle to generate widgets natively.

---

## 2. Estimated Code Volume Breakdown

| Component | Target File(s) | Estimated LOC | Scope |
| :--- | :--- | :---: | :--- |
| **DNA / Serialization** | `source/blender/makesdna/DNA_space_enums.h`<br>`source/blender/makesdna/DNA_space_types.h` | ~40 - 60 | `SPACE_PYTHON` enum & `SpacePython` DNA struct definition. |
| **Blenkernel API** | `source/blender/blenkernel/BKE_screen.hh`<br>`source/blender/blenkernel/intern/screen.cc` | ~80 - 120 | Dynamic space lookup, registration, teardown & fallback logic. |
| **RNA & Python API** | `source/blender/makesrna/intern/rna_space.cc`<br>`source/blender/python/intern/bpy_rna.cc` | ~300 - 400 | `rna_Space_register/unregister`, Python attribute extraction & `bpy.types.Space` export. |
| **UI Header Integration** | `source/blender/makesrna/intern/rna_screen.cc` | ~50 - 70 | Inject custom spaces into `rna_Area_ui_type_itemf` area type dropdown. |
| **Custom Editor Module** | `source/blender/editors/space_custom/` *(New)* | ~450 - 600 | C++ callbacks connecting window region layout to Python `UILayout`. |
| **Versioning & Fallbacks** | `source/blender/blenloader/intern/versioning_*.cc` | ~30 - 50 | Safe handling of `.blend` files when loaded without the add-on active. |

**Total Technical Volume**: **~850 – 1,200 LOC** across ~8 existing files + 1 new module directory.

---

## 3. Maintenance & Technical Risk Requirements

### 3.1 Add-on Unregistration & Live-Reload Safety
* **Risk**: Add-on developers reload scripts frequently (`bpy.utils.unregister_class`).
* **Maintenance**: When a custom space class is unregistered while an active workspace area displays it, Blender must immediately fall back `ScrArea.spacetype` to `SPACE_VIEW3D` or `SPACE_EMPTY` before Python destroys class pointers, preventing crashes.

### 3.2 `.blend` File Forward/Backward Compatibility
* **Risk**: Scenes saved with custom Python space types opened on machines without the add-on installed.
* **Maintenance**:
  - `BKE_screen_area_blend_read_lib()` must gracefully fall back unknown space types to `SPACE_VIEW3D` without stripping space data.
  - Re-enabling the add-on must restore the custom area type seamlessly without workspace corruption.

### 3.3 Context & Event Loop Alignment
* **Risk**: Navigation, keyboard shortcuts, and modal operations inside custom areas.
* **Maintenance**: Provide clean keymap (`wmKeyConfig`) and event pass-through hooks so custom spaces can intercept or pass unhandled input back to the window manager.

### 3.4 UI Engine Updates
* **Risk**: Internal UI framework updates across major Blender versions (e.g., 4.x → 5.x).
* **Maintenance**: Maintain stability for `UILayout` wrapper calls inside the space region drawing callback.
