# Audit: Candidates for Pruning and Codebase Cleanup

**Branch / Context:** `pyareas/addon-space-editor` vs `main` (`027ef661892c1234de0eb8d44bf5bb189eb39d81`)  
**Prefix:** `Gemini_`

---

## Verification notes (added 2026-08-17, cross-checked against source)

Each finding below was independently re-checked by reading the actual cited code and, where
relevant, the wider Blender source tree. Summary verdicts, detail inline per section:

| # | Finding | Verdict |
| :-: | :--- | :--- |
| 1 | Invasive core context redirection / "redundant dual mechanism" | **Incorrect.** Not redundant — verified the two mechanisms cover non-overlapping cases. |
| 2 | Duplicate panel scraping (Python vs C++) | **Partially valid.** Real duplication in the *filtering* logic; the Python-side *display-name* resolution is not duplicative — it uses data (`bl_info`, manifests) only Python can reach, by deliberate design. |
| 3 | String sentinel byte hack | **Valid**, with one caveat on the proposed fix (see below). |
| 4 | `PanelType` deep-cloning | **Incorrect.** Verified structurally necessary — not optional, not comparable to the Properties editor's case. |
| 5 | Global poll-failure blacklist | **Not independently verified this session** — plausible concern, not confirmed or refuted. |
| 6 | Indiscriminate redraw listener | **Incorrect** — this is a deliberate, already-documented tradeoff in the code's own comment, with the exact alternative Gemini proposes explicitly considered and rejected for a stated reason. |
| 7 | DNA persistence should be `IDProperty`, not a struct | **Incorrect.** Contradicts the actual, existing convention (`bAddon`) already in this codebase for the same kind of data. |

Full reasoning per item follows inline, prefixed `> VERIFIED:`.

---

## Executive Summary

The Add-on Space Editor introduces a new space type (`SPACE_ADDON`) designed to host add-on/extension panels in a dedicated window area and route context from other open editors. While the feature delivers functional panel hosting, the implementation contains significant over-engineering, duplicate discovery logic in both C++ and Python, invasive modifications to Blender's core context system, and brittle side-channel hacks that are prime candidates for pruning and simplification.

---

## 1. Invasive Core Context Redirection (`source/blender/blenkernel/intern/context.cc`)

### Issue & Mechanics
In `source/blender/blenkernel/intern/context.cc`, the implementation introduces `ctx_wm_area_effective(C)` and modifies **19 separate typed space context accessors**:
- `CTX_wm_space_data`
- `CTX_wm_view3d`, `CTX_wm_space_text`, `CTX_wm_space_console`, `CTX_wm_space_image`, `CTX_wm_space_properties`, `CTX_wm_space_file`, `CTX_wm_space_seq`, `CTX_wm_space_outliner`, `CTX_wm_space_nla`, `CTX_wm_space_node`, `CTX_wm_space_graph`, `CTX_wm_space_action`, `CTX_wm_space_info`, `CTX_wm_space_userpref`, `CTX_wm_space_clip`, `CTX_wm_space_topbar`, `CTX_wm_space_spreadsheet`, `CTX_wm_space_project`.

In addition, `ScrArea` in `source/blender/makesdna/DNA_screen_types.h` was modified to include `short context_delegate_spacetype`.

### Why it should be pruned
1. **Blenkernel Poluting & Coupling:** `blenkernel/intern/context.cc` is the core kernel context dispatcher. Hardcoding area delegation redirection into all individual space getters creates an architectural violation for a single experimental editor.
2. **Redundant Dual-Mechanism:** In `source/blender/editors/space_addon/space_addon.cc` (`addon_main_region_layout`), the implementation **also** performs direct context swapping:
   ```cpp
   CTX_wm_area_set(C_mutable, area_delegate);
   CTX_wm_region_set(C_mutable, BKE_area_find_region_type(area_delegate, RGN_TYPE_WINDOW));
   ED_region_panels_layout_ex(...);
   CTX_wm_area_set(C_mutable, area_orig);
   CTX_wm_region_set(C_mutable, region_orig);
   ```
   Having context redirection at both the kernel level (`ctx_wm_area_effective`) and transiently during region layout is redundant, confusing, and prone to desynchronization.

> **VERIFIED: incorrect.** Read both mechanisms directly. `context_delegate_spacetype`
> (routed through `ctx_wm_area_effective`) covers only the 18 *typed* space accessors
> (`CTX_wm_view3d`, `CTX_wm_space_node`, ...) and persists past the layout call — needed
> because operator polls and menus fire *after* layout, at button-press time. The
> transient `CTX_wm_area_set`/`CTX_wm_region_set` swap in `addon_main_region_layout`
> covers `CTX_wm_area`/`CTX_wm_region` themselves, which — confirmed by reading
> `CTX_wm_area()`'s own body (`context.cc:954`) — are **not** routed through
> `ctx_wm_area_effective` at all; they read `C->wm.area` directly. These are two
> different problems with non-overlapping scope (persistent typed-accessor delegation
> vs. transient raw pointer swap for one call), not duplication. Separately: also
> checked `bpy.context.temp_override()`'s actual C implementation
> (`python/intern/bpy_rna_context.cc`) — it applies its override via the exact same
> primitive, `CTX_wm_area_set`/`CTX_wm_region_set`, called directly and restored on
> exit. That's Blender's own canonical example of this being the established idiom for
> a transient area/region swap, not a smell unique to this fork.

---

## 2. Duplicate Panel Scraping & Attribution (Python vs C++)

### Issue & Mechanics
There are two completely independent, parallel implementations of panel discovery, inheritance analysis, and add-on module attribution:

1. **Python Side (`scripts/startup/bl_ui/space_addon.py`):**
   - `_registered_panel_classes()` walks `bpy.types.Panel.__subclasses__()` recursively via an explicit stack to find derived panel classes and checks `getattr(cls, "is_registered", False)`.
   - `_addon_top_level_panel_space_types()` parses `cls.__module__` strings to match add-on prefixes (`foo` vs `foo.bar`).
   - `_addon_supported_spaces()` and `_addon_has_open_delegate()` duplicate the filtering logic.

2. **C++ Side (`source/blender/editors/space_addon/space_addon.cc` and `source/blender/blenkernel/intern/screen.cc`):**
   - `BKE_paneltypes_addon_space_types_get()` iterates `BKE_spacetypes_list() -> regiontypes -> paneltypes`.
   - `BPY_class_module_name_get()` in `source/blender/python/intern/bpy_rna.cc` queries `__module__` from `pt.rna_ext.data` via Python C-API and handles extension prefixing (`bl_ext.`).
   - `addon_panel_types_collect()` and `addon_has_registered_panels()` do their own redundant scans.

### Why it should be pruned
- Python's `bpy.types.Panel.__subclasses__()` traversal is slow, fragile, and misses C-defined panels or dynamic panel registrations that don't follow standard Python subclassing.
- C++ already possesses the authoritative registry in `SpaceType::regiontypes::paneltypes`.
- Maintaining two implementations leads to drift where Python displays one set of supported editors in headers/tooltips while C++ collects a different set during layout.

> **VERIFIED: partially valid.** The *filtering* duplication is real — this matches a
> finding independently reached by a separate internal audit (`audit_pruning_candidates.md`
> #3): the "which top-level panels belong to add-on X" scan is implemented nearly
> identically three times (`addon_panel_types_collect`, `addon_has_registered_panels` in
> `space_addon.cc`, and `BKE_paneltypes_addon_space_types_get` in `screen.cc`) — worth a
> shared helper. However, the recommendation to move Python's crawling to "query the C++
> registry via RNA" is an overreach for the *display-name* half of what Python does:
> the plan doc records this as a deliberate choice, not an oversight — resolving a
> human-readable name needs `addon_utils.module_bl_info()` / extension manifest data,
> which only Python can reach, and both consumers of that data (header, empty-state
> panel) are already Python-side, so crossing the language boundary for it would add
> plumbing without moving where the answer is actually needed. Consolidate the
> space-type *filtering* logic; leave the name resolution where it is.

---

## 3. String Sentinel Byte Hack (`SPACE_ADDON_ID_PICK_MARKER`)

### Issue & Mechanics
In `source/blender/makesdna/DNA_space_types.h` and `source/blender/editors/space_addon/space_addon.cc`:
- `#define SPACE_ADDON_ID_PICK_MARKER '\x01'`
- When the user selects "Add an Add-on..." from the sub-type dropdown, `addon_space_subtype_set` prefixes `\x01` onto `SpaceAddon::addon_id`.
- Later, in `source/blender/makesrna/intern/rna_screen.cc` (`rna_Area_ui_type_update`), it detects `saddon->addon_id[0] == '\x01'`, performs a `memmove` to strip the byte, and executes `WM_operator_name_call(C, "ADDON_OT_pick_and_host", ...)`.

### Why it should be pruned
- Mutating a DNA string buffer with control characters as an inter-function communication channel is a fragile hack.
- RNA setters should not encode modal side effects via string corruption.
- UI operators and header popups should trigger search popups directly through standard UI layouts rather than intercepting enum assignments.

> **VERIFIED: valid, with a caveat.** The fragility is real and self-documented: the
> plan doc's own log records a shipped bug from exactly this mechanism (the `-1` vs.
> `0x7FFF` sentinel-collision issue). But the "just use a standard header operator
> instead" framing skips *why* it's shaped this way: the picker entry has to live
> inside the *same* enum dropdown as the curated add-on list, because the explicit
> design goal is one unified editor-type selector, not "list plus a separate button."
> Gemini's alternative is a legitimate UX direction, but it's a design tradeoff to
> weigh, not simply a hack to delete — the sentinel mechanism could be hardened (e.g. a
> named constant instead of a raw `'\x01'` byte, more validation) without necessarily
> abandoning the unified-dropdown approach.

---

## 4. `PanelType` Deep-Cloning and Lifetime Management

### Issue & Mechanics
In `source/blender/editors/space_addon/space_addon.cc`:
- Because `PanelType` has intrusive linked list pointers (`next`, `prev`), `addon_panel_types_collect()` creates heap duplicates of `PanelType` structs using `MEM_dupalloc(&pt)` and stores them in `SpaceAddon_Runtime::paneltypes`.
- To avoid dangling pointers in `region->panels`, it implements:
  - `addon_paneltype_pop()`
  - `addon_panels_type_detach()` (setting `panel.type = nullptr` across all active panels)
  - `SpaceAddon_Runtime::cached_addon_id`, `cached_paneltypes_state`, `cached_screen_signature`
  - Global panel versioning counter `BKE_paneltypes_tag_changed()` / `BKE_paneltypes_state_get()` added to `source/blender/makesrna/intern/rna_ui.cc` and `screen.cc`.
  - Bitmask screen layout hashing in `addon_screen_signature_get()`.

### Why it should be pruned
- Cloning `PanelType` structs violates the design assumption that `PanelType` instances are static registrations managed solely by `WM_paneltype_add` / `WM_paneltype_remove`.
- Mutating cloned `PanelType` pointers (`pt_copy->poll = addon_panel_poll_guarded`) creates dangling references if child panels or RNA lookups attempt to trace back to original types.
- An array of `const PanelType*` pointers or passing a filter predicate to the layout engine eliminates the entire caching, diffing, cloning, and detachment subsystem.

> **VERIFIED: incorrect.** Read `ED_region_panels_layout_ex`'s actual mechanics
> directly (`editors/screen/area.cc:3297-3385`): it walks the passed-in panel list via
> `paneltypes->items_reversed()` — i.e. the list's own intrusive `next`/`prev` links. A
> `PanelType` can only be linked into *one* such chain at a time; it is already a
> member of its home region type's own list. A pointer array would not fix this — the
> layout function doesn't accept one, it requires a real `ListBaseT<PanelType>` with
> working links. Cloning is the only way to draw a `VIEW_3D`-registered panel inside a
> `SPACE_ADDON` region without corrupting the 3D Viewport's own panel list.
> "Instead, pass pointers, like the Properties editor does" does not hold up: I checked
> `space_buttons.cc`'s panel filtering, and it only ever filters panels that are
> *already* members of its own native region-type list — it never moves a panel
> between two different lists, so it never hits the constraint this editor's cloning
> exists to solve. Comparing the two is not apples-to-apples.

---

## 5. Static Global Exception Blacklist (`addon_poll_failed_get`)

### Issue & Mechanics
In `source/blender/editors/space_addon/space_addon.cc`:
- A static `Set<std::string> failed` stores ID names of panels whose `poll()` raised a Python exception when evaluated out of context.
- `addon_panel_poll_guarded()` wraps RNA poll execution and permanently blacklists matching panels from ever being polled again until panel types change.

### Why it should be pruned
- Global static state across all areas causes cross-window and cross-workspace side effects.
- Hiding exceptions silently and permanently blacklisting panels makes debugging add-ons impossible.
- Standard Blender error handling at the Python/RNA boundary already captures and reports script errors.

> **VERIFIED: not independently checked this session.** Plausible concern on its face
> (global state, silent-until-panel-types-change blacklisting), but neither the
> internal audits nor this correction pass traced through whether standard RNA-boundary
> error handling alone would actually be sufficient here (this poll wrapper exists
> specifically to stop a raising `poll()` from being re-invoked every redraw — a plain
> "let the standard reporting handle it" approach might just mean the same exception
> logs every frame instead of once). Flagging as open rather than confirming or
> refuting — worth a dedicated look before acting on this one.

---

## 6. Indiscriminate Redraw Listener (`addon_main_region_listener`)

### Issue & Mechanics
In `source/blender/editors/space_addon/space_addon.cc`:
```cpp
static void addon_main_region_listener(const wmRegionListenerParams *params)
{
  switch (params->notifier->category) {
    case NC_WINDOW:
    case NC_SCREEN:
    case NC_WORKSPACE:
    case NC_WM:
      break;
    default:
      ED_region_tag_redraw(params->region);
      break;
  }
}
```

### Why it should be pruned
- Redrawing on *every single notifier* in Blender (object transforms, animation playback ticks, material changes, node edits, grease pencil strokes, etc.) introduces significant UI thread overhead and defeats Blender's selective notifier hierarchy.
- The listener should either delegate to the active space type's listener or listen specifically to relevant data categories (`NC_SPACE`, `NC_SCENE`, `NC_OBJECT`).

> **VERIFIED: incorrect — already considered and deliberately rejected.** Read
> `addon_main_region_listener`'s own doc comment directly
> (`space_addon.cc:601-620`). It states the reasoning explicitly: "Forwarding to the
> delegate editor's own listener was the obvious alternative and is not safe: several
> listeners cast `params->area->spacedata` to their own space type (see
> `space_clip.cc`, `space_action.cc`, `space_buttons.cc`), and this area holds a
> `SpaceAddon`. Substituting the delegate's area would fix the cast but silently give
> those listeners a different area than the region they are tagging. Redrawing a little
> too often is the cheaper mistake." This is exactly the alternative Gemini proposes
> ("delegate to the active space type's listener"), already tried in reasoning and
> rejected for a concrete, cited cause — not an oversight. (Self-correction: an earlier
> pass in this conversation called this finding "worth taking seriously" without
> cross-checking it against this comment, which had already been read earlier in the
> same session — that was a mistake, corrected here.)

---

## 7. Heavyweight DNA Persistence for Curated Editor List

### Issue & Mechanics
In `source/blender/makesdna/DNA_userdef_types.h`, `readfile.cc`, `writefile.cc`, `versioning_userdef.cc`, and `rna_userdef.cc`:
- Added `struct bAddonEditor` linked list to `UserDef`.
- Added `UserDef::addon_editors`, `UserDef::active_addon_editor_index`, `UserDef::addon_editor_max_visible`, `USER_ADDON_EDITOR_SHOW_BUNDLED` in `uiflag2`.
- Added DNA serialization in `read_userdef` / `write_userdef` and versioning in `blo_do_versions_userdef`.

### Why it should be pruned
- Adding dedicated DNA structs to `UserDef` for managing a UI-level curated list of string module names bloats Blender's core user preferences schema.
- Add-on settings and lists are conventionally managed via `IDProperty` preferences or Python `AddonPreferences` without modifying core C DNA definitions.

> **VERIFIED: incorrect.** `bAddonEditor` was deliberately modeled on the existing
> `bAddon` struct, already present in `DNA_userdef_types.h` — Blender's own established
> convention for "a persistent list of user-curated add-on-related entries in
> `UserDef`" already lives in DNA, not `IDProperty`. The claim that DNA structs are
> unconventional for this contradicts the actual precedent sitting in the same file.
> This is a matter of following existing house style, not introducing new schema
> bloat by a novel pattern.

---

## Summary of Pruning Recommendations

| Component | Target Location | Rationale for Pruning |
| :--- | :--- | :--- |
| **Kernel Context Accessors** | `blenkernel/intern/context.cc` (19 functions) | Remove `ctx_wm_area_effective`; rely on local context override during layout |
| **Python Panel Scraping** | `bl_ui/space_addon.py` (`_registered_panel_classes`) | Eliminate Python subclass crawling; query C++ panel registry via RNA |
| **Picker Sentinel Byte Hack** | `DNA_space_types.h`, `space_addon.cc`, `rna_screen.cc` | Remove `SPACE_ADDON_ID_PICK_MARKER`; invoke picker directly via UI |
| **PanelType Cloning Engine** | `space_addon.cc` (`paneltype_copy_get`, `SpaceAddon_Runtime`) | Eliminate `MEM_dupalloc` copies and manual detachment; pass pointer array |
| **Global Poll Blacklist** | `space_addon.cc` (`addon_poll_failed_get`) | Remove static failure set; handle poll failures through standard RNA reporting |
| **Catch-All Listener** | `space_addon.cc` (`addon_main_region_listener`) | Replace `default: tag_redraw` with targeted notification filtering |
| **UserDef DNA Structs** | `DNA_userdef_types.h`, `readfile.cc`, `writefile.cc` | Migrate `bAddonEditor` persistence to IDProperties or Python preferences |
