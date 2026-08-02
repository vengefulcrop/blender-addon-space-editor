# Custom Asset Browser Asset Types Python API Architecture

## 1. Analysis of Existing Python-Driven Custom Type Systems

### 1.1 How Custom Types Work in Blender
Blender allows Python add-ons to define custom types (e.g. `bpy.types.Node`, `bpy.types.NodeTree`, `bpy.types.NodeSocket`, `bpy.types.Gizmo`, `bpy.types.UIList`, `bpy.types.Panel`, `bpy.types.Menu`, `bpy.types.Header`).

The architecture follows a unified pattern across C++/RNA/Python:
1. **RNA Registration Function**:
   In C++ RNA definition files (e.g. [`source/blender/makesrna/intern/rna_nodetree.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_nodetree.cc#L9564)), types register registration callbacks:
   ```cpp
   RNA_def_struct_register_funcs(srna, "rna_Node_register", "rna_Node_unregister", nullptr);
   ```
2. **Registration Base Routine**:
   When Python subclasses `bpy.types.Node`, `rna_Node_register()` invokes `rna_Node_register_base()`:
   - Validates class properties (`bl_idname`, `bl_label`, `bl_icon`).
   - Allocates a runtime C++ type descriptor (e.g. `bke::bNodeType`).
   - Defines a dynamic RNA struct (`RNA_def_struct_ptr`).
   - Binds C function pointers in `bNodeType` (`poll`, `initfunc_api`, `draw_buttons`, `updatefunc`) to C-to-Python dispatch handlers (`rna_Node_poll`, `rna_Node_init`, `rna_Node_draw_buttons`).
3. **Runtime Registry**:
   The runtime C++ type is registered into global registries (e.g., `bke::node_type_register()`), making custom nodes full first-class citizens in editor drawing, execution, and serialization.

---

## 2. Asset Browser System Architecture & Extension Points

### 2.1 Current Asset Architecture
- **Asset Representation**: [`source/blender/asset_system/AS_asset_representation.hh`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/asset_system/AS_asset_representation.hh) defines `AssetRepresentation` which wraps either a local `ID *` or an external file asset (`ExternalAsset`).
- **Asset Metadata**: [`source/blender/makesdna/DNA_asset_types.h`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_asset_types.h#L98) defines `AssetMetaData` stored directly on `ID::asset_data`. Key fields:
  - `bUUID catalog_id`: Catalog classification.
  - `ListBaseT<AssetTag> tags`: User/system tags.
  - `IDProperty *properties`: Custom metadata key-value storage (supports strings, ints, floats, arrays, groups).
  - `AssetTypeInfo *local_type_info`: Runtime pointer to type callbacks.
- **Asset Type Callbacks**: [`source/blender/blenkernel/BKE_asset.hh`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_asset.hh#L30) defines `AssetTypeInfo` for ID types (`pre_save_fn`, `on_mark_asset_fn`, `on_clear_asset_fn`).
- **Drag and Drop Engine**: [`source/blender/editors/interface/interface_drag.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/interface/interface_drag.cc) and `wm_dragdrop.cc` manage asset drop handling (`wmDragAsset`).

---

## 3. Custom Python Asset Types Architecture (`bpy.types.AssetType`)

### 3.1 Scope of Feature Capabilities
The custom asset type API enables Python add-ons to:
1. **Define Custom Asset Types**: Register new asset categories with custom IDs, icons, and descriptions.
2. **Custom Metadata Schema**: Define structured, typed metadata fields attached to assets.
3. **Associated Datablocks & Python Logic**: Bind custom asset types to specific datablocks (e.g., `NodeTree`, `Text` script, `Object`, or custom presets) and associate Python code execution upon import or instantiation.
4. **Drag & Drop Handlers**: Override default drag-and-drop behavior when dropping an asset into 3D Viewport, Node Editors, or Sequencer.
5. **Custom Asset Browser UI & Tooltips**: Render custom metadata panels, asset shelf tools, and extended tooltips in the Asset Browser.

### 3.2 Registration & Dispatch Mechanism

```
+-----------------------------------+
|  Python Class Subclassing         |
|  bpy.types.AssetType              |
+-----------------------------------+
                  |
                  v  (register)
+-----------------------------------+
|  rna_AssetType_register()         |  <-- RNA_def_struct_register_funcs
+-----------------------------------+
                  |
                  v
+-----------------------------------+
|  AssetTypeInfo Runtime Registry   |  <-- Stores C++ function pointers for Python callbacks
+-----------------------------------+
      |               |           |
      v               v           v
+-----------+   +-----------+   +-------------------+
| UI Draw   |   | Drag&Drop |   | Metadata & Code   |
| Callbacks |   | Handlers  |   | Execution Handler |
+-----------+   +-----------+   +-------------------+
```

#### A. C++ Runtime Registry (`AssetTypeInfo`)
Extend `BKE_asset.hh` and `DNA_asset_types.h` to expand `AssetTypeInfo`:
```cpp
struct AssetTypeInfo {
  char idname[64];
  char ui_name[64];
  int ui_icon;
  
  /* Runtime RNA Extension Pointer */
  rnaImplementationRNA rna_ext;
  
  /* Callbacks */
  bool (*poll_drop)(const bContext *C, const AssetRepresentation *asset, const wmDrag *drag);
  bool (*on_drop)(bContext *C, const AssetRepresentation *asset, const wmDrag *drag);
  void (*draw_metadata)(const bContext *C, uiLayout *layout, AssetMetaData *metadata);
  void (*generate_preview)(const AssetRepresentation *asset, PreviewImage *preview);
  void (*on_mark_asset)(AssetMetaData *metadata);
  void (*on_clear_asset)(AssetMetaData *metadata);
};
```

#### B. Re-using Existing Data Structures (`AssetMetaData.properties`)
- **Metadata Persistence**: Re-uses `AssetMetaData::properties` (`IDProperty`). When a user sets custom metadata on an asset, it is automatically serialized into `AssetMetaData.properties` as an `IDProperty` tree.
- **Zero DNA Changes**: No new fields in `DNA_asset_types.h` are required for metadata storage! The existing `IDProperty` pipeline supports full serialization to `.blend` files out of the box.

---

## 4. Architectural Comparison & Optimal Pathway

### Technical Pathway Analysis

| Criteria | Pathway A: Kernel / DNA Level (New ID Type) | Pathway B: High-Level Infrastructure Extension (Recommended) |
|---|---|---|
| **Mechanism** | Adds a new C `ID` type (e.g. `ID_CUSTOM_ASSET`) in `DNA_ID.h`, updating `bmain`, file IO, blend serialization, depsgraph, and undo systems. | Re-uses existing `ID` datablocks (`ID_NT`, `ID_TXT`, `ID_OB`, `ID_PAL`) or `.blend` asset files paired with `AssetMetaData.properties` (`IDProperty`) + C++ `AssetTypeInfo` registry. |
| **Code Impact** | Invasive edits across ~30+ files in `blenkernel`, `makesdna`, `makesrna`, `blenloader`, `depsgraph`. | Contained entirely within `rna_asset.cc`, `ED_asset`, and `interface_drag.cc`. |
| **Backward Compatibility** | Files saved with new ID code fail to open in older Blender versions. | 100% backward compatible. Older Blender versions safely read standard datablocks and ignore unrecognized `IDProperty` keys. |
| **Functionality** | Full functionality. | Full functionality, plus seamless integration with existing asset libraries and asset shelf UI. |

### Recommendation
**Pathway B (High-Level Infrastructure Extension)** is strictly superior. It delivers all requested Python API capabilities without altering core kernel DNA types or risking file format incompatibility.

---

## 5. Python API Specification (`bpy.types.AssetType`)

### 5.1 Python API Class Definition
```python
import bpy

class CustomAssetType(bpy.types.AssetType):
    bl_idname = "MY_ADDON_custom_preset"
    bl_label = "Custom Addon Preset"
    bl_description = "Custom preset asset with associated Python execution logic"
    bl_icon = 'PRESET'
    
    # Optional: Filter which ID datablocks this asset type can wrap
    id_types = {'NODE_TREE', 'TEXT', 'OBJECT'}

    @classmethod
    def poll_drop(cls, context, drag_data):
        """Returns True if the asset can be dropped into the current region context."""
        return context.space_data.type == 'NODE_EDITOR'

    def on_drop(cls, context, asset, drag_data):
        """Custom drop handler executed when asset is dropped into an editor."""
        print(f"Dropping custom asset: {asset.name}")
        # Execute associated Python script or instantiate custom datablock
        return {'FINISHED'}

    def draw_metadata(self, context, layout, asset_metadata):
        """Draw custom metadata fields in the Asset Browser inspection panel."""
        layout.prop(asset_metadata.properties, "[\"custom_version\"]", text="Version")
        layout.prop(asset_metadata.properties, "[\"author_email\"]", text="Email")

    def generate_preview(self, asset):
        """Optional callback to generate custom thumbnail previews for the asset."""
        ...

# Registration
def register():
    bpy.utils.register_class(CustomAssetType)

def unregister():
    bpy.utils.unregister_class(CustomAssetType)
```

### 5.2 Key Python Interfaces Exposed
1. `bpy.types.AssetType`: Base class for custom asset type registration.
2. `bpy.types.AssetMetaData.properties`: Pythonic access to custom asset `IDProperty` dictionary.
3. `bpy.types.AssetRepresentation.type_info`: Read-only property returning the associated `AssetType` instance.
