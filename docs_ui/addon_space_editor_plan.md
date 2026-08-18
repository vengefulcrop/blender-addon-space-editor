# Add-on Space Editor — Corrected Analysis & Implementation Plan

**Status**: planning complete, implementation not started
**Base**: Blender `main` @ `027ef661` (5.3.0 alpha), cloned 2026-07-31
**Supersedes**: [custom_python_space_types_architecture.md](custom_python_space_types_architecture.md)

---

## 0. Goal

Let a user turn any enabled add-on or extension into a **first-class editor**. The
add-on's N-panel becomes the entire content of a screen area, selectable from the
same editor-type dropdown that offers 3D Viewport, Shader Editor, Outliner, etc.

### Intended user flow

1. User clicks the editor-type button in an area header.
2. The list contains a permanent **`Add-on…`** entry alongside the built-in editors.
3. Clicking it opens a picker listing **all enabled add-ons / extensions**.
4. User picks one. That add-on now appears as its own permanent entry in the
   editor-type list, and the current area switches to it.
5. The area draws that add-on's panels as a full editor.

### Distribution goal

The fork's diff is published so users can compile their own build. This constrains
the design: the change must stay a **small, reviewable, rebasable patch series**
against upstream `main`, not a sprawling refactor.

---

## 1. Corrections to the original analysis

The superseded document was written against a different (already-modified) clone and
assumed a heavier architecture than 5.3 actually requires. Three corrections, each
verified against the fresh clone.

### 1.1 No dynamic space-type registration is needed — subtypes already exist

**Original claim** (§1.2): extend `BKE_spacetype_register()` / `BKE_spacetype_from_id()`
to manage space types registered dynamically at runtime from Python, with lifecycle
and teardown management (~80–120 LOC of new kernel machinery).

**Correction**: `SpaceType` already carries a subtype mechanism, at
[`BKE_screen.hh:156-158`](../source/blender/blenkernel/BKE_screen.hh#L156-L158):

```cpp
int  (*space_subtype_get)(ScrArea *area);
void (*space_subtype_set)(ScrArea *area, int value);
void (*space_subtype_item_extend)(bContext *C, EnumPropertyItem **item, int *totitem);
```

And `rna_Area_ui_type_itemf` already folds subtypes into the editor dropdown, at
[`rna_screen.cc:216-233`](../source/blender/makesrna/intern/rna_screen.cc#L216-L233):

```cpp
SpaceType *st = item_from->identifier[0] ? BKE_spacetype_from_id(item_from->value) : nullptr;
int totitem_prev = totitem;
if (C && st && st->space_subtype_item_extend != nullptr) {
  st->space_subtype_item_extend(C, &item, &totitem);
  while (totitem_prev < totitem) {
    item[totitem_prev++].value |= item_from->value << 16;
  }
}
```

The enum value is packed as `space_type << 16 | subtype`. This is exactly how the
Node Editor presents Shader / Compositor / Geometry Nodes as three separate entries
in the editor menu while being **one** registered space type.

**Consequence**: we register **one** new space type, `SPACE_ADDON`, and every add-on
is a *subtype* of it. No dynamic registration, no runtime `SpaceType` allocation, no
teardown lifecycle, no unregister-crash risk from §3.1 of the old document — because
no `SpaceType` is ever created or destroyed at runtime. Section 1.2 of the old plan
is deleted entirely.

**Persistence**: subtype indices are not stable across sessions (add-on enable/disable
reorders them), so the index is a *view* concern only. The DNA stores the add-on's
module name as a string. This mirrors the Node Editor, which stores the node-tree type
idname in `SpaceNode` and resolves it to an index in `space_subtype_get`.

### 1.2 Panel re-hosting is a hook, not a new drawing stack

**Original claim** (§1.5): build a new editor module that wraps region rendering with
`ui::block_begin` / `ui::block_end` and invokes Python `draw(context)` directly
(~450–600 LOC).

**Correction**: `ED_region_panels_layout_ex` accepts an **arbitrary** panel-type list
rather than being hard-wired to the region's own, at
[`area.cc:3362-3367`](../source/blender/editors/screen/area.cc#L3362-L3367):

```cpp
void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBaseT<PanelType> *paneltypes,
                                wm::OpCallContext op_context,
                                const char *contexts[],
                                const char *category_override);
```

The Properties editor already exploits this to swap panel sets per tab
([`space_buttons.cc:315`](../source/blender/editors/space_buttons/space_buttons.cc#L315)).

**Consequence**: our window region builds a filtered `ListBaseT<PanelType>` of the
chosen add-on's panels and passes it in. We inherit — for free — panel headers,
open/closed state, drag-to-reorder, category tabs, panel search, and background
drawing. No manual block management, no direct Python `draw()` invocation. This
collapses the largest line-count item in the old estimate by roughly two thirds.

### 1.3 The real gap: C++ cannot identify which add-on owns a panel

Neither the old document nor the subtype mechanism addresses this, and it is the only
genuinely new problem in the feature.

`PanelType` has an `owner_id` field at
[`BKE_screen.hh:383`](../source/blender/blenkernel/BKE_screen.hh#L383):

```cpp
/** For work-spaces to selectively show. */
char owner_id[128];
```

The comment is the point: this is the **workspace filter** ID (`wmOwnerID`), set
deliberately by the panel author via `bl_owner_id` for workspace-based UI filtering.
It is empty for the overwhelming majority of add-on panels and is not an add-on
identifier. It cannot be repurposed without breaking workspace filtering.

The reliable add-on identity is the Python class's `__module__` — its top-level
package matches the module name that `addon_utils` enables. That is Python-side data.

**Consequence**: the add-on → panels mapping is computed in Python and pushed into a
small C++ registry. This is also a benefit: Python-side we can read the extension
manifest for a proper display name, and use `addon_utils.modules()` for the picker.

---

## 2. Context resolution — why re-hosting works

The chosen approach re-hosts add-ons' **existing, unmodified** `Panel` classes. The
obvious objection is that those panels declare `bl_space_type = 'VIEW_3D'` and read
View3D context. In practice most of them are fine, for a structural reason.

Blender resolves `context.X` in layers: **screen → area → region**. Nearly everything
a typical N-panel reads is resolved at the **screen** layer, in
[`screen_context.cc`](../source/blender/editors/screen/screen_context.cc), which is
editor-agnostic and works in any area:

`scene`, `object`, `active_object`, `selected_objects`, `selected_editable_objects`,
`editable_objects`, `visible_objects`, `objects_in_mode`, `edit_object`,
`sculpt_object`, `pose_object`, `active_bone`, `active_pose_bone`, `selected_bones`,
`annotation_data`, `grease_pencil_data`, `active_operator`, …

An add-on panel doing `context.object.name` or `context.scene.my_props` works in the
new editor with **zero changes**.

### 2.1 What actually breaks, and the auto-wiring fix

Only View3D-specific lookups fail:

| Breaks | Symptom |
| :--- | :--- |
| `context.space_data` | Returns our `SpaceAddon`; `.overlay`, `.shading`, `.region_3d` fail |
| `context.region_data` | No `RegionView3D` available |
| `poll()` checking `context.space_data.type == 'VIEW_3D'` | Returns `False`, panel silently vanishes |
| `bpy.ops.view3d.*` buttons | Operator poll fails, button greys out |

**Fix — context delegation.** `SpaceAddon` installs a `SpaceType.context` callback.
For `space_data`, `region_data`, `area`, and `region`, it delegates to a real View3D
elsewhere in the screen, resolved in order:

1. the last View3D area the user interacted with, if still present;
2. otherwise `BKE_screen_find_big_area(SPACE_VIEW3D)`;
3. otherwise unresolved.

This is the same mechanism operator context-override relies on, so it is well-trodden
rather than novel. Combined with trapping `poll()` exceptions (report once to the
console, don't spam per redraw), this gives the robustness of an opt-in compatibility
flag with none of the add-on-side changes.

**Honest limitation**: if no 3D Viewport exists anywhere in the workspace, those
specific panels cannot resolve. They render a "requires a 3D Viewport" placeholder
instead of crashing or vanishing silently. This is a deliberate, documented edge —
not something the design pretends to solve.

---

## 3. Implementation plan

### 3.1 File-level breakdown

| # | Change | File(s) | Est. LOC |
| :-: | :--- | :--- | :---: |
| 1 | `SPACE_ADDON` enum value; `SpaceAddon { SpaceLink; char addon_id[128]; }` | `makesdna/DNA_space_enums.h`<br>`makesdna/DNA_space_types.h` | ~40 |
| 2 | `UserDef.addon_editors` list + RNA — persists which add-ons the user activated | `makesdna/DNA_userdef_types.h`<br>`makesrna/intern/rna_userdef.cc` | ~80 |
| 3 | New editor module: space callbacks, region init/draw, context delegation | `editors/space_addon/` *(new)* | ~250 |
| 4 | `space_subtype_get` / `_set` / `_item_extend`, incl. the `Add-on…` entry | `editors/space_addon/space_addon.cc` | ~90 |
| 5 | C++ registry: add-on id → filtered `ListBaseT<PanelType>`; RNA push API | `editors/space_addon/addon_panel_registry.cc` *(new)* | ~120 |
| 6 | Python: group `Panel.__subclasses__()` by `__module__`; picker operator; menu | `scripts/startup/bl_ui/space_addon.py` *(new)* | ~150 |
| 7 | `.blend` fallback when the add-on is absent — keep `SpaceAddon` data, show notice | `blenloader/intern/versioning_*.cc`<br>`blenkernel/intern/screen.cc` | ~60 |
| 8 | Register the new space type in the editor init table | `editors/include/ED_space_api.hh`<br>`editors/space_api/spacetypes.cc` | ~10 |

**Total**: ~600–800 LOC across 8 existing files + 1 new module directory.
(The old estimate of 850–1,200 assumed the dynamic-registration and manual-drawing
designs that §1.1 and §1.2 eliminate.)

### 3.2 Data flow

```
UserDef.addon_editors            persistent, survives .blend files
  └─ "mytool", "node_wrangler"   add-ons the user activated as editors

editor dropdown  (rna_Area_ui_type_itemf)
  └─ space_addon_subtype_item_extend()
       ├─ one entry per UserDef.addon_editors
       └─ "Add-on…"  → opens picker over addon_utils.modules()

ScrArea (spacetype = SPACE_ADDON)
  └─ SpaceAddon.addon_id = "mytool"          ← stored in DNA, not the index

window region draw
  └─ addon_panel_registry_get("mytool")      ← ListBaseT<PanelType>
       └─ ED_region_panels_layout_ex(C, region, list, …)

context lookup
  screen layer  → object / scene / mode / …   already works
  area  layer   → space_data / region_data    delegated to a real View3D
```

### 3.3 Build sequence

1. DNA + space registration skeleton — empty editor appears in the dropdown.
2. Panel registry + filtered `ED_region_panels_layout_ex` — panels actually draw.
3. Subtype dropdown entries + `UserDef` persistence.
4. Python picker operator and menu integration.
5. Context delegation callback.
6. `.blend` load fallbacks and poll-error trapping.

Each step is independently compilable and testable, and maps to one commit in the
published patch series.

---

## 4. Retained risks from the original analysis

Sections of the old document that remain valid, with adjusted severity:

- **§3.1 Add-on unregister / live reload** — *severity reduced*. No `SpaceType` is
  destroyed at runtime, so there is no dangling-`SpaceType` crash. The remaining case
  is stale `PanelType` pointers in the registry: the registry must be invalidated on
  add-on unregister, and the area must fall back to an "add-on not enabled" notice
  rather than dereferencing freed panel types.
- **§3.2 `.blend` compatibility** — *still applies*. Files saved with an add-on editor
  opened without that add-on must keep the `SpaceAddon` data intact so re-enabling
  restores the editor. Do not strip to `SPACE_VIEW3D`.
- **§3.3 Keymaps / event handling** — *still applies*, but narrower: the window region
  uses the standard panel keymap, so only add-on-specific keymap needs remain open.
- **§3.4 UI engine churn across versions** — *still applies, reduced*. We call the
  stable `ED_region_panels_*` API rather than `ui::block_*` internals, so the rebase
  surface across Blender versions is much smaller.

---

## 4a. Implementation log

Findings from actually building the thing, which differ from or extend the plan above.

### Panel types cannot be re-linked (step 2)

`ED_region_panels_layout_ex` walks its list through `PanelType`'s own `next`/`prev`
fields, and every registered type is *already* a member of its home region type's list.
Putting one into a second list corrupts the first. The editor therefore keeps a private
list of **shallow copies** in `SpaceAddon_Runtime`. This is safe because
`panel_find_by_type` matches panels to types by ID name rather than by pointer, and
sub-panels are reached through `children`, which still refers to the registered types.

Because `Panel::type` points at those copies between frames, the copies must be stable.
`BKE_paneltypes_tag_changed()` / `BKE_paneltypes_state_get()` provide a revision counter,
bumped on panel registration and removal, so the cache is rebuilt only when the add-on
changes or panel types actually change.

### The Python-push registry was unnecessary (step 2)

§1.3 concluded that the add-on → panels mapping had to be computed in Python and pushed
into C++. It does not: `makesrna` already links Python under `WITH_PYTHON`, so
`rna_Panel_register` can capture the owning module directly via the new
`BPY_class_module_name_get()`, storing it in `PanelType::addon_id`.

This removes both the C++ registry and the Python push API from §3.1, and eliminates a
class of staleness bugs, since the mapping is written at registration time and cannot
drift.

### Region layout and drawing must be separate callbacks (step 2)

**Symptom**: all widgets in the editor rendered at roughly 0.75 of their normal size.

**Cause**: `art->draw` performed both layout and drawing. Blender runs the layout pass
separately and earlier; it is what computes the region size and the View2D `tot`/`cur`
rectangles from the laid-out content. Drawing therefore used bounds derived from the
previous frame, and since panel widths follow those bounds, everything laid out small.

**Fix**: split them, as the Properties editor does.

```cpp
art->layout = addon_main_region_layout;  /* ED_region_panels_layout_ex */
art->draw = ED_region_panels_draw;       /* drawing only */
```

**Verified**: widget metrics were measured in pixels after the fix and match the rest of
the UI. Two other changes landed in the same build — `RGN_FLAG_INDICATE_OVERFLOW` was set,
and `keymapflag` changed from `ED_KEYMAP_UI | ED_KEYMAP_VIEW2D` to
`ED_KEYMAP_UI | ED_KEYMAP_FRAMES` — so the individual contribution of each is not
isolated, but the result is correct and needs no further investigation. Both are
independently justified: the Properties editor sets the same flags, and
`ED_KEYMAP_VIEW2D` would install view zoom bindings in a region whose zoom stays locked
at 1.0.

Note that `art->prefsizex` is left at 0, which makes `ED_region_panels_layout_ex` choose
`em = 20` rather than the `em = 10` used when it is set. Since measured metrics are
correct, this is the intended configuration for a full-area panel list rather than a
narrow sidebar, and should only be revisited if a specific add-on lays out badly.

### Panel `poll()` is the real compatibility limit (step 2)

Confirmed against the bundled Node Wrangler add-on, whose panel inherits:

```python
return (space.type == 'NODE_EDITOR' and space.node_tree is not None ...)
```

The panel is collected correctly but polls `False`, so the editor draws empty. Panels
with no space-specific `poll()` draw perfectly, including sub-panels and screen-level
context such as `context.object`.

**This moves context delegation (§2.1) earlier in the build sequence.** Shipping the
picker first would let users select add-ons that then render nothing, and Node Wrangler
shows this affects bundled add-ons, not just edge cases.

### Context delegation had to be global, not per-call (step 3)

§2.1's original mechanism — a `SpaceType.context` callback — was wrong. `CTX_wm_space_data`
reads `area->spacedata.first` directly with no hook point
([context.cc:959-963](../source/blender/blenkernel/intern/context.cc#L959-L963)), so a
context callback cannot intercept it.

Delegation went through three widening stages as each one turned out to be insufficient:

1. **Layout-scoped.** Swap the context's area/region for a real editor of the panel's
   declared type, only around `ED_region_panels_layout_ex`. This made panels draw, but
   any menu opened from a panel (drawn later, in its own popup pass) or any operator poll
   run at button-press time saw the un-delegated `SpaceAddon` again and raised.
2. **Global, DNA-backed.** `SpaceAddon::delegate_spacetype` records the borrowed editor
   type. `ctx_wm_area_effective()` in `context.cc` resolves it by type (never by stored
   pointer, so closing the borrowed editor cannot dangle) and all 18 typed space
   accessors (`CTX_wm_space_node`, `CTX_wm_view3d`, ...) route through it. This fixed
   menus and C-side operator polls.
3. **Context members.** Editors also supply members like `selected_nodes` through their
   own `SpaceType.context` callback, a separate mechanism from `space_data`. A new
   `addon_context()` forwards unresolved lookups to the delegate editor's own callback,
   which itself resolves through step 2's accessors and therefore operates on the
   borrowed editor's real data.

**Deliberately not solved**: modal operators (transform, `NODE_OT_attach`) additionally
need the *region* itself at invoke time, not just space/context lookups — panel drawing
needs this area's own region, an invoked modal operator needs the delegate's, and the two
cannot both be `CTX_wm_region`. Deferred; such buttons currently misbehave (logged
`convertViewVec: called in an invalid context` spam) rather than being blocked. If
revisited, the fix is scoping the region swap to operator invocation specifically, not
widening the existing accessor.

**The limitation is narrower than "modal operators break" — it's specifically operators
that need the region's live view/projection state, not modal operators in general
(2026-08-18).** Checked against two real operators, one on each side.

*Breaks, confirmed by reading the actual failure point.* `TRANSFORM_OT_translate` /
`.rotate` / `.resize` (Blender's own G/R/S tool) - `convertViewVec()`
([transform.cc:185-231](../source/blender/editors/transform/transform.cc#L185-L231)),
for the `SPACE_VIEW3D` branch, calls
`ED_view3d_win_to_delta(t->region, xy_delta, t->zfac, r_vec)` - this needs `t->region`'s
actual `RegionView3D` (the real viewport's live view/projection matrices) to turn a 2D
pixel delta into a correctly-scaled 3D-space movement: the same 10-pixel mouse move has
to translate an object by a different real-world distance depending on zoom and camera
distance, and that scaling factor only exists on the real region. When `t->region`/
`t->spacetype` don't resolve to a genuine `SPACE_VIEW3D` region, execution falls to the
`else` branch and prints exactly the logged spam named above -
`"%s: called in an invalid context\n"`. Not an obscure case: any hosted add-on's "Move
Selected" / nudge / duplicate-and-move button calling `transform.translate` under the
hood hits this.

*Works, confirmed by reading the add-on's actual source.* The installed DreamUV add-on's
`view3d.dreamuv_uvscale`
(`scripts/addons/DreamUV-master/DUV_UVScale.py`, both `invoke()` and `modal()`) never
reads `context.region`, `context.space_data`, `context.region_data`, or calls any
`region_2d_to_*`/`ED_view3d_win_to_*`-style conversion. It computes everything from raw
window-space `event.mouse_x`/`event.mouse_y` deltas plus direct bmesh UV-loop edits -
architecturally a "mouse-delta-as-a-slider" tool, the same shape as a custom
drag-to-adjust-a-float operator. It never asks the region what its projection state is,
so which region the click landed in is irrelevant to its correctness. Confirmed working
when hosted, exactly because it never touches the piece this editor doesn't correctly
delegate at invoke time.

**Consequence for any future modal-operator handling (§8/9 of the punch list).** A
detection rule based purely on `wmOperatorType::modal != nullptr` is too coarse - it
would flag DreamUV's scale tool alongside Transform, even though only one of them
actually breaks. The real distinguishing signal (does the operator's `invoke()`/`modal()`
body read region/view-space state) isn't something detectable statically without reading
the operator's own code, which is exactly the class of problem the plan's "dynamic
context routing" discussion (§6) already named as unreliable to solve generically. This
doesn't change the already-confirmed "block always" direction for the warning feature -
if anything it argues for keeping detection coarse (block on `ot->modal != nullptr`) and
accepting the false positives, rather than attempting a finer static classifier that
can't actually be built reliably.

**How a vanilla add-on already solves the same problem, and what that does and doesn't
give us for free (2026-08-18).** The real add-on answer is `bpy.context.temp_override(
area=..., region=...)`, wrapped by the author around one operator call - e.g. a
Properties-panel button that nudges the 3D viewport. Traced the actual C call chain to
confirm the scoping is sound even though the drag continues after the `with` block exits:
`WM_operator_call_py` -> `wm_operator_call_internal`
([wm_event_system.cc:2002](../source/blender/windowmanager/intern/wm_event_system.cc#L2002))
invokes `ot->invoke()` and, for `RUNNING_MODAL`, registers the modal handler and returns
immediately - it does not block for the drag. `WM_event_add_modal_handler`/
`WM_event_add_ui_handler` capture `handler->context.area`/`region` from `CTX_wm_area(C)`/
`CTX_wm_region(C)` at registration time, synchronously, while the override is still
active - a one-time snapshot ("frozen screen context for modal handlers", per the WM
source's own comment), not a live read. So a `temp_override` scoped to just the call is
sufficient for the operator's entire lifetime, for the same reason our own layout-time
`CTX_wm_area_set`/`CTX_wm_region_set` swap only needs to bracket one call.

Two things worth separating precisely, since they answer different questions:

- **The primitive itself is not Python-exclusive.** `CTX_wm_window_set`/`_screen_set`/
  `_area_set`/`_region_set` are plain C++ functions; `temp_override()` is a Python-facing
  convenience wrapper around them, not a capability that only exists in Python.
  `addon_main_region_layout` already calls two of the four directly, with no Python
  involved. A C++-native equivalent of the full four-step sequence would be a dozen
  lines, not a new mechanism.
- **What's actually missing is the interception point, not the primitive.** A Python
  author using `temp_override` writes the `bpy.ops.xxx()` call themselves, so they choose
  exactly where to wrap it. A button in a re-hosted panel is invoked by Blender's own
  generic dispatch (`interface_handlers.cc`'s `ui_apply_but_operator` ->
  `wm_handler_operator_call`), which has no concept of "this button lives in a re-hosted
  foreign panel" and no existing hook to insert anything before invocation. This is the
  same missing piece the modal-operator-warning feature (§punch list item 8) already
  needs solved first - a post-layout walk of the region's `uiBlock`/`uiBut` lists,
  rebinding qualifying buttons to a small wrapper. Building either the warn-and-block
  version or an actual swap-and-fix version requires that same hook; only what the
  wrapper *does* once installed differs.

**Why this has to be solved on our side, and can't be expected of the hosted add-ons.**
`temp_override` is an escape hatch an author reaches for deliberately, only when
knowingly doing something unusual. The overwhelming majority of operators never need it
not because Blender solved cross-editor invocation generally, but because of a structural
guarantee vanilla Blender provides for free: a panel only ever draws inside the editor
type it declared, so `context.region`/`context.space_data` are always correct by
construction - there was never a mismatch to paper over. This editor breaks that
guarantee systematically, for arbitrary unmodified panels whose authors never anticipated
running anywhere but their declared editor and never had a reason to write any
accommodation. DreamUV's scale tool works under us by accident of its own design (no
region dependency at all), not because its author thought about being hosted elsewhere.
If this is ever fixed, it has to happen automatically, on our side, for every qualifying
button - effectively doing on the add-on's behalf, without its knowledge, what a
responsible author would have manually written had they ever anticipated this. There is
no version of "the ecosystem already solved this" to lean on; the ecosystem was never
asked to.

**One simplification this fix would have over the warning feature.** Applying the scoped
swap unconditionally to every button in a hosted panel is harmless when unneeded (DreamUV
would just get correctly-delegated context it never asked for) and correct when needed
(Transform). Unlike the warning feature, this sidesteps the `ot->modal != nullptr`
false-positive problem entirely - no classifier needed, just always wrap using the
delegate the panel's own collection already resolved it against (the same data item 9,
per-panel delegate resolution, already needs - solving 9 would hand this feature most of
what it needs for free).

### Delegate-availability filtering, and its own cache-invalidation gap (step 3)

Confirmed against the bundled Cycles add-on: its Node-Editor-cloned panels
([ui.py:82-92](../build_windows_x64_vc17_Release/bin/Release/5.3/scripts/addons_core/cycles/ui.py#L82-L92))
read `context.material`/`light`/`world` directly, with no defensive `getattr` - they are
written assuming they only ever run inside a real Node Editor. With no Node Editor open
anywhere, `addon_context` correctly reports the member as unavailable, but the add-on's
own `poll()` doesn't handle that gracefully and raises.

Fix: `addon_panel_types_collect` now skips a panel at collection time if no editor of its
declared type is open anywhere, so `poll()` is never called into a context already known
to be unsatisfiable. This is a second, independent reason a panel might not draw, so the
main region now explains *why* it's empty (no add-on chosen / add-on has no matching
panels / needs an editor that isn't open) instead of rendering blank.

This filter introduced a subtler bug worth recording: since inclusion now depends on
*which editors are open elsewhere in the screen*, and the existing cache only invalidated
on add-on change or panel (un)registration, opening or closing an editor wouldn't refresh
the hosted panel list. Fixed with a cheap screen-layout signature (bitmask of open space
types) folded into the cache key.

### The editor-type dropdown is a curated list, not "every enabled add-on" (step 4)

The original plan's cheap derivation — list every add-on that happens to have panels
registered — shipped first, then was replaced after review: it made the menu grow and
shrink as unrelated add-ons were toggled, with no way to remove an unwanted entry. Built
instead: `UserDef.addon_editors` (new `bAddonEditor` list, mirroring the existing `bAddon`
pattern) is a persistent, user-curated list. The editor dropdown gets an "Add-ons" heading
and a first entry, "Add an Add-on...", opening a search popup
(`ADDON_OT_pick_and_host`) over installed add-ons; picking one adds it to the curated list
and hosts it immediately. The same list is manageable from Preferences → Add-ons via a new
panel and UIList, so curation isn't dropdown-only.

Three bugs caught during this change, before they shipped:

- **Sentinel value.** The "Add an Add-on..." entry's sub-type value was first `-1`. Wrong
  twice over: `ScrArea::butspacetype_subtype == -1` is already Blender's own reserved
  sentinel meaning "not yet determined, call `space_subtype_get()`"
  ([area.cc:2952](../source/blender/editors/screen/area.cc#L2952)), so `-1` would never
  have reached our `set` callback at all; and `-1`'s bit pattern is unchanged by the
  `(space_type << 16) | subtype` packing the dropdown uses, which would have decoded back
  to an invalid space type and corrupted the area rather than failing safely. Fixed to
  `0x7FFF` - the maximum value the `short` sub-type field can hold, verified against both
  the packing and unpacking code before use.
- **Extension add-on identity.** `BPY_class_module_name_get` originally kept only the
  first dot-segment of `__module__`. Extensions (Blender's newer package system) import as
  `bl_ext.<repository>.<addon>...`, so every extension-installed add-on would have
  collapsed to the single identity `"bl_ext"` - then been filtered out entirely by the
  `bl_` prefix exclusion, silently hiding every extension add-on from the picker. Fixed to
  keep three segments specifically for that prefix.
- **`invoke_search_popup` return value.** Returns `None`, not an operator result set;
  `invoke()` must call it and separately return `{'RUNNING_MODAL'}`. Also requires
  `bl_property` naming which `EnumProperty` to search - omitting it fails at the same
  point with a different, easily-confused error (`"has no enum property set"`).

### Startup crash from a stale `userpref.blend` mid-development (not a shipping bug)

Hit while iterating on `UserDef.addon_editors`: Blender crashed on startup with an access
violation, `strlen()` reading invalid memory inside `addon_ids_get()`
(`space_addon.cc`), reached from `rna_Area_ui_type_itemf` while drawing an
unrelated editor's header (any header calling `template_header()` enumerates every
sub-type extender, including ours, regardless of which area is being drawn).

**Cause**: this fork's `UserDef` struct shape changed across several rebuilds in one
session (`active_addon_editor_index` and its padding landed in a separate rebuild from
`addon_editors`, and one of those rebuilds hit the DNA alignment bug described above,
before being fixed). The dev machine's single persistent
`%APPDATA%\Blender Foundation\Blender\5.3\config\userpref.blend` was saved by one of
those intermediate shapes and read back by a later one; the newly-added
`bAddonEditor` list's memory came back uninitialized rather than zeroed, so
`bAddonEditor::module` was garbage, and treating it as a C string crashed.

**Fix**: moved the file aside (`userpref.blend.bak_pre_addon_editors`, not deleted -
preserved in case anything else in it is worth recovering by hand). Confirmed the crash
does not reproduce once Blender falls back to freshly-built defaults.

**Not a defect a real install would hit.** This only occurs from repeatedly reshaping a
persisted struct against one real preferences file mid-development. A fresh install of
this fork has no old-shaped `userpref.blend` to read. Recorded here so a future
"crashes on startup right after a `UserDef`/DNA change" incident is fast to diagnose:
check whether `%APPDATA%\...\config\userpref.blend` predates the change, move it aside,
relaunch.

**Recurred once** (`bAddonEditor.name` added in a later commit than the one that last
regenerated `userpref.blend`, so the same struct-growth-vs-persisted-file gap opened
again). Identical signature and fix. This is expected to keep recurring for the rest of
this fork's active `UserDef` work - every field added mid-session invalidates whatever
`userpref.blend` was last saved before it - and is not itself a defect each time; noted
here so it isn't re-diagnosed as a new bug. A standing option to remove the recurrence
entirely: launch with `--factory-startup` while `UserDef`'s shape is still moving, so no
persisted file is ever in the loop until it stabilizes. Not adopted as a rule yet.

### Extension display names (step 4, post-crash)

Reported after the picker redesign: entries for installed Extensions showed their raw
module id in the editor-type dropdown - `bl_ext.<repository>.<addon>` - rather than a
human-readable name. Legacy add-ons were unaffected (their module id, e.g.
`node_wrangler`, already reads as a name), which is why this wasn't caught earlier: the
existing test coverage exercised `cycles`/`node_wrangler`/`pose_library`, none of which
are Extensions.

**Cause**: `addon_space_subtype_item_extend` set both the enum item's identifier *and*
its display label from the same string - the module id, needed for `set`/`get`
round-tripping (`area.ui_type = "cycles"` must keep working from Python). There was
never a separate display string.

**Fix**: `bAddonEditor` gained a `name` field, captured once via
`addon_utils.module_bl_info()` when the picker operator adds the entry - not resolved
live at draw time, since only Python can resolve `bl_info`/manifest data and the
drop-down is built in C. `addon_ids_get()` returns `id`/`label` pairs; the identifier
(used for matching) stays the module id, only the displayed text changes. Sorting moved
from module-id order to label order, since that's what's actually visible in the menu.

**Known limitation, accepted rather than solved**: the name is a snapshot from add-time,
not re-resolved if the add-on's declared name later changes (e.g. an extension update).
Consistent with the curated-list design already accepting "remove and re-add" as the
correction path for a stale entry generally.

### Real-world add-ons surfaced two more architecture gaps (post-picker)

Testing against actually-installed extensions (ucupaint, Texture Manager) rather than
the three bundled add-ons used so far surfaced two distinct issues, worth separating
because only one is a bug in this fork.

**`PropertyGroup.name` is read-only by design - an add-on bug, not ours.** ucupaint's
`Layer.py` does `CollectionProperty(type=bpy.types.PropertyGroup)` and then
`.add().name = uv.name`. The base `PropertyGroup.name` RNA property is deliberately
non-editable
([rna_ID.cc:1782-1789](../source/blender/makesrna/intern/rna_ID.cc#L1782-L1789)); the
property's own `ui_text` says as much: *"can be re-defined in Python sub-classes if
needed"*. The add-on never subclasses it. Confirmed by diff that `rna_ID.cc` is untouched
by this fork - this fails identically against unmodified upstream Blender.

**Mixed-editor add-ons break the single, area-wide delegate - a real gap in this fork.**
ucupaint registers panels for both `VIEW_3D` and `NODE_EDITOR`
(`grep bl_space_type ui.py`). `addon_context_delegate_find` resolves *one*
`delegate_spacetype` per area, from whichever panel's declared type matches an open
editor first. Whichever wins is then applied to *every* panel's context resolution for
that area - so a `NODE_EDITOR`-only panel polled while the delegate happens to be
`VIEW_3D` sees a `SpaceView3D` where it expects a `SpaceNodeEditor`, and its own
unguarded attribute access (`context.space_data.tree_type`) raises, exactly like the
`PropertyGroup` case above but for a different, our-side reason.

**Not fixed yet, recommended approach recorded for when it is**: resolve the delegate
*per panel*, from that panel's own declared `bl_space_type`, rather than once per area.
This is deterministic (every panel already declares what it needs) and requires no
heuristics - see the dynamic-context-routing discussion below for why a more general
"probe every open editor and see what makes `poll()` pass" approach is deliberately
being held back as a fallback layer rather than the primary mechanism.

### Should context routing be dynamic, resolved per-operator-call?

Raised as a design question, not yet built. Two things worth recording distinctly: what's
actually feasible, and the safety property worth deliberately preserving.

**Feasibility.** There is no way to inspect what context an operator's `poll()`/`execute()`
needs without running it - Blender has no manifest for this, and static analysis of
arbitrary Python (or C) is unreliable enough to actively distrust for this purpose (can't
follow into helpers, can't handle conditional access, a false negative reproduces exactly
the class of crash this would exist to prevent). The only feasible mechanism is reactive:
try a candidate delegate, see if `poll()` succeeds, otherwise try the next. That is a
strict superset of "resolve per panel from its own declared `bl_space_type`" (above) -
which already gets correctness in the overwhelmingly common case, deterministically, since
a panel's own declaration *is* a reliable statement of what it needs in virtually every
real add-on. Probing would only add value where a panel's `poll()` needs something other
than what its own `bl_space_type` declares, which is itself unusual enough to treat as an
edge case worth a fallback layer, not a redesign of the primary mechanism.

**The safety property to hold.** Delegation must only ever resolve to a real, currently
open, on-screen editor, via `BKE_screen_find_big_area`
([space_addon.cc:242](../source/blender/editors/space_addon/space_addon.cc#L242)). No
call in this design fabricates or reconstructs context without a visible backing editor -
if nothing matches, delegation returns null and the operator polls false, same as today.
This directly addresses the failure mode raised when the question was posed (an action
running against state the user has no visible editor to observe or correct) - and the
boundary is exactly at "only real, visible editors, never synthesized state." Extending
delegation to probe multiple *real* open editors doesn't cross that line; inventing a
plausible-looking context without one would. An add-on's own deliberate offscreen
automation (`bpy.context.temp_override(...)`) is a separate, already-solved mechanism the
add-on controls itself and this design does not need to interact with.

### Conventional panels outside RGN_TYPE_UI (implemented)

Properties-tab-style add-ons (Texture Manager: one panel, `bl_space_type = "PROPERTIES"`,
`bl_region_type = "WINDOW"`, `bl_context = "scene"`) were invisible to the picker, not
broken - `addon_panel_types_collect` only scanned `RGN_TYPE_UI` regions. Relaxed to also
scan `RGN_TYPE_WINDOW`, which required no other change: `ED_region_panels_layout_ex`
doesn't care what region type a `PanelType` was originally registered under.

**Accepted limitation**: Properties-style panels are normally tab-switched by `bl_context`
via the `contexts` parameter to `panel_add_check`, which this editor passes as `nullptr`
(no filtering). A `bl_context`-heavy add-on would show every tab's panels flattened into
one stack rather than the native tabbed view. Not addressed here; the existing
`bl_category`-based tab mechanism (already active for N-panels, inherited for free from
`ED_region_panels_layout_ex`) is the natural place to bridge this later, by synthesizing
category grouping from `bl_context` when a panel has no `bl_category` of its own.

### Header showed the raw add-on id, not its display name (fixed)

`ADDON_HT_header.draw()` fell back to `layout.label(text=space.addon_id)` when nothing
else was drawn - written before `bAddonEditor.name` existed, and never updated once it
did. Harmless for legacy add-ons, whose module id already reads as a name
(`node_wrangler`); for extensions `addon_id` is the full `bl_ext.<repository>.<addon>`
import path, which is what was actually showing, right-aligned by the preceding
`separator_spacer()`. Every other surface (dropdown entries, Preferences UIList) already
resolved the curated `.name` correctly - only the header's own fallback label was missed.
Fixed by looking up the curated `bAddonEditor` entry matching `addon_id` and showing its
`name`, falling back to `addon_id` only if no curated entry exists.

### Horizontal panel layout: scoped as a separate, large feature, not attempted here

Requested: let a hosted add-on's collapsible sections arrange left-to-right instead of
top-to-bottom, with an aspect-ratio-based default and a manual override button.

Assessed and deliberately not started. `ED_region_panels_layout_ex` has no horizontal
concept anywhere in it - panels accumulate strictly by Y-offset at a fixed width, and
that assumption is load-bearing throughout: collapse/expand height bookkeeping,
drag-reorder, and the View2D scroll lock (X locked, Y free, set identically in every
panel-list region in Blender including this editor's own
`addon_main_region_init`/`ED_region_panels_init`). True horizontal columns means forking
that layout function's internals, or reimplementing panel headers, collapse state, and
drag interaction independently - a larger undertaking than everything else in this plan
combined, and one with an ongoing cost specifically at odds with staying a rebasable
patch series: every upstream change to panel layout internals would need re-porting.

**Cheaper adjacent option, using only existing mechanisms**: the category-tab system
already active in this editor (inherited for free, the same one N-panels use for
`bl_category`) gives one-section-at-a-time navigation via edge tabs - not simultaneous
side-by-side columns, but a real answer to "many collapsible sections are unwieldy as one
long scroll," buildable without touching panel layout internals at all. The
aspect-ratio-adaptive default and manual override are separable from the layout question
entirely and cheap regardless of which direction is chosen.

Recorded as a candidate future item, intentionally out of scope for the current plan.

### Supported-editor info: header button when drawn, in-region block when empty (implemented)

Simple version of the "open the editor for me" idea, deliberately without the
split-and-collapse mechanic (that part stays a candidate future item, not built): show
which editor types a hosted add-on's panels are written for, as a header info button
when at least one panel is drawing, or as an in-region information block when none are.

**Single source of truth, in Python, used by both.** `_addon_supported_spaces(addon_id)`
in `space_addon.py` walks `bpy.types.Panel.__subclasses__()` filtered to the add-on's
module (matched by prefix, mirroring the C side's attribution rule), collects the
distinct `bl_space_type` values of its top-level `UI`/`WINDOW` panels, and resolves each
to Blender's own display name via `bpy.types.Area.bl_rna.properties["type"].enum_items` -
reusing Blender's existing curated names rather than hand-maintaining a second copy.
Deliberately not computed in C++ and exposed via new RNA: both consumers are already
Python-side (header, and the panel below), so crossing the language boundary would only
add plumbing without moving where the answer is needed.

**Header button** (`ADDON_OT_supported_editors_info`, drawn next to the editor-type
selector): an inert `INTERNAL` operator whose `description()` classmethod returns the
joined list as its tooltip - the standard Blender idiom for a hover-only info affordance,
the same mechanism Blender's own disabled-button tooltips use to explain why.

**Caught before shipping: `bpy.types.Region.panels` does not exist.** The first version
of the button-vs-block condition checked `len(region.panels) > 0` from Python, assumed
by analogy with `region.panels[0].is_open`-style scripting patterns seen elsewhere -
wrong; no such RNA property is exposed on `Region` anywhere in this codebase (confirmed
by grep against `rna_screen.cc`/`rna_ui.cc`), so this would have raised an
`AttributeError` the moment the header ever drew, i.e. always. Caught in headless
testing before reaching the user. Replaced with `_addon_has_open_delegate()`, which
checks whether any editor type the add-on's panels need is currently open in the screen
(`{area.type for area in context.screen.areas}`) - not a guarantee any specific panel
will draw (an individual `poll()` can still fail for unrelated reasons), but the same
signal that decides whether delegation can find anything at all, and the best one
available from Python without exposing new state from the C++ side. Shares its panel/
space-type filtering with `_addon_supported_spaces()` via one extracted helper,
`_addon_top_level_panel_space_types()`, so the two cannot silently drift apart. Mutually
exclusive with the block below by design.

**In-region block, replacing the old single-line placeholder.** The previous mechanism
(`SpaceAddon_Runtime::missing_spacetype`, a single value, hand-drawn via raw `BLF` calls
in `addon_main_region_draw`) is removed entirely, not extended: a single value could not
express "needs one of several editor types," and the raw-text drawing path duplicated
logic that already had to exist in Python for the header button. Replaced with
`ADDON_PT_empty_state`, an ordinary Python `Panel` (`bl_space_type = 'ADDON'`,
`HIDE_HEADER`) that C++ injects into the collected panel list precisely when that list
would otherwise be empty (`addon_panel_types_collect`, after normal collection, via
`addon_empty_state_paneltype_find()` looking it up by idname from this editor's own
native `RGN_TYPE_WINDOW` panel registry - never reached any other way, since this
editor's region always lays out `SpaceAddon_Runtime::paneltypes`, not the region type's
own native list). Drawn through the ordinary `ED_region_panels_draw` path like any other
panel, so `addon_main_region_draw`'s custom draw wrapper is gone too -
`art->draw = ED_region_panels_draw` directly, same as before the placeholder was added.

Net effect on `space_addon.cc`: smaller than before this change, despite doing more -
the removed BLF/`fmt`/`TIP_` drawing code and its supporting field outweighed the small
injection helper added. `bf::blenfont` dropped from `CMakeLists.txt` accordingly, nothing
in the file uses it anymore.

**Wording, after review.** The block's message was rewritten to match a specific
requested phrasing and formatting: an explicit two-line lead-in
("This add-on's panels require one of the following / editor types to be present in the
workspace:") followed by a blank separator and one bulleted line per editor name, using
`•` rather than a literal `-` for cleaner rendering in Blender's UI font. One follow-up
was raised and then explicitly withdrawn: whether `IMAGE_EDITOR` should display as two
separate entries, "UV Editor" and "Image Editor", since UV editing is a *mode* of the
Image Editor rather than a distinct `Area.type` - Blender has only the one identifier,
displayed as "UV/Image Editor". A split-display override was drafted, then reverted at
the user's request once this was confirmed intentional, not a bug: they are the same
editor, and showing Blender's own combined name is correct. Recorded so this isn't
re-litigated - the override point (in `_addon_supported_spaces()`, next to the
`type_enum.get(space_type)` lookup) is still the right place if a similar split is ever
wanted for a specific, deliberately-chosen editor.

**Line break clarified.** The two-line lead-in noted above is a deliberate hard split -
two separate `col.label()` calls, not one label wrapped by a narrow region.
`UILayout.label()` never auto-wraps in Blender; it clips with an ellipsis on a narrow
region instead, so a mid-sentence break always means two calls, at exactly the point
chosen in code, regardless of area width.

**Preferences panel description.** `USERPREF_PT_addon_editors`'s description referenced
"Add an Add-on..." inline with no visual separation from the surrounding sentence.
`layout.label()` has no rich-text/markup support - no way to bold a substring within one
call - so quotation marks are used instead: `via "Add an Add-on..." in any area's...`.

### Disabled add-ons hidden from both the picker and the curated dropdown (implemented)

Two related, separately-triggered fixes, both driven by the same underlying rule:
disabling an add-on unregisters its classes, so an entry for it can never draw anything
right now - listing it anywhere invites picking a dead end.

**Curated dropdown** (`addon_ids_get()` in `space_addon.cc`): entries in
`UserDef.addon_editors` are now filtered through `addon_has_registered_panels()`, a
lightweight existence scan (does at least one currently-registered top-level `PanelType`
have this `addon_id`) before being returned to `get`/`set`/`item_extend`. Deliberately
not routed through Python (`addon_utils.check()`) despite being conceptually an
"is it enabled" question: the existence check is already exactly what
`addon_panel_types_collect` does for the active add-on, reuses the same attribution
mechanism, and needs no cross-language call for something C++ can already answer from
data it already has.

**Accepted cosmetic tradeoff.** `get`/`set`/`item_extend` must share one filtered list -
enum values are indices into it, so filtering only the visually-drawn list while leaving
`get`/`set` unfiltered would desync the index a click maps to from what is actually
shown. Consequence: if the area currently hosting a since-disabled add-on has that
add-on hidden from the (now filtered) list, `addon_space_subtype_get` cannot find it and
falls back to index 0, so the dropdown's "current selection" highlight can show the
wrong entry. `SpaceAddon::addon_id` itself is untouched by this - the region's own
content resolution is independent of the curated list and remains correct (still
correctly explains "no panels to show" via `ADDON_PT_empty_state`). Judged not worth
solving: the highlight is cosmetic, and the situation ("you are looking at something you
just disabled") is inherently a momentary, self-correcting one.

**Picker** (`_installed_addon_items()` in `space_addon.py`): filtered separately, by
`addon_utils.check(module_name)[1]` (`loaded_state`). Needed in addition to the dropdown
fix above, not instead of it: picking a disabled add-on from the picker would add a
curated entry that the dropdown filter then immediately hides anyway, a confusing
no-feedback dead end. Verified specifically against installed Extensions (not just
legacy add-ons), since disabled Extensions were the concrete case reported - all five
installed on the dev machine were correctly excluded once disabled, `ucupaint` and
`texture_manager` included.

### Blank area-type button icon when the curated list is empty (fixed)

Reported alongside the picker screenshot: with no curated add-ons yet, the collapsed
area-type selector button showed no icon at all, rather than a neutral placeholder.

**Cause**: `addon_space_subtype_get()`'s fallback (nothing selected, or the selected
entry was filtered out) returned `0`. `RNA_ENUM_ITEM_HEADING(name, description)` expands
to `{0, "", 0, name, description}` - value `0`, icon `0`. Both get OR'd with the same
`SPACE_ADDON << 16` by `rna_Area_ui_type_itemf`
([rna_screen.cc:223-227](../source/blender/makesrna/intern/rna_screen.cc#L223-L227)), so
the fallback's packed value was indistinguishable from the "Add-ons" heading's. Whatever
Blender-core logic resolves "which item matches the current value, to draw its icon on
the closed button" landed on the heading - icon `0`, i.e. blank.

**Fix**: the fallback now returns `ADDON_SUBTYPE_PICK` (the reserved `0x7FFF` sentinel
already used for the "Add an Add-on..." entry) instead of `0`. That entry has a real
icon (`ICON_ADD`) and is guaranteed collision-free (chosen and verified against the
packing scheme when first added, see the sentinel-value bug earlier in this log). Same
fix incidentally improves the "wrong entry highlighted" cosmetic tradeoff noted just
above: a since-disabled hosted add-on now shows the neutral pick state rather than
falsely claiming a different, unrelated add-on is selected.

### Bundled add-ons: opt-in preference, not a hidden filter

A screenshot of the picker showed only Blender's own bundled tooling (Cycles, Pose
Library, format importers/exporters) as options, and none of them appeared to draw
anything when picked. First instinct was to filter these out of the picker entirely, the
same way panel-less and disabled add-ons already are. Checked before building that:
`CyclesButtonsPanel` gates on `COMPAT_ENGINES = {'CYCLES'}`
([cycles/ui.py:60-64](../build_windows_x64_vc17_Release/bin/Release/5.3/scripts/addons_core/cycles/ui.py#L60-L64))
- its panels only draw when Cycles is the *active render engine*, unrelated to which
editor is open. Pose Library has its own scene-state `poll()` similarly. These are
legitimate, working `poll()` conditions, not bugs - confirmed directly by testing (the
user manually switched the render engine to Cycles and the panels drew correctly).

Filtering them out permanently would have been the wrong fix: it discards real,
functioning capability for anyone who *does* want to host Cycles or Pose Library, over a
condition (which render engine happens to be active) this fork has no business
predicting - see the dynamic-context-routing discussion below for why guessing `poll()`
outcomes is deliberately avoided elsewhere in this design too.

**Resolved as an opt-in preference instead**: `Preferences.show_addon_editor_bundled`,
off by default (picker stays dead-end-free out of the box), with a checkbox in
`USERPREF_PT_addon_editors`. The underlying distinction is not "will this draw right
now" (unanswerable without predicting `poll()`) but "is this Blender's own bundled
tooling or something the user installed" - a simple, deterministic, path-based fact:
`_addon_is_bundled()` checks whether the module's `__file__` contains `addons_core`
(where every bundled add-on lives) versus a user's Extensions or legacy add-ons
directory (confirmed via direct inspection: `cycles`/`pose_library`/`io_scene_gltf2`
resolve under `.../5.3/scripts/addons_core/...`; installed Extensions resolve under
`%APPDATA%\...\extensions\<repository>\...` and always import as `bl_ext.*`, never
`addons_core`). No `poll()` involved anywhere in the filter.

**DNA cost: none.** Rather than a new `UserDef` field (a struct-shape change, and this
session has already hit the stale-`userpref.blend` crash from exactly that three times),
the flag reuses an existing spare bit: `eUserpref_UI_Flag2`'s `USER_UIFLAG2_UNUSED_2`,
renamed to `USER_ADDON_EDITOR_SHOW_BUNDLED`, same bit position. `uiflag` itself (the
field a code comment recommends for new flags) turned out to be fully saturated at
32/32 bits before this fork ever touched it, so `uiflag2`'s explicitly-reserved spare
bits are the only place left to add a flag without changing struct layout at all - and
with no layout change, there is no read/write struct-size mismatch to worry about in
either direction, unlike every previous `UserDef` change this session.

**Residual risk, worth naming rather than glossing over**: if a future upstream Blender
version claims this same bit for an unrelated flag of its own, rebasing onto that commit
will produce a visible, ordinary merge conflict on that one enum line - loud and
resolvable (rename to a different still-free bit), not silent corruption. `uiflag2`'s
other two spare bits carry historical annotations (`/* dirty */`, `/* Not cleared! */`)
suggesting past reuse baggage; `UNUSED_2` carries neither, the best available signal
(short of a full history audit) that it has stayed genuinely unused.

### Context delegation made generic, and panel attribution moved out of core (2026-08-05)

Raised as a design goal: get back to the narrower version of "keep the core untouched" -
not zero changes outside `editors/space_addon/`, but zero changes that teach shared code
the name of this specific editor. Two spots failed that bar; both are fixed now, and
neither changed the feature's behaviour, only where the same logic lives.

**Context delegation.** `ctx_wm_area_effective()` in `context.cc` used to branch on
`area->spacetype != SPACE_ADDON` before reading `SpaceAddon::delegate_spacetype` - the
concrete form of the coupling `code_review.md` §3 flagged as the branch most likely to
draw upstream pushback. The field moved to a new, generic `ScrArea::context_delegate_spacetype`
(replacing 2 bytes of existing padding - no struct size change), and the function now
reads it directly with no spacetype check at all. `blenkernel` no longer mentions
`SPACE_ADDON` anywhere except `CTX_wm_space_addon()`'s own body, which is the same
one-line pattern every other typed accessor (`CTX_wm_space_node`, `CTX_wm_view3d`, ...)
already uses for its own type - not special-cased delegation logic, just the ordinary
shape of a typed accessor. Nothing else changed: same resolution order (current area,
unless it declares a delegate; resolve by type via `BKE_screen_find_big_area`; fall back
to the area itself if nothing matches), same fallback semantics, same three call sites in
`space_addon.cc` that set and read it - only the field's owner and the accessor's
knowledge of who uses it.

**Panel attribution.** `PanelType::addon_id` used to be populated by a hook added to
`rna_Panel_register` in `makesrna/intern/rna_ui.cc`, calling `BPY_class_module_name_get()`
for *every* panel registered anywhere in Blender - a cost paid by the whole panel system
for one editor's own bookkeeping, and the actual reason §1.3's "the Python-push registry
was unnecessary" conclusion (§4a, earlier) still left a core-wide touch behind even though
it removed the cross-language plumbing. The field and the registration-time hook are both
gone. `space_addon.cc` now derives the same attribution on demand, only while collecting
panels for its own use, through a new `addon_panel_owner_get()` that calls the *existing*
`BPY_class_module_name_get()` directly on `PanelType::rna_ext.data` - the Python class
every registered panel already carries, hook or no hook. `rna_Panel_register` is back to
doing exactly what it did before this fork touched it.

Both changes were validated by a full rebuild (DNA-touching, ~8-9 minute cascade each) with
zero new compiler errors or warnings; behavior was not re-tested interactively since
neither change alters *when* or *how* delegation resolves, only *where* the same data is
stored and *when* the same function is called - the load-bearing logic (resolution order,
fallback, the three call sites in `space_addon.cc`) is byte-for-byte what it was before
either refactor. See `code_review.md` §3/§9 for the corresponding update to the
architectural note this closes out.

### Delegate icon in the header, and icons on the supported-editors list (2026-08-05)

Two small UX additions, both reusing the generic accessor from the refactor above rather
than adding new state.

**Header delegate icon** (later superseded by the preference dropdown below, see next
entry). A read-only `Area.context_delegate_spacetype` RNA property was added so
Python-drawn UI could show which real editor an area currently borrows context from,
without recomputing the borrowing logic itself - generic like the `ScrArea` field it
exposes, not `SpaceAddon`-specific.

**Supported-editors list, with icons.** `_addon_supported_spaces()` (space_addon.py) now
returns `(space_type, name, icon)` triples instead of bare names, via a new shared
`_space_type_icon_name()` helper - the one place both the header and the empty-state
block resolve an editor type to what the user actually sees, so they cannot disagree on
which icon or name represents a given type. The empty-state's bulleted list (`•  Name`)
became icon-labeled entries instead, strictly more informative than the bullet it
replaced.

### Letting the user pick which editor an add-on delegates to (2026-08-05)

The motivating case, raised directly: ucupaint-style add-ons registering panels for more
than one editor type had their delegate resolved by an automatic "first declared type
with an editor open wins" scan (§4a, "Context delegation had to be global, not
per-call"), with no way for the user to see or change the outcome.

**The mechanism.** `SpaceAddon` gained `preferred_delegate_spacetype` (`SPACE_EMPTY` = no
preference, keeping the automatic scan as the fallback). `BKE_paneltypes_addon_space_types_get()`
is new in `blenkernel` - "which editor types does this add-on declare top-level panels
for" - shared by `addon_delegate_spacetype_find()`'s automatic path (replacing its own
inline scan) and originally intended for the RNA dropdown's item list too (see the itemf
saga below). `addon_main_region_layout` now resolves the delegate on *every* layout pass
rather than only on cache miss, and folds the resolved value into the cache-invalidation
check - otherwise toggling the preference would do nothing until some unrelated change
happened to trigger a rebuild.

**The UI**, after iterating through three designs in conversation before landing:
1. Row of `prop_enum` icon buttons, one per supported type - dropped in favor of a
   dropdown once compared against the 3D Viewport's own interaction-mode selector, the
   closer precedent for "a second control next to the editor-type button."
2. Direct `layout.prop(space, "preferred_delegate_spacetype")` - works, but the item
   labels are Blender's plain editor names ("UV/Image Editor"), and the request was to
   prefix each with the add-on's own name ("Lumos: UV/Image Editor"), which a directly-
   drawn property can't customize per item.
3. **Landed on**: a small Python operator, `ADDON_OT_set_preferred_delegate_spacetype`,
   with its own dynamic `items` callback (the same idiom `ADDON_OT_pick_and_host` already
   uses for the add-on picker) drawn via `layout.operator_menu_enum()`. The real,
   C-defined property stays the single source of truth for storage and resolution; only
   its *presentation* moved to Python, which is where the add-on-name prefixing belongs
   anyway since it's display formatting, not stored state.

**The itemf saga - attempted, dropped, and why it's recorded rather than just reverted.**
The first version of the RNA property used a dynamic itemf
(`rna_SpaceAddon_preferred_delegate_spacetype_itemf`) restricting its own item list to
the add-on's declared types, mirroring `SpaceNodeEditor.node_tree_sub_type`'s existing
dummy-items-plus-itemf pattern. It reliably failed to compile with `C2065: undeclared
identifier`, but only from the *generated* `rna_space_gen.cc`, and only for this one
callback - not for the structurally identical, pre-existing `node_tree_sub_type_itemf`
sitting earlier in the same file. Investigated at length: confirmed `RNA_RUNTIME` truly
isn't defined for the `makesrna` code-generation tool's build of this file (checked the
actual vcxproj `PreprocessorDefinitions`, not just the CMakeLists); confirmed
`ARegionType`/`PanelType` show as forward-declared-only there even though
`BKE_screen.hh` is textually included; confirmed no separate prototypes header declares
either function; confirmed the two callbacks have identical signatures. No single check
resolved the asymmetry between the working and non-working callback.

**Dropped rather than resolved**, once it became clear the itemf's filtering was no
longer load-bearing: since the header stopped reading the property's own item list
directly (step 3 above), the underlying C property only needs to stay a valid store for
whatever Python reads or writes on it - it does not need to filter anything itself. The
property is now a plain enum over the ordinary, already-proven `rna_enum_space_type_items`
(the same array `Area.type` uses), with `SPACE_EMPTY`'s own "Empty" identifier doing
double duty as "no preference," shown to the user as "Auto." Zero custom itemf, zero
mystery. Recorded here in case a future dynamic-itemf addition on a *new* struct hits
the same wall - the working theories (unity-build grouping, generator-vs-runtime
compilation split) are what to check first, not re-derive from scratch.

**Honoring an explicit choice strictly (2026-08-05, same day, after initial ship).**
The first version of `addon_delegate_spacetype_find()` fell back to the automatic scan
whenever the preferred type wasn't open, silently substituting a different declared
type's panels. Raised as a problem: it made "the editor you picked isn't open" a
practically unreachable state, since falling back to *anything else* open first
disguised it, and it meant an explicit choice could be silently overridden by
whichever *other* editor happened to be open - defeating the point of choosing at all.
Fixed to honor the preference strictly: not open means the resolved delegate is
`SPACE_EMPTY`, full stop, no substitution. The empty result still reaches the user -
`ADDON_PT_empty_state` checks for exactly this case (a set preference matching no open
area) and names that one editor specifically ("This panel requires the following to be
open in the workspace:"), rather than the generic "one of the following" list kept for
the no-preference (Auto) case.

### Crash switching an Add-on editor area to a stock editor type (2026-08-05, fixed)

Reported directly after the preference feature shipped: switching an area hosting the
Add-on editor to any stock editor type crashed Blender. Root cause was a residual risk
from the earlier "context delegation made generic" refactor, not the new feature itself:
`ScrArea::context_delegate_spacetype` was being *set* (by the Add-on editor) and *read*
(by `ctx_wm_area_effective` in `context.cc`) but never *cleared*. Switching an area away
from the Add-on editor to, say, a Node Editor left the stale delegate type in place, so
every context lookup for that *new* Node Editor area silently redirected to an unrelated
area of the old delegate type elsewhere in the screen - while `CTX_wm_region` kept
correctly returning the Node Editor's own region. A real editor's region paired with a
different, unrelated editor's space data is exactly the kind of mismatch code elsewhere
assumes cannot happen, and it didn't take much to crash something.

**Fixed generically**, not with Add-on-editor-specific code: `ED_area_newspace()` in
`editors/screen/area.cc` - the one place an area's `spacetype` actually changes - now
resets `context_delegate_spacetype` to `SPACE_EMPTY` whenever the type changes, for any
area, regardless of what it's switching to or from. Consistent with the field's own
framing as a generic per-area capability rather than something the Add-on editor owns.

### Mio3 UV: mode-gated panels, investigated and confirmed working (2026-08-05)

Raised as a possible conflation bug: Mio3 UV appears to show different panels depending
on whether the real, borrowed editor is in UV-editing mode versus plain image viewing,
and the new preference dropdown doesn't offer a way to pick between them.

**Not a bug, and not something the dropdown should handle.** Checked directly against
the installed extension's source: every one of its panels declares
`bl_space_type = "IMAGE_EDITOR"` - Blender has only the one `Area.type` for both UV and
Image editing, already settled in an earlier entry above ("UV/Image Editor" is one
editor, not two). The actual distinction lives one level down, in a shared base class
(`Mio3UVPanel` in `classes/operator.py`) whose `poll()` checks
`context.area.spaces.active.mode == "UV"` - `SpaceImage.mode`, a sub-state of the space
*instance*, not the declared editor *type*. A few panels (`UV_PT_mio3_Utility` and its
children) override `poll()` without that check, so they show in both modes.

This distinction was already fully transparent to the existing delegation mechanism
with no change needed: `CTX_wm_space_data` resolves to the real, borrowed `SpaceImage`
instance regardless of its mode, so `poll()` reads that instance's *actual, live* mode
exactly as it would in a native Image Editor sidebar. Confirmed directly: with the
borrowed editor in image-viewing mode, only the mode-agnostic `Utility` panel draws,
matching `Mio3UVPanel.poll()`'s own logic exactly.

**Multiple Add-on editor areas, independence confirmed (raised alongside the above).**
`context_delegate_spacetype` lives on `ScrArea` - per area, not global - and each area's
own layout pass resolves and stores it independently from its own `SpaceAddon::addon_id`
and `preferred_delegate_spacetype`. Two Add-on editor areas hosting different add-ons,
each delegating to a different open editor, do not interfere with each other by
construction. The one pre-existing limitation, unrelated to multiple Add-on areas:
`BKE_screen_find_big_area` picks one specific instance when several real editors of the
*same* type are open, with no per-viewer disambiguation - the same single-delegate
limitation already documented in §5/§7, just visible from a new angle.

### Capping the editor-type menu, without capping curation itself (2026-08-05)

Raised as a UX question once the curated list had no upper bound in practice: what
happens once a user curates enough add-ons that the editor-type menu becomes unwieldy?
The design went through one significant reversal before landing.

**First direction, reverted before implementation finished.** A richer design was
underway - a persisted per-entry `last_used_time` on `bAddonEditor`, a "visible count"
preference showing the N most-recently-used entries inline, and a "More Add-ons..."
entry opening a search popup over the full curated list (mirroring the existing
`ADDON_OT_pick_and_host` picker, scoped to curated entries instead of all installed
ones) for anything beyond that. Reverted mid-implementation, before any build was run,
once three constraints were stated plainly: the curated list must stay exactly as
curated (never reordered as a side effect of normal use - recency tracking would have
done exactly that), it must never grow "out of control" only when the user wants it to
(a preference, not a forced behaviour), and picking an add-on must never be *blocked* by
a full list (no "you can't do that, remove something first").

**What shipped instead is simpler and satisfies all three directly.**
`UserDef.addon_editor_max_visible` (0 = no cap) limits how many entries
`addon_ids_get()` (`space_addon.cc`) returns for the menu - in the order they were
added, not alphabetically or by recency. Addition order was already the *reason* the
previous alphabetical sort existed (the comment cited index stability for the sub-type
value), so switching to natural `ListBase` order costs nothing there and needs no new
per-entry state at all - no `last_used_time`, no DNA growth on `bAddonEditor`, no
"More..." popup, no second picker operator. The cap only ever shortens what the
function *returns*; `UserDef.addon_editors` itself is never trimmed, reordered, or
otherwise touched by it - an entry past the cap stays fully curated and editable in
Preferences, simply not offered in the menu until an earlier entry is removed or the
limit is raised.

**Picking still always works.** `ADDON_OT_pick_and_host` adds the entry and switches the
area to host it unconditionally, regardless of the cap - there is no "list is full,
can't add this" path anywhere. If the addition happened to push the count past the cap
(new entries always land at the tail, so this is exactly "did this one just become
hidden"), the operator reports a standard `{'INFO'}` message - Blender's ordinary,
non-intrusive status-bar mechanism - naming the add-on and where to manage it, rather
than silently hiding it with zero feedback.

### Empty-state fallback drawn with the wrong space, crashing on `space.addon_id` (2026-08-17, fixed)

Reported live: `ADDON_PT_empty_state.draw` (and, by the same cause, `ADDON_HT_header`) raised
`AttributeError: 'SpaceNodeEditor' object has no attribute 'addon_id'` (or `'SpaceView3D'`,
depending which editor happened to be delegated to at the time), intermittently, across more
than one hosted add-on, typically on reopening an area whose collected panel list had gone
empty.

**Root cause, and why the earlier "just read `context.area.spaces.active` instead of
`context.space_data`" framing (§4a, "Context delegation had to be global, not per-call")
did not actually cover this case.** That framing assumed `context.area`
(`CTX_wm_area(C)`) itself is never touched by delegation - only the *typed* accessors
routed through `ctx_wm_area_effective()` are. True in general, but
`addon_main_region_layout` (`space_addon.cc`) does something cruder and broader for its own
purposes: around the entire `ED_region_panels_layout_ex` call - the pass in which every
panel's Python `draw()` actually runs, not a separate raster pass - it directly calls
`CTX_wm_area_set(C, area_delegate)` / `CTX_wm_region_set(...)`, restoring both afterward.
That swap has no concept of *which* panel is currently drawing; it wraps the whole call.

`ADDON_PT_empty_state` is injected into that same call as the sole entry precisely when the
real, foreign panel list comes up empty (`addon_panel_types_collect`) - but a delegate can
still be resolved to a real, open editor even when nothing in the list ends up needing it
(an explicit `preferred_delegate_spacetype` set to a type the add-on has no matching panel
for, or every matching panel dropped by the space-type filter for an unrelated reason). In
that case the swap still fires, and the one panel being laid out - this editor's own chrome,
not re-hosted content - reads `context.area.spaces.active` and gets the delegate's real
`SpaceNodeEditor`/`SpaceView3D` instead of this area's own `SpaceAddon`. `context.area` was
never the safe accessor to assume here; the code doing the swapping doesn't distinguish "a
foreign panel that needs this" from "our own panel that never should have been given it."

**Fix, scoped to the layout function, no Python change needed.** Before deciding whether to
swap, `addon_main_region_layout` now checks whether `saddon->runtime->paneltypes` is a
single entry named `"ADDON_PT_empty_state"` (`ListBaseT::is_single()` plus one `STREQ`) and
skips the `CTX_wm_area_set`/`CTX_wm_region_set` pair entirely when it is. Foreign panels are
laid out exactly as before - this only changes behavior for the one case where the fallback
panel is the sole thing about to be laid out. `ADDON_HT_header`'s equivalent `space.addon_id`
read draws from a separate call path not wrapped by this swap in the first place, so it was
never actually at risk once the region-layout case was traced precisely - the header symptom
reported alongside it was the same underlying crash observed from the same log noise, not a
second site needing its own fix.

**Rebuild note.** This change touches only `editors/space_addon/space_addon.cc` - no DNA, no
RNA - so it was a normal incremental `blender.vcxproj` build, not the DNA-touching cascade
described elsewhere in this log. The first build attempt failed for an unrelated reason:
invoking MSBuild through a POSIX shell mangled `/p:`-style flags into path fragments,
producing `MSB1008` with no compilation attempted at all - a tooling gotcha, not a build
failure, worth remembering if this build step is scripted again from a POSIX shell rather
than PowerShell. The subsequent attempt, correctly invoked, triggered a much wider rebuild
than the single changed file would suggest (`bf_editor_space_addon` plus dozens of unrelated
modules) because the branch checkout immediately prior touched `DNA_space_types.h` and
`DNA_screen_types.h`, both broadly included - expected per §7's own mergeability table, not
a regression. Unbounded `/m` parallelism against that wide a rebuild exhausted the paging
file and produced `C1060: compiler is out of heap space` across many unrelated projects;
capping to `/m:4` resolved it.

### Delegation is single-window; a second window's Add-on Editor cannot see it (2026-08-17, recorded, not built)

Raised as a design question after the empty-state fix above: if a user opens a *new*
window and hosts an add-on there needing, say, `NODE_EDITOR`, and the only open Node
Editor is in the main window, delegation does not find it - even though both windows
belong to the same Blender session.

**Cause.** `BKE_screen_find_big_area()` - the upstream utility this editor's delegation
reuses (see §"Where else Blender uses this," recorded from the earlier Q&A) - takes a
single `bScreen*`, and each `wmWindow` owns its own active screen.
`addon_delegate_spacetype_find()` calls it via `CTX_wm_screen(C)`, which only ever
returns the *current* window's screen. Not fork-specific: every one of the 9 other
upstream call sites of this function shares the same single-screen assumption - this
editor inherited the limitation rather than introducing it.

**What fixing it would cost.**
1. A new cross-window search helper in `blenkernel/intern/screen.cc`, iterating
   `wm->windows` and each window's active screen, picking the biggest match across all of
   them (~20-30 lines). `BKE_screen_find_big_area()` itself should not change signature -
   9 existing upstream callers depend on its current single-screen contract, and it is
   exactly the kind of shared utility this fork has avoided touching (§7).
2. Two call sites in `addon_delegate_spacetype_find()` to swap over, plus
   `addon_screen_signature_get()` (the cache-invalidation signature), which currently only
   hashes the *current* screen's open space types - opening/closing an editor in a
   *different* window would not invalidate the cache otherwise, the same class of gap
   already documented once for the single-screen case (§4a, "its own cache-invalidation
   gap").
3. The Python side (`_addon_has_open_delegate()`, header/empty-state messaging) checks
   `{area.type for area in context.screen.areas}` - single-screen too. Widening only the
   C side would reproduce exactly the C/Python disagreement the empty-state fix above
   exists to prevent - both sides have to move together.
4. **The real risk, flagged rather than guessed at**: once the delegate can resolve to an
   area in a *different* `wmWindow`, `CTX_wm_area_set`/`CTX_wm_region_set` would set the
   bContext's area/region to something that no longer belongs to `CTX_wm_window(C)`. No
   accessor in the 18-strong chain through `ctx_wm_area_effective()` has been audited
   against that mismatch, since it has never been possible before. This is structurally
   the same bug class as the empty-state fix above - one piece of context swapped,
   something nearby not, silently disagreeing - so it needs a deliberate audit, not an
   assumption that it's fine.

**Not started.** Recorded as a deliberate future item alongside horizontal panel layout
and per-panel delegate resolution, not attempted now - estimated at roughly half a day to
a day once the cache-signature widening, Python mirroring, and the window-mismatch audit
are all counted, not a quick add.

---

## 6. Design questions answered along the way

**Does an add-on panel need a corresponding editor open at all?** Often not. Only
`space_data`/`region_data` and editor-supplied context members (§4a, step 3) need a
delegate. `context.scene`, `context.object`, `context.selected_objects`, and `bpy.data`
are resolved at the screen level
([screen_context.cc](../source/blender/editors/screen/screen_context.cc)) and work
in any editor, including ours, with nothing else open. A panel that only reads `bpy.data`
- a light manager built as "list `scene.objects` filtered to lights, edit `light.energy`"
- needs no delegate and no other editor present at all. Selection state itself lives on
the object/view-layer, not on any space, so even that survives with zero 3D Viewports
open anywhere.

**Should delegated context persist after the source editor closes?** Deliberately not
attempted. That would mean an operator acting on a node selection, say, that may no
longer be selected or may have been deleted - exactly the staleness class of bug
Blender's poll-every-frame convention exists to prevent. A data-only add-on (previous
question) never needs this in the first place, since it reads `bpy.data` live.

**Is a per-tool workspace the right pattern?** Yes, and it composes with an existing,
unrelated Blender mechanism: `PanelType.bl_owner_id` already lets a panel opt in to
specific workspaces. A "Lighting" workspace containing only an Outliner, a Properties
editor, and this fork's Add-on editor is fully functional with no 3D Viewport, provided
the hosted add-on is data-only in the sense above.

---

## 7. Fork depth, upstream mergeability, and file compatibility

As of the point this section was written: 25 files touched, +1126/-21 across 4 commits.
New files (`editors/space_addon/`, `bl_ui/space_addon.py`) carry the bulk of it and can
never conflict with upstream changes. What can:

| File | Nature of the touch | Conflict risk on rebase |
| :--- | :--- | :--- |
| `blenkernel/intern/context.cc` | 17 accessor bodies call one static helper instead of `CTX_wm_area` directly; the helper itself mentions no editor by name (see §4a, "Context delegation made generic") | **Low-medium**, down from *highest*. Upstream touching an accessor still needs a one-line re-merge, but the helper's own body - the part that could actually drift in meaning - no longer names `SPACE_ADDON`, so there is nothing editor-specific for an upstream reviewer to object to at that call site. |
| `blenkernel/BKE_screen.hh`, `blenkernel/intern/screen.cc` | Down to a 5-line panel-types revision counter, plus one new query function (`BKE_paneltypes_addon_space_types_get`, §4a) | **Near zero.** The one-time hook this fork used to add to `rna_Panel_register` (populating `PanelType::addon_id` for every panel registered in Blender) was removed entirely - see §4a. The new function is additive, not a hook into an existing code path. |
| `makesdna/DNA_space_enums.h` | `SPACE_TYPE_NUM` rebased onto a new enumerator | Low-medium. Conflicts only if upstream adds a space type in the same commit range; trivially resolved by renumbering. |
| `makesdna/DNA_screen_types.h` | One `short` field on `ScrArea`, replacing 2 bytes of existing padding - no struct size change | Low. Additive, generic (not named after this editor), and framed the way `SpaceNode`-style precedent already is. |
| `makesdna/DNA_space_types.h` | One more `short` field on `SpaceAddon` (`preferred_delegate_spacetype`), same padding-reuse pattern | Low. Purely additive to this fork's own struct. |
| `editors/screen/area.cc` | One line in `ED_area_newspace()` resetting `ScrArea::context_delegate_spacetype` on any area-type change | **Low.** A single, generic reset - not editor-specific - in a function upstream touches occasionally; a one-line re-merge at worst. Fixes a real crash (§4a), so worth defending as a correctness fix regardless of rebase cost. |
| `makesrna/intern/rna_space.cc`, `rna_screen.cc`, `spacetypes.cc` | Additive entries in existing lists/switches; two new plain enum properties (`Area.context_delegate_spacetype`, `SpaceAddon.preferred_delegate_spacetype`), no custom itemf (§4a) | Low. Insertions, not edits to existing lines. |
| `anim_filter.cc`, `grease_pencil_convert_legacy.cc`, `resources.cc` | One `case` label added to an exhaustive switch each | Near zero. |
| `makesdna/DNA_userdef_types.h`, `rna_userdef.cc` | New struct + field, additive | Low, with one caveat: DNA requires 8-byte alignment for pointer-containing members file-wide, so an inserted field must be padded correctly (hit this once already, §4a). |

Net assessment: this stays a **rebasable patch series**, not a fork that diverges
structurally. Nothing overrides upstream behavior for any area type other than
`SPACE_ADDON`; every other space type's code path is byte-for-byte what it was.
`context.cc` was the one file with real ongoing maintenance cost and a design objection
attached to it; after the §4a refactor, the design objection is gone (no kernel code
names this editor) and what's left is an ordinary, low-risk rebase surface.

### Save-file behavior and compatibility

`SpaceAddon` is a normal `SpaceLink`: it participates in the same `blend_write`/
`blend_read_data` machinery as every other space, so which editor an area is hosting,
and its `addon_id`, are saved as part of the screen layout precisely like any other
editor choice - not specially, not separately.

**Opening a fork-saved file in unmodified upstream Blender is safe**, verified against
existing upstream mechanisms rather than assumed:

- File-version compatibility is gated by `BLENDER_FILE_VERSION`/`BLENDER_FILE_SUBVERSION`
  ([readfile.cc:1177-1196](../source/blender/blenloader/intern/readfile.cc#L1177-L1196)),
  compared against what the *reading* build was compiled with. This fork has not touched
  either define, so files remain nominally same-version and this check never fires.
- Struct-level forward compatibility is what Blender's SDNA format is for: a reading
  build that doesn't know the `SpaceAddon` struct doesn't need to, since `SpaceLink`'s
  common header (`next`/`prev`/`regionbase`/`spacetype`/`link_flag`) is what every
  reader depends on, and SDNA is self-describing per-file.
- `ScrArea::spacetype` resolving to an ID no build recognizes already has a generic,
  pre-existing fallback with exactly this docstring: *"Setup a known space type in the
  event a file with an unknown space-type is loaded"*
  ([area.cc:2240-2270](../source/blender/editors/screen/area.cc#L2240-L2270)). It
  degrades the area to `SPACE_VIEW3D`. This is not something this fork added or needs to
  add - it is how upstream Blender already handles exactly this situation, for any
  experimental or build-specific space type.

Net effect: a vanilla build opening a fork-saved file loses the add-on-hosting areas
(they become 3D Viewports) but does not crash, does not corrupt unrelated data, and
loses nothing else in the file.

### Resaving through a build that doesn't know `SPACE_ADDON`: real, silent data loss

The above covers *opening* a fork-saved file elsewhere. *Resaving* it from there is a
different question, checked against the actual write path rather than inferred:

```cpp
// BKE_screen_area_map_blend_write, blenkernel/intern/screen.cc:1435-1444
for (SpaceLink &sl : area->spacedata) {
  for (ARegion &region : sl.regionbase) {
    write_region(writer, &region, sl.spacetype);      // always runs
  }
  SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);
  if (space_type && space_type->blend_write) {          // gated
    space_type->blend_write(writer, &sl);
  }
}
```

Region data (`write_region`, headers/window regions) writes unconditionally regardless
of whether the space type is recognized. The space-specific struct - for `SpaceAddon`,
that means `addon_id` - only writes if `BKE_spacetype_from_id` resolves the type *and*
it has a `blend_write` callback. On a build with no `SPACE_ADDON` registered, that
condition is false, so the write is skipped entirely - not written with defaults, not
written as raw bytes, simply omitted.

Combined with the read-side fallback (`area_init_type_fallback`, previous section):
opening a fork-saved file in vanilla Blender promotes a `SpaceView3D` to active for any
`SPACE_ADDON` area, while the original `SpaceAddon` entry survives *in memory* as a
non-active `spacedata` list member (Blender keeps one entry per editor type an area has
ever shown, to preserve state when switching back). If that session is then **saved**
from vanilla, the write loop above reaches that orphaned entry, finds no recognized
`SpaceType`, and drops it. The area itself saves fine - it has a working `SpaceView3D` -
but the specific `addon_id` and the fact that area was ever hosting an add-on is gone
from the file, permanently. Reopening that resaved file even with this fork no longer
offers a way back to what was hosted there; it is an ordinary `View3D` area now, with no
record it was ever anything else.

**This is the one real compatibility cost to name plainly**: not a crash, not corruption
of anything else in the file, but silent, irreversible loss of *this fork's own* screen
state, specific to the round trip "saved here → opened and resaved elsewhere." Opening
without resaving, or resaving with this fork, is unaffected either way.

### Identifying the fork without claiming to be "newer"

`BLENDER_FILE_VERSION`/`SUBVERSION` must **not** be bumped - that is the newer-version
refusal/warning trigger for anyone opening a fork-saved file, in the fork or in vanilla
Blender. The correct lever already exists in upstream for exactly this purpose:
`BLENDER_VERSION_SUFFIX` in
[BKE_blender_version.h](../source/blender/blenkernel/BKE_blender_version.h) - a
free-form cosmetic string, currently empty, that does not feed into any file
compatibility check (its only current reader compares it against `"LTS"` for splash/about
labeling). Setting it identifies the build in the UI without changing file compatibility
semantics at all.

There is also, already, a de-facto identifier with zero code changes needed: the build
hash and commit date shown by `blender --version` and in `Help > About`, sourced from
`buildinfo` and already distinct for this fork's every commit (confirmed earlier: `build
hash: 027ef661892c`).

---

## 8. Repository conventions for this fork

- Work happens on a dedicated branch off `main`; the publishable diff is
  `git format-patch main..<branch>`.
- `main` is never committed to, so upstream rebases stay clean.
- Building requires `make update` first, to fetch precompiled libraries into `lib/`.
