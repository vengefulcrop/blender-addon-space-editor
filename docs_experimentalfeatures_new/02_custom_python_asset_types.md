# Custom Python Container Asset Types

> **Goal.** Extensibility infrastructure letting add-ons declare their own *container* asset
> types — an asset that bundles several datablocks plus structured metadata into one unit the
> user manipulates as a single thing, the way `bpy.types.Node` lets add-ons declare node types.
>
> **Motivating use case.** Representing external DCC/engine formats as single units. An Unreal
> `StaticMesh` is one asset containing: several LOD meshes, a collision mesh, a material array,
> UV channel assignments, and an extensive metadata block. Blender has no convenient way to hold
> that as one asset today.
>
> **Status:** design proposal. Claims about existing code are verified against this tree with
> file:line citations. Sections marked **Proposed** describe code that does not exist yet.

---

## 1. Executive Summary

The good news: **Blender already contains almost every mechanism this feature needs**, but they
are spread across three unrelated subsystems and none is exposed to Python. Specifically:

1. **The container datablock already exists, with its whole pipeline.** `Collection` (`ID_GR`) is
   an asset-markable ID holding arbitrary objects and data. It already has viewport drop (with an
   instance-vs-localize option), rendered previews, and append-reuse deduplication on repeat
   drops. No DNA changes are needed for it to serve as the container.
2. **Asset-type dispatch by metadata, not by ID type, is already the established pattern.**
   Geometry node group assets are routed by an `IDProperty` named `"type"` on asset metadata —
   not by `ID_Type`. This is exactly the indirection a custom-type registry needs, and it is
   already load-bearing in shipping code.
3. **Dynamic type registration driven by asset metadata already ships.** Geometry **Node Tools**
   read a `"node_tool_idname"` string from asset metadata and register a real `wmOperatorType`
   from it at runtime, including a typed parameter schema and a context-declaration bitflag.
   This is a near-complete working prototype of the requested architecture, applied to operators.
4. **Context declaration already exists** as `GeometryNodeAssetTraitFlag`.
5. **Graceful degradation is structural, not a mechanism.** A container whose add-on is disabled
   is simply a `Collection` — it still opens, still drops, still saves.

What is genuinely missing is: a Python-facing registry, and per-editor drop dispatch that
consults it. That is a real but *bounded* C change — and critically, it is a **one-time** change,
after which new container types are pure Python.

The earlier draft of this document was wrong in both directions: it overstated how easy this is
(claiming three files), and it understated it by proposing to invent machinery that already exists.

---

## 2. Verified Foundations

### 2.1 Collections are already valid assets

`id_type_is_supported()`
([asset_type.cc:23](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_type.cc#L23)):

```cpp
return ELEM(GS(id->name), ID_BR, ID_MA, ID_GR, ID_OB, ID_AC, ID_WO, ID_NT, ID_SCE);
```

`ID_GR` is `Collection`. It is supported today, with no experimental flag required.

The underlying gate, `BKE_id_can_be_asset()`
([lib_id.cc:2679-2683](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/lib_id.cc#L2679-L2683)),
is `ID_IS_EDITABLE(id) && !ID_IS_OVERRIDE_LIBRARY(id) && BKE_idtype_idcode_is_linkable(...)`.
**It never inspects `asset_type_info`.**

Crucially, **`AssetTypeInfo` is not required for an ID to be asset-markable.** Collection's
`IDTypeInfo` has `.asset_type_info = nullptr`
([collection.cc:426](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/collection.cc#L426)),
yet collections are fully functional assets. `AssetTypeInfo` only supplies three optional
lifecycle hooks — `pre_save_fn`, `on_mark_asset_fn`, `on_clear_asset_fn`
([BKE_asset.hh:30-42](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_asset.hh#L30-L42)) —
and only `ID_OB`, `ID_NT`, `ID_AC`, `ID_BR` populate it. (The field defaults to a sentinel
`InvalidPointer` that trips a validation assert if a type forgets to set it
([idtype.cc:73](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/idtype.cc#L73)),
so every type must say `nullptr` or a real pointer explicitly — but `nullptr` is a fully
supported answer.)

### 2.1b Collections already have the full asset pipeline

Verified, all via generic machinery with no Collection-specific special-casing:

- **Viewport drop already works.** Two dropboxes are registered in
  [view3d_dropboxes.cc](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_view3d/view3d_dropboxes.cc):
  `OBJECT_OT_collection_instance_add` for local IDs (lines 713-719) and
  `OBJECT_OT_collection_external_asset_drop` for library assets (lines 704-710).
  The external copy callback (lines 563-628) calls `WM_drag_asset_id_import(...)` then, unless
  `use_instance_collections` is set, `make_selected_objects_local(...)` (line 604) — i.e. **the
  drop operator already exposes an instance-vs-localize choice**, which is exactly the axis a
  StaticMesh container needs (instance the whole thing vs. unpack LOD0).
- **Previews already render.**
  [render_preview.cc:422-428](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/render/render_preview.cc#L422-L428)
  handles `ID_GR` by wrapping the collection in a throwaway instance-Empty
  (`OB_DUPLICOLLECTION`) and reusing the standard object preview path. No new preview work needed.
- **Repeat drops deduplicate.** Import defaults to `ASSET_IMPORT_APPEND_REUSE`
  ([asset_import.cc:38-49](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_import.cc#L38-L49)),
  which uses `BLO_LIBLINK_APPEND_LOCAL_ID_REUSE` and the generic `ID.library_weak_reference`
  mechanism. Collection opts in via `IDTYPE_FLAGS_APPEND_IS_REUSABLE`
  ([collection.cc:425](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/collection.cc#L425)),
  so dropping the same library asset twice reuses the local datablock instead of duplicating it.

**Consequence:** the earlier draft's framing of `AssetTypeInfo` as *the* asset type registry was
wrong. It is a small optional callback table, not a type system. Extending it is not the path.

There is also an existing experimental flag, `use_extended_asset_browser`
([asset_type.cc:32](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_type.cc#L32)),
which unlocks all technically-capable asset types — a natural place to gate this feature.

### 2.2 Asset type is *already* dispatched by IDProperty, not ID type

This is the single most important existing precedent. Node group assets are filtered and routed
by reading a metadata IDProperty:

```cpp
const IDProperty *tree_type = BKE_asset_metadata_idprop_find(metadata, "type");
if (!tree_type || IDP_int_get(tree_type) != NTREE_GEOMETRY) { return false; }
```

Verified call sites of `BKE_asset_metadata_idprop_find`
([BKE_asset.hh:77](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_asset.hh#L77),
impl [asset.cc:183](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset.cc#L183)):

| Site | Key read | Purpose |
|---|---|---|
| [view3d_dropboxes.cc:314](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_view3d/view3d_dropboxes.cc#L314) | `"type"` | gate geometry-nodes drop into viewport |
| [view3d_dropboxes.cc:134](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_view3d/view3d_dropboxes.cc#L134) | `"dimensions"` | drop-preview sizing from metadata |
| [space_node.cc:1005](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/space_node.cc#L1005) | `"type"` | gate node-group drop into node editor |
| [link_drag_search.cc:170,178](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/link_drag_search.cc#L170) | `"type"`, `"properties"` | link-drag search over assets |
| [add_menu_assets.cc:47](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/add_menu_assets.cc#L47) | `"type"` | asset entries in Add menu |
| [add_modifier_assets.cc:60,64](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/object/add_modifier_assets.cc#L60) | `"type"`, traits flag | modifier asset listing |
| [sequencer_add_modifier_assets.cc:52,56](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_sequencer/sequencer_add_modifier_assets.cc#L52) | `"type"`, traits flag | sequencer modifier assets |

Note `view3d_dropboxes.cc:134` in particular: it reads a **`"dimensions"` float array** straight
out of asset metadata to size the drop preview *without loading the asset*. That is precisely the
"rich metadata readable before instantiation" property a StaticMesh container needs (bounds, LOD
count, material slot count, triangle counts).

**Consequence:** a string type-idname in `AssetMetaData.properties` is not a workaround — it is
the idiom Blender already uses for exactly this purpose.

### 2.3 Node Tools: metadata-driven dynamic type registration already ships

[`node_group_operator.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/geometry/node_group_operator.cc)
is the closest existing analogue to the requested feature, and it should be the template.

`custom_idname_for_asset()` ([line 219-227](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/geometry/node_group_operator.cc#L219-L227))
reads a **string idname out of asset metadata**:

```cpp
const IDProperty *id_property = BKE_asset_metadata_idprop_find(&metadata, "node_tool_idname");
if (!id_property || id_property->type != IDP_STRING) { return std::nullopt; }
return IDP_string_get(id_property);
```

`OperatorTypeData::from_asset()` ([line 229-283](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/geometry/node_group_operator.cc#L229-L283))
then builds a full runtime type descriptor from metadata alone:

- validates the idname (`WM_operator_idname_ok_or_report`, line 205) and mangles it into a real
  operator idname (`WM_operator_bl_idname`, line 215) — **the same validation discipline
  `rna_Node_register_base` applies**;
- reads a **context/traits bitflag** (`"geometry_node_asset_traits_flag"`, line 247);
- reads a **typed parameter schema** from a nested `"properties"` → `"inputs"` IDProperty group,
  validating that each input declares a `"type"` (lines 257-277);
- takes a **weak reference** to the asset (`asset.make_weak_reference()`, line 255) so the
  descriptor works for external library assets without loading them;
- deep-copies the schema (`IDP_CopyProperty`, line 279) and hashes it (`ensure_hash`, line 281)
  for change detection.

There is a parallel `from_group()` (line 294) for the local, non-asset case — the same duality a
container type needs (asset in a library vs. local datablock in the current file).

**Consequence:** "read a type identifier and a typed schema from asset metadata, then register a
runtime type from it" is not speculative. It is shipping code with error reporting, validation,
weak references, and cache invalidation already worked out.

### 2.3b Node group assets are already "containers that collapse on drop"

The requested "present custom UX in context, collapse to essentials otherwise" behaviour has a
direct precedent. Dropping a node group asset into the node editor:

1. `node_group_drop_poll()`
   ([space_node.cc:974-1012](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/space_node.cc#L974-L1012))
   gates on the `"type"` metadata IDProperty vs. `snode->edittree->type` — **metadata-only, no
   datablock materialized**. This is the cheap pre-instantiation context check a container needs.
2. `node_group_drop_copy()` (lines 1105-1113) imports the tree via
   `WM_drag_get_local_ID_or_import_from_asset` and stashes its `session_uid` on the operator.
3. `add_node_group_asset()`
   ([node_add.cc:480-525](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/node_add.cc#L480-L525))
   creates **one** group node referencing the imported tree. The inner nodes are never unpacked.

That is precisely the container semantic: an asset holding many things, which on drop becomes a
single referencing unit in the target editor, with the internals addressable but collapsed.

### 2.4 Context declaration already exists

`GeometryNodeAssetTraitFlag`
([DNA_node_types.h:317-326](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_node_types.h#L317-L326)):

```c
GEO_NODE_ASSET_TOOL          = (1 << 0),
GEO_NODE_ASSET_EDIT          = (1 << 1),
GEO_NODE_ASSET_SCULPT        = (1 << 2),
GEO_NODE_ASSET_MESH          = (1 << 3),
GEO_NODE_ASSET_CURVE         = (1 << 4),
GEO_NODE_ASSET_MODIFIER      = (1 << 6),
GEO_NODE_ASSET_OBJECT        = (1 << 7),
GEO_NODE_ASSET_WAIT_FOR_CURSOR = (1 << 8),
GEO_NODE_ASSET_GREASE_PENCIL = (1 << 9),
```

A bitflag stored in asset metadata declaring which modes and geometry types the asset applies to,
consulted via `asset_flag_for_context(const Object &active_object)`
([node_group_operator.cc:321](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/geometry/node_group_operator.cc#L321)).

This is directly the "declare its intended context — mesh editor, object editor" requirement.
The container design should generalise this rather than invent a parallel concept.

### 2.5 Drop dispatch: one poll choke point, per-space registration

- **All** drags funnel through `dropbox_active()`
  ([wm_dragdrop.cc:513-558](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/windowmanager/intern/wm_dragdrop.cc#L513-L558)),
  called from `wm_dropbox_active()` (lines 598-633) and `wm_drop_update_active()` (lines 638-678).
  It walks region → area → window handlers and returns the first dropbox whose `poll` succeeds.
- But **registration** is inherently per-`(spaceid, regionid)`: `WM_dropboxmap_find()`
  ([wm_dragdrop.cc:112-129](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/windowmanager/intern/wm_dragdrop.cc#L112-L129))
  keys a map by that pair, and each space registers its own list. There is no global list. 78
  `WM_dropbox_add`/`WM_dropboxmap_find` call sites across 20 files.
- **The cheap lever:** `WM_dropbox_add()` takes an *operator idname string*
  ([wm_dragdrop.cc:131-142](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/windowmanager/intern/wm_dragdrop.cc#L131-L142))
  resolved via `WM_operatortype_find()` — which searches the same registry Python operators
  populate. **A Python `bpy.types.Operator` is already a valid drop target.** No C-to-Python
  callback marshalling layer is required.
- No existing Python drop exposure: `rna_wm.cc` has zero drop-related RNA.

**Consequence:** the drop work is "register one generic dispatching dropbox per editor, once" —
not "add a callback per asset type per editor". After that one-time change, add-ons supply an
operator idname and never touch C.

### 2.6 Metadata persistence

`AssetMetaData::properties` round-trips through `.blend` files: `BKE_asset_metadata_write()`
([asset.cc:201-217](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset.cc#L201-L217))
calls `IDP_BlendWrite`; read side at
[readfile.cc:2315](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenloader/intern/readfile.cc#L2315).
IDProperties are schemaless, so unknown keys survive save/load untouched — this is what makes
graceful degradation work (§4).

`local_type_info` is a runtime pointer populated at read time from the static per-ID-type table
([readfile.cc:2320](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenloader/intern/readfile.cc#L2320),
[BKE_idtype.hh:202](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_idtype.hh#L202));
it is not serialized and is not a viable home for a custom type.

---

## 3. Proposed Architecture

### 3.1 The container is a Collection

```
MyAddon StaticMesh container asset  ==  Collection (ID_GR), asset-marked
  |
  +-- Objects: LOD0, LOD1, LOD2, UCX_collision      <- ordinary collection members
  +-- Material slots on those objects               <- ordinary ID references
  +-- AssetMetaData.properties (IDProperty group):
        "asset_type"          = "MYADDON_static_mesh"   <- type idname (string)
        "asset_type_context"  = <bitflag>               <- intended contexts
        "schema"              = { ... typed fields ... } <- addon's metadata
        "dimensions"          = [x, y, z]               <- reuses existing key
```

Everything the container needs already works: dependency tracking, linking/appending, undo,
`.blend` serialization, library overrides, and the Asset Browser listing — because it is just a
Collection.

### 3.2 Registration flow (modelled on `bke::node_register_type`)

```
Python class subclassing bpy.types.AssetContainerType
                  |
                  v  bpy.utils.register_class
  rna_AssetContainerType_register()        <-- NEW: RNA_def_struct_register_funcs in rna_asset.cc
                  |                             validates bl_idname (cf. rna_Node_register_base)
                  v
  Global registry: Map<StringRef, AssetContainerType *>   <-- NEW, mirrors node type registry
                  |
     +------------+------------------+
     v                               v
  Asset Browser filtering        Generic drop dispatcher (one per editor)
  (reads "asset_type" idprop)    reads "asset_type" -> registry -> operator idname
                                 -> invokes the addon's own bpy.types.Operator
```

The registry is keyed by the **string idname**, matched against the `"asset_type"` IDProperty on
the asset — exactly the §2.2 idiom, and exactly the §2.3 Node Tools lookup.

### 3.3 Proposed C++ type descriptor

New struct in `blenkernel` (**not** in `AssetTypeInfo`, which is an unrelated lifecycle-callback
table per §2.1, and **not** in DNA — this is runtime-only):

```cpp
/* BKE_asset_container_type.hh (new) */
struct AssetContainerType {
  std::string idname;          /* matched against the "asset_type" IDProperty */
  std::string ui_name;
  std::string description;
  int ui_icon;

  /* Which ID types may serve as the container. Default: ID_GR only. */
  uint64_t container_id_types;

  /* Intended contexts; generalises GeometryNodeAssetTraitFlag (§2.4). */
  AssetContextFlag context_flag;

  /* Per-editor drop targets: space type -> Python operator idname.
   * Resolved via WM_operatortype_find, so plain bpy.types.Operator works (§2.5). */
  Map<int /*eSpace_Type*/, std::string /*op idname*/> drop_operators;

  ExtensionRNA rna_ext;        /* mirrors bNodeType::rna_ext */
};

void asset_container_type_register(AssetContainerType *type);
void asset_container_type_unregister(AssetContainerType *type);
const AssetContainerType *asset_container_type_find(StringRef idname);
```

### 3.4 Python API (Proposed)

```python
import bpy

class UnrealStaticMesh(bpy.types.AssetContainerType):
    bl_idname = "MYADDON_static_mesh"
    bl_label = "Unreal Static Mesh"
    bl_description = "LODs, collision, materials and metadata as one asset"
    bl_icon = 'MESH_DATA'

    # Which contexts this container is meaningful in (§2.4 generalised).
    bl_contexts = {'OBJECT', 'EDIT_MESH'}

    # Which datablock acts as the container. Collection is the default.
    bl_container_type = 'COLLECTION'

    # Declarative metadata schema -> stored in AssetMetaData.properties.
    lod_count:      bpy.props.IntProperty(name="LOD Count", min=1, default=1)
    collision_type: bpy.props.EnumProperty(
        name="Collision",
        items=[('NONE', "None", ""), ('UCX', "Convex", ""), ('BOX', "Box", "")],
    )
    lightmap_res:   bpy.props.IntProperty(name="Lightmap Resolution", default=64)

    # Per-editor drop behaviour: map space type -> your own operator idname.
    bl_drop_operators = {
        'VIEW_3D':  "myaddon.staticmesh_drop_viewport",
        'NODE_EDITOR': "myaddon.staticmesh_drop_nodes",
    }

    @classmethod
    def poll_drop(cls, context, asset):
        """Optional refinement beyond bl_contexts / bl_drop_operators."""
        return context.mode == 'OBJECT'

    def draw_info(self, context, layout, asset):
        """Optional: extra rows in the Asset Browser sidebar."""
        layout.prop(self, "lod_count")
        layout.prop(self, "collision_type")


class MYADDON_OT_staticmesh_drop_viewport(bpy.types.Operator):
    """Ordinary operator — this is what actually runs on drop."""
    bl_idname = "myaddon.staticmesh_drop_viewport"
    bl_label = "Drop Static Mesh"

    def execute(self, context):
        # Instantiate LOD0 only, parent collision as a child, apply materials.
        return {'FINISHED'}
```

Design notes:

- `poll_drop` is a **classmethod** — there is no per-asset Python instance, matching `Panel.poll`
  and `Node.poll`.
- Drop *behaviour* is a normal operator, not a callback. This is the §2.5 finding: it is both
  cheaper to implement and better for users (drops become undoable, redo-panel-capable,
  scriptable, and keymappable for free).
- No `generate_preview`. Previews go through `ED_preview_icon_render()`
  ([render_preview.cc:2201](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/render/render_preview.cc#L2201)),
  a background-job pipeline with offscreen GPU context handoff across threads — unsafe for
  arbitrary Python. Collections already receive normal preview rendering; an add-on that wants a
  bespoke thumbnail can set it explicitly from the main thread via existing preview APIs.

---

## 4. Graceful Degradation ("collapse to bare essentials")

This requirement is satisfied **structurally**, with no fallback mechanism to build — which is a
strictly better outcome than the node analogy that motivated it.

| Situation | Node type | Container asset type |
|---|---|---|
| Add-on enabled | Full custom UX | Full custom UX + custom drop operator |
| Add-on disabled | Undefined-node placeholder; data preserved but node is non-functional | **Still a working Collection.** Drops as a normal collection instance. Renders. Saves. |
| Re-enabled | Self-heals losslessly | Self-heals losslessly |
| Opened in older Blender | Node type unknown | **Fully functional Collection** — no version gate |

**The node self-healing precedent, verified.** When an add-on is unregistered,
`update_typeinfo()`
([node.cc:2701-2736](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/node.cc#L2701-L2736))
sets `node->typeinfo = &NodeTypeUndefined`
([node.cc:2671](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/node.cc#L2671))
but **never touches `node->idname` or `node->storage`**. On file load, storage is read untyped
when the type is unknown — `node_blend_read_data_storage()`
([node.cc:1972-1994](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/node.cc#L1972-L1994)):

```cpp
if (ntype && !ntype->storagename.empty()) { /* typed read */ }
else { /* Untyped read because we don't know the type yet. */
  BLO_read_raw_address(reader, &node->storage); }
```

`node_tree_set_type()` (lines 2738-2754) then re-resolves by the preserved idname, so
re-registering the add-on restores everything. Note the one case where the idname *is* mangled to
`"Undefined[<old>]"` — `node_set_undefined_type()` (lines 5665-5673) — applies only to
permanently-unreadable built-in types from newer files, not to the add-on-disabled case.

The container design gets the same property for free and more robustly: type identity lives in
schemaless IDProperties, and `IDP_BlendWrite`/`IDP_BlendDataRead` perform no schema or whitelist
validation whatsoever (only a depth guard), so unknown keys round-trip byte-for-byte
([asset.cc:205-206, 223-226](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset.cc#L205-L206)).
And because the payload is a Collection, the degraded state is not a red placeholder — it is the
actual usable geometry.

For the StaticMesh case specifically: without the add-on, the user drops it and gets a collection
instance containing all LODs and the collision mesh. Slightly cluttered, entirely functional. With
the add-on, they get LOD0 instanced and the rest wired up correctly.

---

## 5. Current Python Exposure Gaps

This section documents precisely what is *not* reachable from Python today. These are the
limitations the feature exists to remove.

### 5.1 No way to register an asset type from Python

- `rna_asset.cc` contains **no** `RNA_def_struct_register_funcs` call.
- The string `AssetType` does not appear anywhere in `source/blender/makesrna/`.
- Therefore there is no `bpy.types.AssetType` / `AssetContainerType` to subclass, and
  `bpy.utils.register_class` has nothing to hook.

This is the root gap; 5.2-5.4 are consequences of it.

### 5.2 No registry to register into

The node system's extensibility rests on a global runtime registry — `bke::node_register_type()`
([rna_nodetree.cc:2185](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_nodetree.cc#L2185))
— that every consumer (drawing, evaluation, IO) looks types up in. The asset system has no
equivalent. The nearest-looking thing, `AssetTypeInfo`, is **not** a registry:

- It is a compile-time table with exactly four populated entries (`ID_OB`, `ID_NT`, `ID_AC`,
  `ID_BR`); every other ID type sets `nullptr`.
- It holds three lifecycle callbacks only — `pre_save_fn`, `on_mark_asset_fn`,
  `on_clear_asset_fn` ([BKE_asset.hh:30-42](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_asset.hh#L30-L42)).
- It is reached through `IDTypeInfo`, i.e. keyed by `ID_Type`, so it structurally cannot express
  "several distinct asset types sharing one ID type".

### 5.3 Nothing about drag & drop is exposed to Python

- `rna_wm.cc` has **zero** drop-related RNA. No `bpy.types.DropBox`, no Python drop poll, no way
  to register a dropbox.
- Every drop poll is a C function testing `ID_Type` directly (e.g.
  `view3d_drop_id_in_main_region_poll_get_id_type`,
  [view3d_dropboxes.cc:57-92](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_view3d/view3d_dropboxes.cc#L57-L92)).
- Registration is per-`(spaceid, regionid)` via `WM_dropboxmap_find`
  ([wm_dragdrop.cc:112-129](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/windowmanager/intern/wm_dragdrop.cc#L112-L129));
  there is no global dropbox list to append to.

**Mitigating factor:** `WM_dropbox_add` resolves an *operator idname* through
`WM_operatortype_find` ([wm_dragdrop.cc:131-142](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/windowmanager/intern/wm_dragdrop.cc#L131-L142)),
which already searches the registry Python operators populate. The *destination* of a drop is
therefore already Python-writable; only the *wiring* is missing.

### 5.4 No sanctioned convention for the type-tag metadata

`AssetMetaData.properties` is readable and writable from Python as an IDProperty dict, and C code
already reads type tags out of it (§2.2). But there is no reserved-key policy, no collision
protection, and no API for claiming a type identifier. Add-ons doing this today are writing
arbitrary keys into a shared namespace.

### 5.5 No per-type UI dispatch

`Node` gets `draw_buttons` bound to `rna_Node_draw_buttons`
([rna_nodetree.cc:2147-2156](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_nodetree.cc#L2147-L2156));
`Panel` gets an equivalent through `rna_Panel_register`. Assets have no such dispatcher — there is
nowhere for a per-asset-type `draw()` to be called from.

**Mitigating factor:** the Asset Browser metadata sidebar is already pure Python
([space_filebrowser.py:778-899](file:///d:/gamedev/_blender_fork/latest_pyareas/scripts/startup/bl_ui/space_filebrowser.py#L778-L899)),
so add-ons can extend *that* surface today with an ordinary `Panel` and a `poll()`. What's missing
is type-bound UI that follows the asset into other editors.

### 5.6 What *is* already exposed

For completeness, so none of this gets rebuilt unnecessarily:

| Capability | Status |
|---|---|
| Collections as assets, with drop/preview/dedup | Works today (§2.1, §2.1b) |
| `asset.metadata.properties` schemaless IDProperties, saved/loaded | Works today (§2.6) |
| Asset Browser metadata sidebar extension | Works today, plain `Panel` |
| Operators as drop destinations | Writable today, not wirable |
| Marking/clearing assets from Python | Works today (`id.asset_mark()`) |

---

## 6. Custom Per-Type UI

**Target: parity with custom *node types* (`draw_buttons`), not with user-made node groups.** The
type itself draws arbitrary widgets — multi-column LOD tables, per-material rows, live
triangle-count readouts — not just auto-generated property rows.

### 6.1 The asymmetry that has to be resolved first

A custom node has one unambiguous UI surface: its body on the editor canvas, drawn by
`draw_buttons`. An asset has no single equivalent, because an asset exists in two distinct states
with different available data:

| | Node | Container asset |
|---|---|---|
| **Persistent visual body** | Yes — the node on canvas | No |
| **Before instantiation** | n/a | `AssetRepresentation` — may be an external library file with **no local `ID`**, only metadata |
| **After instantiation** | n/a | A real datablock in the scene (a Collection instance object) |

So "custom node parity" splits into two problems, and they have very different costs.

### 6.2 Post-drop UI: already fully possible in Python today

Once a container is dropped it is a real datablock — a Collection instance object. Custom UI for it
is an ordinary `bpy.types.Panel` whose `poll()` recognises the container type:

```python
class MYADDON_PT_static_mesh(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"
    bl_label = "Static Mesh"

    @classmethod
    def poll(cls, context):
        ob = context.object
        coll = ob and ob.instance_collection
        ad = coll and coll.asset_data
        return bool(ad and ad.get("asset_container_type") == "MYADDON_static_mesh")

    def draw(self, context):
        # Arbitrary widgets: LOD table, collision controls, material rows.
        ...
```

**This needs no new infrastructure whatsoever.** Full custom widgets, full `uiLayout` access, in
the Properties editor and the 3D viewport N-panel. It is genuine `draw_buttons`-equivalent power
for the instantiated container.

What the registry adds here is **ergonomics, not capability**: the type declares its UI once
instead of each add-on hand-writing a `poll()` that re-derives the type tag. Worth doing, but it
should not be on the critical path — and it means you can prototype the entire post-drop UX
*today*, before any C lands.

### 6.3 Pre-drop UI: the part that genuinely needs new code

Drawing type-specific UI for an asset that has **not** been instantiated — and may live in an
external library with no local `ID` — is the real gap. Only `AssetMetaData` is available (which is
precisely why §2.2's metadata-only dispatch matters, and why `view3d_dropboxes.cc:134` reads
`"dimensions"` from metadata rather than loading the asset).

This is the `Node.draw_buttons` pattern proper: bind a `draw` function pointer in the registered
type to a C-to-Python dispatcher, exactly as `rna_Node_register_base` does at
[rna_nodetree.cc:2147-2156](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_nodetree.cc#L2147-L2156).
Mechanical and well-precedented, but genuinely new code.

The design decision it forces is **which call site**, since four exist with different context
availability:

| Call site | Context available | Recommendation |
|---|---|---|
| Asset Browser sidebar | Full `bContext`, active asset | **Do this first.** Already Python panels, lowest risk |
| Drop redo panel | Full context, post-drop | Falls out of §6.2 — operator properties draw themselves |
| Tooltip | **Restricted** — no operator execution, transient layout | Defer; constrained and easy to get wrong |
| Asset shelf | Full context, but per-region state | Defer to a later phase |

Recommend: sidebar in Phase 4a, asset shelf in 4b, tooltips last or never.

### 6.4 Consequence for the plan

Because §6.2 is free, the honest sequencing is: **prototype the whole post-drop UX in Python now**,
and treat the C dispatcher (§6.3) as buying pre-instantiation UI specifically — a real capability
gain, but a narrower one than "custom UI" implies at first glance. That also de-risks the proposal:
you can demonstrate a working StaticMesh container with full custom widgets before writing any C.

---

## 7. Making All Datablocks Asset-able

### 7.1 The current gate is a curated allowlist, not a technical limit

`id_type_is_supported()`
([asset_type.cc:26-39](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_type.cc#L26-L39)):

```cpp
if (!BKE_id_can_be_asset(id)) { return false; }
if (USER_EXPERIMENTAL_TEST(&U, use_extended_asset_browser)) {
  /* The "Extended Asset Browser" experimental feature flag enables all asset types that can
   * technically be assets. */
  return true;
}
return id_type_is_non_experimental(id);
```

with

```cpp
bool id_type_is_non_experimental(const ID *id)
{
  /* Remember to update #ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_FLAGS() and the messages in
   * asset_operation_unsupported_type_msg with this! */
  return ELEM(GS(id->name), ID_BR, ID_MA, ID_GR, ID_OB, ID_AC, ID_WO, ID_NT, ID_SCE);
}
```

**Images are already technically assets.** `Image` passes `BKE_id_can_be_asset()` (editable,
not a liboverride, linkable). Enabling *Preferences → Experimental → Extended Asset Browser*
makes Images — and every other linkable datablock — markable and browsable **today, with no code
changes**. The filter list widens to `FILTER_ID_ALL` in the same flag check (line 43-45).

### 7.2 So why hasn't it shipped? Evidence, not speculation

The code carries its own history. The experimental flag's description
([rna_userdef.cc:7703-7706](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_userdef.cc#L7703-L7706)):

> "Enable Asset Browser editor and operators to manage regular data-blocks as assets,
> **not just poses**"

The asset system shipped for the Pose Library first. The non-experimental allowlist is the set of
types whose *end-to-end UX* was subsequently completed, added one at a time. Supporting evidence:

- The flag lives in the block commented "automatically sanitized (set to 0) when the release cycle
  is not alpha" ([DNA_userdef_types.h:883-886](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_userdef_types.h#L883-L886))
  — alpha-only, i.e. an unfinished-work gate, not a permanent opt-in.
- It links to a *project* page, not a bug: `blender/blender/projects/10`, "Pipeline, Assets & IO
  Project Page" ([space_userpref.py:3006-3007](file:///d:/gamedev/_blender_fork/latest_pyareas/scripts/startup/bl_ui/space_userpref.py#L3006-L3007)).
  Tracked, intentional, incremental.
- The allowlist's comment requires manually syncing two other places (a filter-flags macro and
  user-facing error strings) — friction that makes each addition a deliberate act.

**Verdict: neither functional nor philosophical. It is incremental productization.** Storage,
serialization, previews and browsing are type-generic and already work. What is missing per type
is the *semantics*: what does dropping an Image into the 3D viewport mean — a reference plane, an
empty image, a texture on the active material, a world background? Blender has not committed to an
answer, so the type stays behind the flag. Nothing prevents it technically.

### 7.3 Datablock level or container level?

**Both, for different reasons — and they are not in competition.**

Do it at the **datablock level** when the type has one obvious meaning. For Image that is arguably
true, and the change is literally adding `ID_IM` to the allowlist in
[asset_type.cc:23](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_type.cc#L23)
plus the two synced locations. The cost is not implementation; it is committing to drop semantics
for every editor.

Do it at the **container level** when the unit is inherently compound, which is the StaticMesh
case. An Unreal StaticMesh is not "a mesh with extra fields" — it is LODs plus collision plus
materials plus metadata, and no single existing datablock is the right home. Collection is.

**The important observation:** §7.2 identifies the blocker for extended asset support as
"nobody has decided what dropping type X should do." That is *exactly* what this proposal lets
add-ons decide, per type, in Python. A working custom asset type system is therefore not a
competitor to widening the allowlist — it is plausibly the mechanism that makes widening it
tractable, by moving the per-type semantics decision out of Blender's core and into the add-on
that actually knows the answer.

This is worth stating explicitly in any upstream proposal: it reframes the feature from "add-on
convenience" to "unblock a stalled core project."

---

## 8. Editing Assets In Place and Writing Back to the Library

### 8.1 The mechanism already exists and is generic

[`BKE_asset_edit.hh`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/BKE_asset_edit.hh)
+ [`asset_edit.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset_edit.cc)
implement exactly the round-trip in question. From the header's own description (lines 11-25):

> Asset blend files are linked into the global main database, with the asset datablock itself and
> its dependencies. These datablocks remain linked but are **marked as editable**. User edited
> asset datablocks are written to individual blend files per asset. […] **This mechanism is
> currently only used for brush assets.**

The full API is already there and is not brush-specific in its signatures:

```cpp
ID  *asset_edit_id_from_weak_reference(Main &, ID_Type, const AssetWeakReference &);
bool asset_edit_id_is_editable(const ID &);
bool asset_edit_id_is_writable(const ID &);
std::optional<std::string> asset_edit_id_save_as(Main &, const ID &, StringRefNull name,
                                                 const bUserAssetLibrary &,
                                                 AssetWeakReference &r_weak_ref, ReportList &);
bool asset_edit_id_save(Main &, const ID &, ReportList &);
ID  *asset_edit_id_revert(Main &, ID &, ReportList &);
bool asset_edit_id_delete(Main &, ID &, ReportList &);
ID  *asset_edit_id_find_local(Main &, ID &);
ID  *asset_edit_id_ensure_local(Main &, ID &);
```

Editability is gated by `ID_IS_EDITABLE`
([DNA_ID.h:693-696](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_ID.h#L693-L696)),
which requires the library be tagged `LIBRARY_ASSET_EDITABLE` **and** the type pass:

```c
#define ID_TYPE_SUPPORTS_ASSET_EDITABLE(id_type) \
  ELEM(id_type, ID_BR, ID_TE, ID_NT, ID_IM, ID_PC, ID_MA)
```
([DNA_ID.h:690-691](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_ID.h#L690-L691))

### 8.2 Cost for the node group case: low — it is already supported

**`ID_NT` is in that list.** A node group asset is already an asset-editable type. The
infrastructure to edit a library node group's interface values from the Asset Browser and write
them back to its `.blend` exists today; what is missing is UI wiring, not kernel work. The brush
path shows the shape of it — `brush_asset_edit_metadata_exec` calls `bke::asset_edit_id_save`
([brush_asset_ops.cc:406](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/sculpt_paint/brush_asset_ops.cc#L406)),
and `bpy.ops.brush.asset_save_as` is already reachable from Python
([brush_asset_test.py:61](file:///d:/gamedev/_blender_fork/latest_pyareas/tests/python/sculpt_paint/brush_asset_test.py#L61)).

`ID_IM` and `ID_MA` being in the list is also notable: it partly answers §7 — Image is not only
technically asset-able, it is already asset-*editable*.

### 8.3 Cost for a Collection-backed container: blocked, with a stated reason

**Scope note.** "Collection container" here means the §3 design choice of `Collection` (`ID_GR`) as
the datablock holding the bundle. What follows blocks exactly **one** capability — the in-place
round-trip, i.e. editing an asset that lives in an external library and having the change written
back into that library's `.blend` without opening it. It does **not** affect any of:

| Capability | Status with a Collection container |
|---|---|
| Collection as the container datablock | Works |
| Marking, browsing, filtering as an asset | Works |
| Drop, preview, append-reuse dedup | Works (§2.1b) |
| Authoring a container and saving it into a library | Works |
| Custom UI after drop | Works, pure Python (§6.2) |
| **In-place edit written back to the library** | **Blocked** |

The v1 story with a Collection container is therefore "to edit a container, open the file it lives
in" — which is how most asset workflows already behave. §3 is not invalidated.

**The blocker.** `ID_GR` is not in `ID_TYPE_SUPPORTS_ASSET_EDITABLE`, and `ID_ME` is not either.
The codebase states why, in `blendfile.cc:1019-1021`:

> "Keep linked brush asset data, similar to UI data. Only does a known subset now. **Could do
> everything, but that risks dragging along more scene data than we want.**"

That is a genuine functional objection, not caution for its own sake. The types on the list are
all leaf-ish: brushes, textures, node trees, images, materials, paint curves. A Collection is the
opposite — it transitively owns Objects, which own Meshes, modifiers, and further references.
Making Collections asset-editable means the "link the asset and its dependencies into global Main"
step pulls in an unbounded subgraph. `asset_edit_id_ensure_local()` already has to defensively
clear dependencies that fail the type check
([asset_edit.cc:486-513](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset_edit.cc#L486-L513)) —
for a Collection that clearing would gut the asset.

**Metadata is not a loophole.** For an external asset, `AssetRepresentation` holds its own
`AssetMetaData` copy inside the `ExternalAsset` struct
([AS_asset_representation.hh:54-80](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/asset_system/AS_asset_representation.hh#L54-L80));
changing it in memory does not touch the library file. Metadata lives inside the `.blend` — unlike
catalogs, which have a sidecar (`blender_assets.cats.txt`). So **editing an external container's
metadata hits the same wall as editing its contents**, and that is exactly the surface §6.3's
pre-drop UI operates on. Any "tweak values in the Asset Browser and have it stick" feature depends
on this section, not just on §6.

### 8.3b Choosing the container substrate

Editability is therefore a real axis in the §3 choice of container datablock, and the original
design picked `Collection` on drop/preview/dedup merit alone without weighing it. The options:

| Option | In-place editable | Natural bundle of datablocks | Notes |
|---|---|---|---|
| **A. `Collection`** | No | **Yes** — objects, meshes, materials as members | §3 as written. Best fit for a StaticMesh's shape |
| **B. `NodeTree`** | **Yes** (`ID_NT` passes the gate) | No — payload must be procedural or referenced | Gains parameters + in-place editing, loses the bag-of-datablocks model |
| **C. `Collection`, accept file-open editing** | No | Yes | A is this, stated honestly. Recommended for v1 |
| **D. Add `ID_GR` to the macro** | Yes | Yes | One-line change; compiles; then drags an unbounded subgraph. Not recommended without a dependency-scoping design |

**The tempting hybrid does not work.** "A `NodeTree` container that references a Collection holding
the real payload" appears to get both — in-browser parameter editing plus a datablock bundle. It
fails at the same place: `asset_edit_id_ensure_local()` walks dependencies and **clears** any that
fail `ID_TYPE_SUPPORTS_ASSET_EDITABLE`
([asset_edit.cc:486-513](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/asset_edit.cc#L486-L513)).
`ID_GR` fails it, so the Collection reference would be stripped during localize — the container
would be edited into an empty shell. This is worth recording explicitly because it is a natural
design to reach for and its failure mode is silent data loss rather than a compile error.

**Recommendation:** ship v1 as option C. Revisit B only if in-browser parameter editing turns out
to matter more than the bundle semantics, and treat D as requiring a real dependency-scoping
proposal (see §10).

### 8.4 Override-assets: the right idea, but do not build it on library overrides

The "a config for an existing asset, exposed as its own asset" concept is sound and cheap — but
**library overrides are explicitly forbidden from being assets**. `BKE_id_can_be_asset()`
([lib_id.cc:2679-2683](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/blenkernel/intern/lib_id.cc#L2679-L2683)):

```cpp
return ID_IS_EDITABLE(id) && !ID_IS_OVERRIDE_LIBRARY(id) && BKE_idtype_idcode_is_linkable(...);
```

So a literal "liboverride marked as asset" is a dead end at the first gate.

**The better construction needs no new datablock at all.** Blender already has the exact primitive:
`AssetWeakReference`
([DNA_asset_types.h:197-224](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_asset_types.h#L197-L224)),
a serializable pointer-to-an-asset-by-identity, **already exposed to RNA**
([rna_asset.cc:890](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_asset.cc#L890)),
with `make_reference()` and full comparison operators. It is what brushes use to remember which
library asset they came from, and what `asset_edit_id_from_weak_reference()` resolves.

An override-asset is therefore just a container whose metadata is:

```
"asset_container_type" = "MYADDON_static_mesh_variant"
"base_asset"           = <AssetWeakReference to the base asset>
"overrides"            = { ...IDProperty group of changed values... }
```

This is pure metadata — no ID payload, no liboverride machinery, no new DNA. It resolves the base
via the existing weak-reference path and applies the override group on drop. It is arguably the
**cheapest** thing in this entire proposal, and it composes: a variant of a variant is the same
structure one level up.

Caveats to design for: cycle detection on `base_asset` chains, behaviour when the base asset is
missing or has changed schema, and whether overrides are applied at drop time only or maintained
as a live link (recommend drop-time only for v1 — a live link re-introduces the dependency
management that §8.3 warns about).

---

## 9. Implementation Plan

### Phase 1 — Registry and Python type (no behaviour change)
- `source/blender/blenkernel/BKE_asset_container_type.hh` + `intern/asset_container_type.cc` — new
  registry, modelled on the node type registry (`bke::node_register_type`, §1).
- `source/blender/makesrna/intern/rna_asset.cc` — `AssetContainerType` RNA struct +
  `RNA_def_struct_register_funcs`. There is currently **no** `RNA_def_struct_register_funcs` in
  this file and no `AssetType` string anywhere in `makesrna`; this is greenfield.
- Idname validation should follow the `rna_Node_register_base` / `WM_operator_idname_ok_or_report`
  precedent, including duplicate-idname rejection.

### Phase 2 — Marking and metadata
- `source/blender/editors/asset/` — operator to stamp `"asset_type"` (and the schema group) onto
  `AssetMetaData.properties` when marking a Collection as a typed container.
- `scripts/startup/bl_ui/space_filebrowser.py` — filter/display by container type. Note the
  metadata sidebar is already pure Python
  ([space_filebrowser.py:778-899](file:///d:/gamedev/_blender_fork/latest_pyareas/scripts/startup/bl_ui/space_filebrowser.py#L778-L899)),
  so add-ons can already extend it with an ordinary `Panel` today, independent of this feature.

### Phase 3 — Drop dispatch (the bulk of the C work)
One generic registry-consulting dropbox per editor, added at each editor's existing
`WM_dropbox_add` site:
- `view3d_dropboxes.cc` (11 existing call sites), `space_node.cc` (12), `sequencer_drag_drop.cc`
  (8), `outliner_dragdrop.cc` (7), `space_text.cc` (5), `space_console.cc` (5), `space_clip.cc`
  (3), `space_file.cc` (2), `space_image.cc` (1).
- `wm_dragdrop.cc` — carry the resolved container type on `wmDragAsset`.
- `interface_drag.cc` — populate it at drag start (note: this file handles drag *start* only,
  lines 19-104; it is not the drop path).

Realistically, ship Phase 3 for `VIEW_3D` first and add editors incrementally — the registry
design does not require them to land together.

### Phase 0 — Python-only prototype (do this before any C)
Post-drop custom UI needs no new infrastructure (§6.2). Build the StaticMesh container end to end
with a hand-marked Collection, a type tag written into `asset_data`, a `Panel` with a `poll()`, and
an ordinary drop operator invoked manually. This validates the whole UX and de-risks everything
below.

### Phase 4 — Pre-instantiation custom UI (§6.3)
Bind a `draw` function pointer to a C-to-Python dispatcher following `rna_Node_draw_buttons`
([rna_nodetree.cc:2147-2156](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_nodetree.cc#L2147-L2156)).
- **4a:** Asset Browser sidebar.
- **4b:** Asset shelf.
- **Deferred:** tooltips — restricted context, no operator execution.

Post-drop UI (§6.2) needs nothing here beyond optionally letting the type declare its panels
instead of each add-on writing its own `poll()`.

### Not required
- **No DNA changes.** Type identity lives in IDProperties; the container is an existing ID type.
- **No new `ID_Type`.** Rejected: it would touch `blenkernel`, `makesdna`, `makesrna`,
  `blenloader`, `depsgraph` and undo, and files saved with a new ID code fail to open in older
  Blender — for no capability gain over a Collection.
- **No changes to `AssetTypeInfo`.** It is an unrelated three-callback lifecycle table (§2.1).

---

## 10. Open Questions

1. **Reserved IDProperty key naming.** `"type"` is already taken by node trees (§2.2). Suggest
   `"asset_container_type"` to avoid collision, and a documented reservation policy.
2. **Registry key.** Idname alone, or `(idname, container_id_type)`?
3. **Nested containers.** Collections nest natively — should a container be allowed to contain
   another typed container, and does drop dispatch recurse?
4. **Schema versioning.** When an add-on updates its schema, how are existing assets migrated?
   Node Tools sidestep this via `ensure_hash()` change detection
   ([node_group_operator.cc:281](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/geometry/node_group_operator.cc#L281));
   a container equivalent needs deciding.
5. **Should this generalise Node Tools rather than parallel it?** §2.3 is 80% of this feature
   already. A unified "asset-metadata-declared runtime type" layer serving both would be more
   defensible upstream than a second, similar-but-separate system.
6. **Dependency scoping for editable container types (§8.3b option D).** Making `ID_GR`
   asset-editable requires bounding what gets linked into global Main. Is there a defensible rule —
   "the collection's direct members and their data, but not scene/world/view-layer references" —
   that makes this safe? Without one, option D stays off the table.

   *External precedent (not sourced from this codebase — general engine-architecture knowledge,
   not file-verified).* Unreal Engine does not solve this scoping problem; it avoids needing to.
   Its assets are not a nested-ownership graph the way Blender's ID system is. A `StaticMesh`
   embeds its own LOD/collision data in one package, but its material slots hold a **reference by
   path** to separate `Material` assets — never an embedded copy. Saving the mesh only ever writes
   its own package; nothing is dragged in because nothing was owned in the first place.

   Mapped onto §8.3: Blender's Collection→Object→Mesh chain is *owned* (`ID` pointers with
   reference counting), so `asset_edit_id_ensure_local()` has no principled place to stop walking
   it. The Unreal-shaped resolution is to change what the container *is*, not to find a smarter
   walk: keep the editable core small (schema properties + a reference to e.g. LOD0) and make
   collision, other LODs, and materials `AssetWeakReference`s resolved on demand, per §8.4,
   rather than Collection members that are owned outright. That reframes "editable Collection
   container" as a real design constraint on the container's internal structure, not an
   implementation detail to solve later.
7. **Gating.** Reuse `use_extended_asset_browser`
   ([asset_type.cc:32](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/asset/intern/asset_type.cc#L32))
   or add a dedicated experimental flag?
