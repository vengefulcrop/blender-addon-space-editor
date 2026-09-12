---
type: research
title: "Native Blender Equivalents for the Add-on Space Editor"
description: "Merged internal and external audit of hand-rolled code in the Add-on Space Editor versus existing native Blender primitives"
tags: [audit, addon-editor, native-apis]
last_updated: 2026-09-12
sources:
  - id: internal-native-equivalents
    resource: docs_ui/audits/audit_native_equivalents.md
    title: "Internal audit: native Blender equivalents"
    author: claude-sonnet/agent
  - id: external-native-equivalents
    resource: docs_ui/audits/Gemini_native_blender_equivalents.md
    title: "External audit: native Blender equivalents"
    author: gemini/reviewer
---

# Native Blender Equivalents for the Add-on Space Editor

This file merges two audits of the same branch, `pyareas/addon-space-editor` versus
`main`:

- The internal audit, `audit_native_equivalents.md`. A Claude Sonnet agent wrote
  it. The agent read the source and the diff hunks directly.
- The external audit, `Gemini_native_blender_equivalents.md`. Gemini wrote it as
  an external reviewer.

Where the two audits disagree, this file states both verdicts and names the
disagreement. See [`verified_native_apis.md`](./verified_native_apis.md) for the
exact signatures and file locations of every cited native API, and
[`reconciliation_summary.md`](./reconciliation_summary.md) for the cross-check that
produced these verdicts.

## Method

**Internal audit.** Diffed `main...pyareas/addon-space-editor` for `source/` and
`scripts/` (32 files, about 1965 insertions). Read in full:

- `source/blender/editors/space_addon/space_addon.cc` (900 lines)
- `source/blender/editors/space_addon/addon_intern.hh`
- `scripts/startup/bl_ui/space_addon.py` (471 lines)

Read as diff hunks with context: `blenkernel/intern/context.cc`,
`blenkernel/BKE_screen.hh`, `blenkernel/intern/screen.cc`, `editors/screen/area.cc`,
`makesdna/DNA_screen_types.h`, `makesdna/DNA_space_types.h`,
`makesdna/DNA_userdef_types.h`, `makesrna/intern/rna_screen.cc`,
`makesrna/intern/rna_space.cc`, `python/BPY_extern.hh`, `python/intern/bpy_rna.cc`.

Cross-referenced against `space_node.cc`, `rna_ui.cc`, `BLI_string_utils.hh`,
`BLI_string_ref.hh`, and grepped for `BKE_screen_find_big_area`,
`CTX_wm_area_set`, and `rna_ext.call`. This was a static read and grep audit. No
code was built, run, or changed.

**External audit.** Reviewed the same branch and diff, citing specific file and
line locations for each claimed native equivalent.

## Findings

### 1. Dotted module-name segment extraction

`BPY_class_module_name_get` (`source/blender/python/intern/bpy_rna.cc:10399-10437`)
walks a Python `__module__` string with a raw `strchr` loop to keep the first 1 or
3 dot-separated segments.

- Internal audit: searched `BLI_string.hh`, `BLI_string_utils.hh`, and
  `BLI_string_ref.hh` for a "first N segments" split or tokenize helper. Found
  none. `blender::StringRef` has no `split()` method. `BLI_string_split_prefix`/
  `_suffix` only split on the first occurrence, so using them here would still
  need a loop. Verdict: no simplification is available. Flagged as "searched, not
  found," not as a confirmed miss.
- External audit: did not raise this item.

### 2. `addon_panel_poll_guarded` duplicates `rna_ui.cc` poll boilerplate

`addon_panel_poll_guarded` (`space_addon.cc:214-245`) re-implements the
`PointerRNA`/`ParameterList`/`RNA_parameter_set_lookup`/`rna_ext.call`/
`RNA_parameter_get_lookup` sequence in `rna_ui.cc:114-136` (`panel_poll`), and adds
an `err` check that `panel_poll` lacks.

- Internal audit: grepped all `rna_ext.call` sites (`rna_ui.cc`, `rna_wm_gizmo.cc`,
  `rna_wm.cc`, `rna_render.cc`, `rna_nodetree.cc`, `rna_animation.cc`). None check
  the return code. `addon_panel_poll_guarded` is a strictly safer version of an
  existing pattern, not a worse reinvention, but it is isolated to the add-on
  editor. The same silent-exception bug class can hit `panel_poll` for any
  ordinary panel polled outside its expected context. Effort to fix: medium.
  Extract a shared `rna_call_checked()`-style helper into `RNA_access.hh` or
  `rna_internal.hh`, used by both `rna_ui.cc::panel_poll` and
  `addon_panel_poll_guarded` (about 30 to 60 lines, two files, low risk).
- External audit: did not raise this item as a separate finding.

### 3. Manual context save-restore in `addon_main_region_layout`

`space_addon.cc:570-594` saves `area_orig`/`region_orig`, swaps in a delegate
area/region, calls `ED_region_panels_layout_ex`, then restores both manually. The
restore path only runs on the non-exceptional path.

| Audit | Cited native equivalent | Verdict |
|---|---|---|
| Internal | `python/intern/bpy_rna_context.cc`'s `BPyContextTempOverride` | Closest analogue, but it is a Python-object-lifetime construct (`tp_dealloc`/`__exit__`), not a C++ RAII type usable from `space_addon.cc`. No C++ `ContextOverride` scope guard exists in `BKE_context.hh` or elsewhere. Every other `CTX_wm_area_set` call site (`screen_ops.cc`, `render_update.cc`, `wm_operators.cc`) does its own manual save/restore. This is the established idiom, not a gap. |
| External | `struct bContextStore` / `CTX_store_*` (`BKE_context.hh:122-186`), exposed in Python as `bpy.context.temp_override(...)` | Cited as a cleaner overlay mechanism that avoids polluting global accessors. |

**Disagreement, resolved.** Read `python/intern/bpy_rna_context.cc` directly (the
actual C implementation of `temp_override()`). It does not call `CTX_store_add`/
`CTX_store_set` anywhere. It applies its override with a private, file-local
struct (also named `ContextStore`, without the `b` prefix, likely the source of
the external audit's confusion) and the same raw `CTX_wm_area_set`/
`CTX_wm_region_set` primitives the fork already uses, wrapped in a Python-only
context manager object. The internal audit's verdict stands: `bContextStore` is
real, but it is not what `temp_override()` uses, and no C++ RAII alternative
exists in this codebase.

### 4. `addon_delegate_spacetype_find` / `BKE_screen_find_big_area`

`addon_delegate_spacetype_find` (`space_addon.cc:455-477`) and
`ctx_wm_area_effective` (a new static function in `blenkernel/intern/context.cc`)
both resolve "find an open area of type X in this screen" via
`BKE_screen_find_big_area(screen, type, 0)`, an existing primitive with 12 call
sites (`wm_window.cc`, `wm_files.cc`, `view3d_utils.cc`, `text_draw.cc`,
`paint_image_proj.cc`, `render_view.cc`, `object_bake_api.cc`,
`io_grease_pencil.cc`). Internal audit verdict: correct reuse, not a finding.

### 5. Editor sub-type get/set/item_extend pattern

`addon_space_subtype_get/set/item_extend` (`space_addon.cc:746-842`) stores the
add-on by string id (`SpaceAddon::addon_id`) and maps it to an enum index from a
live-built `Vector<AddonEditorEntry>`. This matches
`node_space_subtype_get/set/item_extend` (`space_node.cc:1635-1654`), which does
the same shape of thing with `SpaceNode::tree_idname`.

Minor asymmetry: `space_node.cc` centralizes its mapping in a shared
`rna_node_tree_idname_to_enum`/`itemf_impl` pair, reused elsewhere for ordinary
RNA enum properties. The add-on editor's `addon_ids_get()` (`space_addon.cc:731-744`)
is `static` and local to one file. Internal audit verdict: not a missed reuse
opportunity, because nothing else in the tree needs a "list of curated add-on
editors."

### 6. `bAddonEditor` / `UserDef::addon_editors`

`bAddonEditor` (module + name struct) and `ListBaseT<bAddonEditor> addon_editors`
on `UserDef`, plus `active_addon_editor_index` and `addon_editor_max_visible`.

| Audit | Cited native equivalent | Verdict |
|---|---|---|
| Internal | `bAddon`/`ListBaseT<bAddon> addons` in the same file | `bAddonEditor` mirrors this established convention correctly (module-name field, list-based storage, `.new()`-style Python collection access). Not a finding. |
| External | `IDProperty` dynamic groupings (`BKE_idprop.hh`), or Python `AddonPreferences`/`PropertyGroup` | Cited as more conventional, avoiding DNA schema changes. |

**Disagreement, resolved.** `bAddonEditor` was deliberately modeled on the
existing `bAddon` struct, already present in `DNA_userdef_types.h`. Blender's own
established convention for a persistent list of user-curated add-on-related
entries in `UserDef` already lives in DNA, not `IDProperty`. The claim that DNA
structs are unconventional for this contradicts the existing precedent in the
same file. The internal audit's verdict stands.

### 7. `addon_screen_signature_get` cache key

`space_addon.cc:485-495` builds a `uint64_t` bitmask of which space types are
open in a screen, used as a cheap change-detection key alongside
`BKE_paneltypes_state_get()`. Internal audit: grepped `screen.cc` and
`BKE_screen.hh` for an existing "signature of open editor types" helper. Found
none. This is narrowly-scoped, correctly-cheap bespoke logic with no broader
equivalent to replace it, and it does not substitute for
`BKE_screen_find_big_area`, which still resolves the delegate.

### 8. Context routing: 19 kernel accessors plus a manual swap

The fork modifies 19 typed space context accessors in
`blenkernel/intern/context.cc` (`CTX_wm_space_data`, `CTX_wm_view3d`,
`CTX_wm_space_text`, `CTX_wm_space_console`, `CTX_wm_space_image`,
`CTX_wm_space_properties`, `CTX_wm_space_file`, `CTX_wm_space_seq`,
`CTX_wm_space_outliner`, `CTX_wm_space_nla`, `CTX_wm_space_node`,
`CTX_wm_space_graph`, `CTX_wm_space_action`, `CTX_wm_space_info`,
`CTX_wm_space_userpref`, `CTX_wm_space_clip`, `CTX_wm_space_topbar`,
`CTX_wm_space_spreadsheet`, `CTX_wm_space_project`), routed through
`ctx_wm_area_effective(C)`, and adds `ScrArea::context_delegate_spacetype`
(`DNA_screen_types.h`). Separately, `addon_main_region_layout` performs a manual
`CTX_wm_area_set`/`CTX_wm_region_set` swap around `ED_region_panels_layout_ex`.

- External audit: called this a "redundant dual mechanism."
- Verification (from the reconciliation pass): read both mechanisms directly.
  `context_delegate_spacetype`, routed through `ctx_wm_area_effective`, covers
  only the 18 typed space accessors and persists past the layout call, needed
  because operator polls and menus fire after layout, at button-press time. The
  transient `CTX_wm_area_set`/`CTX_wm_region_set` swap covers `CTX_wm_area`/
  `CTX_wm_region` themselves, which are not routed through
  `ctx_wm_area_effective` at all. `CTX_wm_area()` reads `C->wm.area` directly
  (`context.cc:954`). These are two problems with non-overlapping scope, not
  duplication. Verdict: external audit's claim does not hold up.

### 9. Panel filtering and layout: `PanelType` deep-cloning

`addon_panel_types_collect()` deep-clones `PanelType` instances with
`MEM_dupalloc(&pt)` into `SpaceAddon_Runtime::paneltypes`, with
`addon_paneltype_pop()`, `addon_panels_type_detach()`, and cache-invalidation
state (`cached_addon_id`, `cached_paneltypes_state`, `cached_screen_signature`).

- External audit: cited the Properties editor (`space_buttons.cc:315`,
  `panel_add_check()` at `area.cc:3320`) as proof that "no `PanelType` structs are
  cloned" is possible, and recommended a `Vector<const PanelType*>` instead.
- Verification: read `ED_region_panels_layout_ex` directly
  (`area.cc:3297-3385`). It walks the passed list via `paneltypes->items_reversed()`,
  the list's own intrusive `next`/`prev` links. A `PanelType` can only belong to
  one such chain at a time, and every registered `PanelType` is already linked
  into its home region type's list. Cloning is the only way to draw a
  `VIEW_3D`-registered panel inside a `SPACE_ADDON` region without corrupting the
  3D Viewport's own list. The Properties editor comparison does not hold: it only
  filters panels that are already native members of its own list, so it never
  faces this constraint. Verdict: external audit's claim is incorrect. Cloning is
  structurally required, not optional.

### 10. Picker sub-type invocation

The fork encodes an out-of-band marker `SPACE_ADDON_ID_PICK_MARKER = '\x01'` into
`SpaceAddon::addon_id` when the user selects "Add an Add-on...". `rna_screen.cc`
(`rna_Area_ui_type_update`) detects the marker, strips it with `memmove()`, and
calls `WM_operator_name_call(C, "ADDON_OT_pick_and_host", ...)`.

- External audit: called this fragile and recommended a standard header operator
  or `invoke_search_popup` instead.
- Verification: `invoke_search_popup` is real and already in use. The plan doc's
  own log records the current picker operator, `ADDON_OT_pick_and_host`, already
  using `invoke_search_popup`. The sentinel byte only gets from the enum dropdown
  to that operator call. It does not compete with `invoke_search_popup`, which
  already runs once the operator is invoked. The fragility itself is real: the
  plan doc's own log records a shipped bug from a sentinel-value collision
  (`-1` versus `0x7FFF`). The design goal is a single unified dropdown, not a
  list plus a separate button, so the fix is to harden the sentinel (for example
  a named constant, more validation), not to abandon the unified dropdown.

### 11. Panel discovery: Python subclass crawling versus `owner_id`

Python's `_registered_panel_classes()` walks `bpy.types.Panel.__subclasses__()`
recursively. `BPY_class_module_name_get()` parses `__module__` for extension
prefixing.

- External audit: cited `PanelType::owner_id` (`BKE_screen.hh:383`) and
  `BKE_workspace_owner_id_check()` (`BKE_workspace.hh:183`,
  `workspace.cc:613`) as the native mechanism for add-on identity, and
  `bpy.types.SpaceType.panel_types` as a way to expose the C++ registry to
  Python directly.
- Verification: the cited call site (`area.cc:3321-3326`) gates this check
  behind `if (panel_type->owner_id[0])`, meaning it applies only when a panel
  author has explicitly set `bl_owner_id`. The plan doc's own design log (§1.3)
  already investigated this and rejected it: `owner_id` is a workspace UI filter,
  empty for most panels, and not an add-on identifier. Repurposing it would
  break workspace filtering for panels that already use it. Separately,
  `bpy.types.SpaceType.panel_types` does not exist: zero grep matches in
  `makesrna/`, and no file:line citation was given for it. Verdict: external
  audit's claim is incorrect for this purpose.

### 12. Redraw listener

`addon_main_region_listener` tags a redraw for every notifier except `NC_WINDOW`,
`NC_SCREEN`, `NC_WORKSPACE`, and `NC_WM`.

- External audit: cited `ARegionType::listener` (`BKE_screen.hh:281`) as an
  unused alternative, pointing to `buttons_main_region_listener` and
  `view3d_buttons_region_listener` as examples of targeted listeners.
- Verification: `ARegionType::listener` is not an unused alternative. It is the
  exact mechanism already in use: `space_addon.cc:880` sets
  `art->listener = addon_main_region_listener;`. Whether the listener's breadth
  is the right call is a separate question, but it is already covered by the
  listener's own comment (`space_addon.cc:601-620`), which explicitly considered
  and rejected forwarding to the delegate's own listener. The cited reason:
  several editors' listeners (`space_clip.cc`, `space_action.cc`,
  `space_buttons.cc`) cast `params->area->spacedata` to their own space type.
  Substituting the delegate's area would silently mismatch the region being
  tagged. Redrawing too often is the accepted cheaper mistake. Not an oversight.

## Summary

| Confidence | Finding |
|---|---|
| Confirmed duplication worth fixing | #2: `addon_panel_poll_guarded` should share its exception-safe RNA-call pattern with `rna_ui.cc::panel_poll`, rather than staying addon-editor-only |
| Unconfirmed, weak lead | #1: dotted-module-name segment extraction has no suitable native split/tokenize helper to swap to |
| Non-findings, correct reuse confirmed | #3 (no RAII alternative exists), #4 (`BKE_screen_find_big_area` reused correctly), #5 (sub-type pattern matches `space_node.cc`), #6 (`bAddonEditor` follows `bAddon` convention), #7 (bespoke cache key has no broader equivalent) |
| External audit claims that do not survive verification | #6 (`IDProperty`), #8 (redundant dual mechanism), #9 (`PanelType` cloning avoidable), #10 (sentinel byte should become a header operator), #11 (`owner_id` / `SpaceType.panel_types`), #12 (listener should delegate) |

Overall, the implementation's stated goal of building on Blender's existing
primitives (`BKE_screen_find_big_area`, the `SpaceType::space_subtype_*` callback
triple, the `bAddon` list convention, `RNA_enum_item_add`,
`ED_region_panels_layout_ex`, `WM_window_is_main_top_level`) holds up under direct
verification. The one real opportunity is small and additive: share the
exception-safe RNA poll wrapper back to `rna_ui.cc`.
