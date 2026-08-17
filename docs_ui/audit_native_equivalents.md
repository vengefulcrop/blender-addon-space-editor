# Audit: Native Blender Equivalents for the Add-on Space Editor

## Scope and method

Branch `pyareas/addon-space-editor` diffed against `main` with
`git diff main...pyareas/addon-space-editor -- source/ scripts/` (32 files, ~1965
insertions). Read in full:

- `source/blender/editors/space_addon/space_addon.cc` (900 lines)
- `source/blender/editors/space_addon/addon_intern.hh`
- `scripts/startup/bl_ui/space_addon.py` (471 lines)

Read as diff hunks with surrounding context: `blenkernel/intern/context.cc`,
`blenkernel/BKE_screen.hh`, `blenkernel/intern/screen.cc`, `editors/screen/area.cc`,
`makesdna/DNA_screen_types.h`, `makesdna/DNA_space_types.h`,
`makesdna/DNA_userdef_types.h`, `makesrna/intern/rna_screen.cc`,
`makesrna/intern/rna_space.cc`, `python/BPY_extern.hh`, `python/intern/bpy_rna.cc`.

Cross-referenced against the wider tree with targeted reads/greps of:

- `source/blender/editors/space_node/space_node.cc` (`node_space_subtype_get/set/item_extend`,
  `rna_node_tree_idname_to_enum`) — the sibling editor with the closest sub-type/module
  pattern.
- `source/blender/makesrna/intern/rna_ui.cc` (`panel_poll`, `panel_draw`) — the canonical
  Python `Panel.poll()`/`draw()` RNA-call site.
- `source/blender/blenlib/BLI_string_utils.hh`, `BLI_string_ref.hh` (grepped for `split`)
  for string-splitting idioms.
- `grep -rl BKE_screen_find_big_area source/blender` (12 call sites: `wm_window.cc`,
  `wm_files.cc`, `view3d_utils.cc`, `text_draw.cc`, `paint_image_proj.cc`,
  `render_view.cc`, `object_bake_api.cc`, `io_grease_pencil.cc`) to confirm this is an
  established, correctly-reused primitive rather than something reinvented.
- `grep -rl CTX_wm_area_set source/blender` (32 files) and a read of
  `python/intern/bpy_rna_context.cc` (`BPyContextTempOverride`) to check whether a
  reusable "scoped context override" idiom exists that the manual save/restore in
  `addon_main_region_layout` should have used.
- `grep -rl "rna_ext.call" source/blender/makesrna/intern` (7 files: `rna_ui.cc`,
  `rna_wm_gizmo.cc`, `rna_wm.cc`, `rna_render.cc`, `rna_nodetree.cc`, `rna_animation.cc`)
  to check whether an exception-checked RNA-call wrapper already exists elsewhere.
- `scripts/startup/bl_ui/space_userpref.py` (existing add-ons list UI) for comparison
  with `ADDON_UL_editors`.

I did not build or run the code; this is a static read/grep audit only, and I did not
modify anything.

---

## Findings, most significant first

### 1. `BPY_class_module_name_get` hand-rolls dotted-path segment extraction instead of using an existing string-splitting primitive

**Hand-rolled**: `source/blender/python/intern/bpy_rna.cc:10399-10437` (new function
`BPY_class_module_name_get`). It walks the `__module__` string with a raw `strchr` loop
to keep the first 1 or 3 dot-separated segments:

```cpp
const char *end = module;
for (int i = 0; i < segments_to_keep; i++) {
  const char *sep = strchr(end, '.');
  if (sep == nullptr) { end = module + strlen(module); break; }
  end = sep + 1;
}
const size_t len = (end > module && *(end - 1) == '.') ? size_t(end - module - 1) : size_t(end - module);
BLI_strncpy(r_module, module, std::min(len + 1, r_module_maxncpy));
```

**Native equivalent**: searched `BLI_string.hh`, `BLI_string_utils.hh` (`BLI_string_split_name_number`,
`BLI_string_split_suffix`/`_prefix` — all split on a *single* delimiter occurrence, not
"first N of many"), and `BLI_string_ref.hh` for a `StringRef::split`/tokenize method —
**none found**. `blender::StringRef` in this tree has no `split()`/tokenize helper, and
`BLI_string_utils.hh`'s split functions are single-delimiter, not "first N segments."
This is a case where I looked and did not find a ready-made equivalent — I cannot assert
one exists.

**Gap**: this is not a case of ignoring an existing helper; the closest existing
functions (`BLI_string_split_prefix`/`_suffix`) only split on the *first* occurrence, so
using them for the extension case (keep first 3 segments) would need to be called
in a loop anyway, netting no real simplification. Not a strong finding — flagged as
"searched, not found" rather than a confirmed miss.

**Effort**: N/A (no swap recommended without a suitable primitive existing).

---

### 2. `addon_panel_poll_guarded` duplicates `rna_ui.cc`'s `panel_poll` RNA-call boilerplate rather than factoring a shared, exception-checked helper

**Hand-rolled**: `source/blender/editors/space_addon/space_addon.cc:214-245`
(`addon_panel_poll_guarded`). It re-implements the `PointerRNA`/`ParameterList`/
`RNA_parameter_set_lookup`/`rna_ext.call`/`RNA_parameter_get_lookup` sequence found in
`rna_ui.cc`, adding an `err` check that `rna_ui.cc` lacks.

**Native equivalent (partial)**: `source/blender/makesrna/intern/rna_ui.cc:114-136`
(`panel_poll`) is structurally identical but discards the `pt->rna_ext.call(...)` return
value entirely — it cannot distinguish "poll() returned False" from "poll() raised."
Grepped `source/blender/makesrna/intern/*.cc` for other `rna_ext.call(...)` sites
(`rna_wm_gizmo.cc`, `rna_wm.cc`, `rna_render.cc`, `rna_nodetree.cc`, `rna_animation.cc`)
— none of them check the return code either. So there is no existing "call this RNA
function and detect whether it raised" helper anywhere in the tree; the new code is
actually a *better*, safer version of an existing pattern, just not shared back into
`rna_ui.cc` or a common location (e.g. `RNA_access.hh`) where `panel_poll` itself could
use it too.

**Gap**: this is code duplication in the direction of "the new copy is superior but
isolated," not "the new copy reinvented something worse." Worth flagging because the bug
class (silent redraw-rate exception storms) that motivated `addon_panel_poll_guarded`
could equally hit `rna_ui.cc`'s own `panel_poll` for any ordinary panel polled outside
its expected context, not just hosted ones — the safer implementation is currently
addon-editor-only.

**Effort**: medium. Extracting a shared `rna_call_checked()`-style helper into
`RNA_access.hh`/`rna_internal.hh` and having both `rna_ui.cc::panel_poll` and
`addon_panel_poll_guarded` call it would be a ~30-60 line refactor touching two files,
low risk since both call sites already agree on the calling convention.

---

### 3. Manual `CTX_wm_area_set`/`CTX_wm_region_set` save-restore in `addon_main_region_layout` has no scoped/RAII equivalent to reuse — confirmed absent, not overlooked

**Hand-rolled**: `source/blender/editors/space_addon/space_addon.cc:570-594`. Saves
`area_orig`/`region_orig`, conditionally swaps in a delegate area/region, calls
`ED_region_panels_layout_ex`, then manually restores both — with the restore only
reached through the normal (non-exceptional) control path.

**Searched for a native equivalent**: `python/intern/bpy_rna_context.cc`'s
`BPyContextTempOverride` (grepped via `CTX_wm_area_set` across 32 files) is the closest
analogue — a save/apply/restore-on-`__exit__` wrapper around `CTX_wm_*_set` — but it is
a **Python-object-lifetime construct** (`tp_dealloc`/`__exit__`), not a C++ RAII/scope-guard
type usable from `space_addon.cc`. No C++-side `ContextOverride` scope guard exists in
`BKE_context.hh` or elsewhere in `blenkernel`/`editors`; every other `CTX_wm_area_set`
call site I checked (`screen_ops.cc`, `render_update.cc`, `wm_operators.cc`, etc.) does
its own manual save/restore, matching what `space_addon.cc` does. This is the
established idiom in this codebase, not a reinvention of something better available.

**Gap**: none relative to existing practice. Noted only because it was an obvious place
to look for a missed primitive — there isn't one to swap to.

**Effort**: N/A — no change recommended.

---

### 4. `addon_delegate_spacetype_find` / `BKE_screen_find_big_area` reuse — correctly done, not a finding

`addon_delegate_spacetype_find` (`space_addon.cc:455-477`) and
`ctx_wm_area_effective` (`blenkernel/intern/context.cc`, new static function ahead of
`CTX_wm_space_data`) both resolve "find an open area of type X in this screen" via
`BKE_screen_find_big_area(screen, type, 0)`, an existing, widely-used primitive (12 call
sites across `wm_window.cc`, `wm_files.cc`, `view3d_utils.cc`, `text_draw.cc`,
`paint_image_proj.cc`, `render_view.cc`, `object_bake_api.cc`, `io_grease_pencil.cc`).
This is exactly the kind of native lookup the audit was asked to check for, and it is
used rather than re-derived by hand. No finding here — flagged as a deliberate
non-finding since the task asked to distinguish "reuses well" from "reinvents."

---

### 5. Editor sub-type get/set/item_extend pattern matches `space_node.cc`'s own convention closely — largely a non-finding, one minor asymmetry

**Hand-rolled**: `addon_space_subtype_get/set/item_extend`
(`space_addon.cc:746-842`) stores the add-on by string id (`SpaceAddon::addon_id`) and
maps it to/from an enum index computed from a live-built `Vector<AddonEditorEntry>`.

**Native equivalent**: `source/blender/editors/space_node/space_node.cc:1635-1654`
(`node_space_subtype_get/set/item_extend`) does the same shape of thing — string id
(`SpaceNode::tree_idname`) resolved to/from an index via
`rna_node_tree_idname_to_enum`/`rna_node_tree_type_from_enum`, and
`RNA_enum_node_tree_types_itemf_impl` (shared with `RNA_enum_node_tree_types_itemf`,
used elsewhere for property item lists) supplies the dropdown items.

**Gap**: minor. `space_node.cc` centralizes its id<->enum mapping in one shared
`rna_node_tree_idname_to_enum`/`itemf_impl` pair reused by both the space-subtype
callbacks and ordinary RNA enum properties elsewhere. The add-on editor's equivalent
(`addon_ids_get()`, `space_addon.cc:731-744`) is `static` and local to this file only —
there is no dynamic RNA itemf anywhere else in the tree that needs "list of curated
add-on editors" (a reasonable difference, since nothing else needs that list), so this
is not actually a missed reuse opportunity, just a structurally smaller version of the
same idiom because the id space is smaller. Not counted as a real finding.

**Effort**: N/A.

---

### 6. `bAddonEditor` / `UserDef::addon_editors` follows the existing `bAddon`/`addons` convention correctly

**Hand-rolled**: `source/blender/makesdna/DNA_userdef_types.h` new `bAddonEditor` struct
(module + name) and `ListBaseT<bAddonEditor> addon_editors` on `UserDef`, alongside
`active_addon_editor_index` and `addon_editor_max_visible`.

**Native equivalent**: `bAddon`/`ListBaseT<bAddon> addons` in the same file — the
existing convention for "a persistent list of user-curated add-on-identifying strings."
`bAddonEditor` mirrors it (module-name field, list-based storage, `.new()`-style Python
collection access via `context.preferences.addon_editors`) rather than reinventing, e.g.,
a fixed-size array or a delimited string blob. This is a case of correct reuse of an
established DNA idiom, not a finding.

---

### 7. `addon_screen_signature_get` bespoke bitmask cache-key — reasonable, no equivalent found, not a finding

`space_addon.cc:485-495` builds a `uint64_t` bitmask of which space types are open in a
screen, used purely as a cheap change-detection key alongside
`BKE_paneltypes_state_get()`. I looked for an existing "signature of open editor types"
helper (grepped `screen.cc`, `BKE_screen.hh`) and found none — this is a narrowly-scoped,
correctly-cheap piece of bespoke logic with no broader Blender equivalent to reuse, and
it is not attempting to replace any general-purpose primitive (e.g. it is not a
substitute for `BKE_screen_find_big_area`, which is still what actually resolves the
delegate on the following lines).

---

## Summary

- **1 confirmed duplication worth fixing**: #2, `addon_panel_poll_guarded` vs.
  `rna_ui.cc::panel_poll` — the new code has a strictly better (exception-safe) version
  of an existing RNA-call pattern that should arguably be factored out and shared back,
  rather than being an addon-editor-only fix for a bug class that also applies to
  ordinary panels.
- **1 unconfirmed/weak lead**: #1, the dotted-module-name segment extraction in
  `BPY_class_module_name_get` — searched `BLI_string.hh`, `BLI_string_utils.hh`, and
  `BLI_string_ref.hh` for a "first N segments" or generic split/tokenize helper and
  found none suitable; flagged explicitly as "looked, did not find" rather than treated
  as a miss.
- **4 explicit non-findings** (#3, #4, #5, #6, #7 — five, not four) where the
  implementation correctly reuses an existing Blender primitive or established DNA/RNA
  convention: `BKE_screen_find_big_area` for area lookup, the manual `CTX_wm_area_set`
  save/restore idiom (no RAII alternative exists anywhere in this codebase to have used
  instead), the `space_node.cc` sub-type get/set/item_extend shape, the `bAddon`-style
  persisted list convention for `bAddonEditor`, and the bespoke screen-signature cache
  key (no broader equivalent exists to replace it with).

Overall the implementation's stated goal of building on Blender's existing primitives
(`BKE_screen_find_big_area`, the `SpaceType::space_subtype_*` callback triple, the
`bAddon` list convention, `RNA_enum_item_add`, `ED_region_panels_layout_ex`,
`WM_window_is_main_top_level`) holds up under direct verification against the sibling
editors and shared subsystems it's structurally closest to. The one real opportunity
found is small and additive (share the exception-safe RNA poll wrapper back to
`rna_ui.cc`), not a sign of a parallel, narrower reimplementation of core machinery.
