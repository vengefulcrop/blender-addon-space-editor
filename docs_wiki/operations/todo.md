---
type: operation
title: "Add-on Space Editor — Open Tasks"
description: "The open task list for the Add-on Space Editor, grouped by subject"
tags: [addon-space-editor, todo, punch-list]
last_updated: 2026-09-13
---

# Open Tasks

Consolidated from `docs_ui/punch_list.md` and the open items in
`docs_ui/legacy/code_review.md`. Status as of 2026-08-19, checked against
the git log up to commit `d66db359820` ("Add-on Editor: tree row activation, bundled add-ons, and a drawing-nothing notice"). See [bugfix.md](./bugfix.md) for
defects and [handoff.md](../handoffs/handoff.md) for the current branch
state.

## 1. Shared logic and constants

1. [ ] Extract the panel-scan walk into one shared helper. The walk (skip
   `SPACE_ADDON`, iterate `RGN_TYPE_UI`/`RGN_TYPE_WINDOW`, skip
   sub-panels, resolve owner, compare) was recorded in three places. Two
   of them exist: `addon_panel_types_collect` (`space_addon.cc`) and
   `BKE_paneltypes_addon_space_types_get` (`screen.cc`). The third name,
   `addon_has_registered_panels`, does not exist in the code.

2. [ ] Share a named constant for `"ADDON_PT_empty_state"`. The code
   hardcodes the literal string independently in `space_addon.cc:178`,
   `space_addon.cc:568`, and implicitly through Python's `bl_idname`. Add
   the constant to `addon_intern.hh`.

3. [ ] Extract `addon_panel_poll_guarded` into a shared helper and upgrade
   `rna_ui.cc`'s own `panel_poll` to the same safety. The guarded version
   already distinguishes a raised exception from a plain `False` return,
   which the original does not.

4. [ ] Fix the stale doc-comment on `BKE_paneltypes_addon_space_types_get`.
   The comment still references the dynamic-itemf approach that developers
   attempted and dropped. The function itself is correct.

5. [ ] Unify the two add-on attribution implementations. C++
   `BPY_class_module_name_get` truncates to the top-level module and
   matches with `STREQ`. Python `_addon_top_level_panel_space_types`
   matches by module prefix.

   They agree today, but the comment in
   `addon_panel_types_collect` asserts they are the same filter, which
   invites drift. Add a cross-reference note on both sides, or expose the
   C++ answer to Python so one implementation remains. See
   [bugfix.md](./bugfix.md) item 5.

6. [ ] Remove the redundant double `BKE_screen_find_big_area` lookup per
   layout pass in `addon_main_region_layout` (once inside
   `addon_delegate_spacetype_find`, once directly). Low priority.

## 2. Multi-window delegation

7. [ ] Widen `addon_delegate_spacetype_find()` to search every window's
   screen, not only the current window's own screen.
   `BKE_screen_find_big_area` takes a single `bScreen*`, so an editor open
   in the main window is invisible to an Add-on Editor area in a second
   window.

   Currently mitigated by hiding the Add-on Editor option in
   non-main windows (commit `e0ec73dfd1a` ("Add-on Editor: hide the Add-ons menu section in non-main windows")), not solved. Estimated at
   about half a day, counting the cache-signature widening and the
   Python-side mirroring.

8. [ ] Mirror the cache-signature widening on the Python side.

9. [ ] Build the cross-window swap by following the order established in
   `bpy_rna_context_temp_override_enter`
   (`python/intern/bpy_rna_context.cc`, around line 344-355):
   `CTX_wm_window_set` then `CTX_wm_screen_set` then `CTX_wm_area_set`
   then `CTX_wm_region_set`.

   Add the window and screen calls alongside
   the existing `CTX_wm_area_set`/`CTX_wm_region_set` calls in
   `addon_main_region_layout`.

## 3. Modal operator handling

10. [ ] Build a wrapper operator that stashes the real operator's idname
    and properties and blocks a modal operator with a warning, instead of
    invoking it. Confirmed direction: block always, no "run anyway."

11. [ ] Add a post-layout walk of the region's `uiBlock`/`uiBut` lists to
    find and rebind buttons whose `wmOperatorType::modal` is not null.
    Keep the check coarse.

    A finer classifier is not staticly buildable:
    `TRANSFORM_OT_translate` breaks when hosted because its
    `convertViewVec()` (`transform.cc:185-231`) needs the real region's
    `RegionView3D` state. However, the installed DreamUV add-on's
    `view3d.dreamuv_uvscale` works fine hosted, because its `invoke()`
    and `modal()` never read region or space data.

12. [ ] Evaluate the `temp_override`-based real fix as an alternative to
    the warning: wrap the operator call with
    `bpy.context.temp_override(area=..., region=...)` using the delegate
    the panel's own collection already resolved.

    `WM_operator_call_py`
    returns once `invoke()` reports `RUNNING_MODAL`, and
    `WM_event_add_modal_handler` captures its area and region from
    `CTX_wm_area(C)`/`CTX_wm_region(C)` synchronously at that moment, so a
    tightly scoped swap covers the whole operator lifetime.

    The missing
    piece is the interception point: Blender's generic dispatch (`interface_handlers.cc`)
    invokes a button in a re-hosted panel,
    which has no hook for "this button lives in a re-hosted foreign
    panel." This needs the same `uiBlock`/`uiBut` rebind walk as item 11.

## 4. Per-panel and per-area hosting

13. [ ] Implement per-panel delegate resolution. **Settled on 2026-09-12
    by a code trace. It is not implemented.** The delegate stays one
    value for the whole area.

    The two commits `ed592f7ef76` ("Add-on Editor: scope context
    delegation to panel callbacks, fix extension names") and
    `3e266991c86` ("UI: let ED_region_panels_layout_ex run panel
    callbacks under an overridden context") narrowed the *scope* of the
    override. They did not change its *resolution*. The override now
    wraps each panel callback instead of the whole layout pass. It still
    carries one area-wide value.

    Evidence:
    - `space_addon.cc:571-583` builds exactly one
      `PanelDrawContextOverride` per layout pass, and passes it to the
      single `ED_region_panels_layout_ex` call at line 585.
    - The value comes from `ScrArea::context_delegate_spacetype`
      (`DNA_screen_types.h:663`), set once by
      `addon_delegate_spacetype_find()` (`space_addon.cc:464-486`).
    - `panel_add_check()` and `ed_panel_draw()` pass the same pointer
      down. Neither reads `PanelType::space_type`.

    The ucupaint gap is still open. An add-on with panel X for
    `'VIEW_3D'` and panel Y for `'IMAGE_EDITOR'` gets one delegate.
    The panel that does not match resolves `context.space_data` from the
    wrong editor, or fails its own poll.

    This work requires three parts:
    1. Record on each collected panel type which space type it came
       from. `addon_panel_types_collect()` (`space_addon.cc:334-362`)
       already iterates `BKE_spacetypes_list()` and duplicates the type.
    2. Change the single `ctx_override` parameter of
       `ED_region_panels_layout_ex` into a per-panel lookup. Resolve
       each panel with `BKE_screen_find_big_area(screen, pt->space_type, 0)`.
    3. Make `ctx_wm_area_effective()` (`context.cc:972-992`) per-panel
       aware. An operator started from a button in panel Y must resolve
       the `IMAGE_EDITOR` context. Today the field is on `ScrArea`.

14. [ ] Design and implement per-panel hosting: store a list of panel
    ID names on `SpaceAddon`. An empty list means host everything
    belonging to `addon_id`, so existing areas and saved files keep
    working.

    A non-empty list means host exactly those panels. The
    motivating case is Lumos, whose four registered panels (Light
    Editor, a one-button popup panel, and two Manager panels) all get
    hosted today when the user wants only one.

15. [ ] Decide list vs single panel id for the per-panel hosting field. A
    list is "compose my own editor." A single `char panel_id[64]` is
    simpler, with no `ListBase` and no blend read/write. Unresolved
    which the Lumos case needs.

16. [ ] Enforce the "one editor type per area" invariant at pick time once
    item 14 is built: once an area holds its first panel, offer only
    panels that declare the same `bl_space_type` or are space-agnostic.
    This makes the ucupaint-class bug unrepresentable rather than merely
    avoided.

17. [ ] Add explicit `blend_write` and `blend_read_data` handling for the
    new panel-list DNA field, in the same commit as the DNA change.
    `addon_blend_write` currently writes only the flat struct. Getting
    this wrong reproduces the dangling-pointer crash described in
    [bugfix.md](./bugfix.md) item 1.

18. [ ] Only support top-level panels for item 14. Collection already
    skips `pt.parent != nullptr`, and children come along through
    `PanelType::children`. Hosting a sub-panel standalone means lifting
    that skip, and its `draw()` may assume the parent ran first.

## 5. Hosting-detection API for add-on authors

19. [ ] Add a runtime-only WM-level flag, not persisted DNA, set and
    cleared symmetrically around the same `ED_region_panels_layout_ex`
    call already bracketed for the area and region swap.

    Give hosted
    add-ons an official way to know they run inside this editor,
    so cooperative authors can guard region- or view-space-dependent
    code themselves.

20. [ ] Expose the resolved delegate's editor type and the curated
    add-on's own display name alongside the flag, not just a bare
    boolean, so authors can write real messaging.

21. [ ] Do not expose this signal through `context.area.type` or the
    generic screen-layer context (`screen_context.cc`). During a panel's
    own `poll()`/`draw()`, `CTX_wm_area(C)` is already swapped to the
    delegate, so `context.area.type` deliberately reads as the native
    editor type, not `'ADDON'`.

    Exposing it through the generic
    screen-layer context would teach generic core code about
    `SPACE_ADDON` by name, reversing the work already done to make
    context delegation generic. Keep the flag self-contained.

## 6. Documentation for add-on authors

22. [ ] Write a recipe describing header hosting. `ADDON_HT_header`
    (`space_addon.py:187`) is an ordinary `bpy.types.Header` with
    `bl_space_type = 'ADDON'`, so the stock extension mechanism already
    applies: any add-on can call
    `bpy.types.ADDON_HT_header.append(my_draw_func)` and have it fire
    only while hosted in this editor. Nothing needs building. This is
    undiscoverable without reading the source.

23. [ ] Document that `context.space_data` is not the correct accessor
    for reading `SpaceAddon` state, in header draw or anywhere else.
    `CTX_wm_space_data()` (`context.cc:990`) routes through
    `ctx_wm_area_effective()` unconditionally, so it always resolves to
    the delegate's space when one is active.

    Document
    `context.area.spaces.active` as the correct accessor, exactly as
    `ADDON_HT_header.draw()` already does (`space_addon.py:199`).

24. [ ] Document `context.area.context_delegate_spacetype` for add-ons
    that declare panels for more than one space type. A header draw
    function can gate its buttons on which of the add-on's own panel
    sets is currently showing by combining
    `context.area.spaces.active.addon_id` with
    `context.area.context_delegate_spacetype`.

## 7. Sentinel marker hardening

25. [x] Delete `SPACE_ADDON_ID_PICK_MARKER`. **Done on 2026-09-12.**

    A code trace and a repo-wide grep settled it first. Nothing wrote the
    marker. Nothing read it. The commit `5add1f67281` ("Add-on Editor:
    remove the curated-list picker, tree replaces it") had already deleted
    both live sites in `rna_screen.cc`:
    - The write, `saddon->addon_id[0] = SPACE_ADDON_ID_PICK_MARKER;`, in
      `rna_Area_ui_type_set`.
    - The read, the marker test in `rna_Area_ui_type_update` that called
      `ADDON_OT_pick_and_host`, a symbol also now deleted along with the rest
      of the picker.

    Two dead references survived in
    `source/blender/makesdna/DNA_space_types.h`. Both are now gone:
    - The `#define`, formerly at line 1331.
    - The `SpaceAddon::addon_id` doc comment, which named
      `addon_space_subtype_set`. That symbol does not exist anywhere in
      `source/` or `scripts/`. The comment now states what the field
      holds, and what an old file can hold.

    The change required no versioning step. `source/blender/blenloader/` holds zero
    references to `addon_id` or `SpaceAddon`. An old `.blend` can carry a
    leftover 0x01 byte in `addon_id`. That byte is inert. It matches no
    module name, so the editor treats the field as unset.

## 8. Open investigations

26. [ ] Confirm whether the global poll-failure blacklist
    (`addon_poll_failed_get`, a static `Set<std::string>` shared across
    all areas and windows) needs a narrower replacement, or whether
    standard RNA-boundary error reporting alone is sufficient.

    The audit raised a concern (cross-window side effects, silent permanent
    blacklisting) but did not trace it through to a conclusion.

27. [ ] Reproduce the tall-panel overlap (see
    [bugfix.md](./bugfix.md) item 11) in the regular 3D Viewport sidebar
    first, to confirm whether the fault belongs to this editor or to the
    hosted panel's own content.

## 9. Dead symbols from the removed picker

Found by a pre-alpha audit on 2026-09-12. A repo-wide grep verified each one. **Marked, not deleted.** Delete them in one commit, after
the next rebase onto upstream `main`.

**No versioning step is necessary.** The user states that no real file holds
any of these values. Only the test `.blend` files do, and no user has ever
set one. Treat the DNA fields as free to remove.

28. [ ] Delete `active_addon_editor_index`. Zero readers.
    - `source/blender/makesdna/DNA_userdef_types.h:1097`, the DNA field.
    - `source/blender/makesrna/intern/rna_userdef.cc:7963-7966`, the RNA
      property. It carries `PROP_SKIP_SAVE`, so it never reaches a file.
    - It was the active index of the Preferences UIList that the picker
      used. No UIList and no Python reads it now.

29. [ ] Delete `addon_editor_max_visible`. Zero readers.
    - `source/blender/makesdna/DNA_userdef_types.h:1104`, the DNA field.
    - `source/blender/makesrna/intern/rna_userdef.cc:7978-7986`, the RNA
      property.
    - It capped how many curated entries showed in the editor type menu.
      That menu section went with the picker. Nothing reads the value.

30. [ ] Decide on `CTX_wm_space_addon()`. Zero call sites.
    - `source/blender/blenkernel/BKE_context.hh:229`, the declaration.
    - `source/blender/blenkernel/intern/context.cc:1071`, the body.
    - `space_addon.cc` reads `area->spacedata.first` directly instead.
    - Every other space type has a matching `CTX_wm_space_*`. Keep it for
      that parity, or delete it as an unused public API. This one is a
      judgment call, not a defect.

### Not dead: `UserDef::addon_editors`

An audit reported this list as dead with zero writers. **That is wrong.**
`rna_userdef.cc:1120` writes it with `BLI_addtail`, exposed to Python as
`preferences.addon_editors.new()`.

The accurate state: nothing populates the list any more, because the
picker that called `new()` is gone. Two sites still read it as a display
name cache:
- `source/blender/editors/space_addon/addon_tree_view.cc:99`
- `scripts/startup/bl_ui/space_addon.py:21`

A preference file written before developers removed the picker can still hold
entries. The comment at `space_addon.py:15-19` already states this: the
tree can host an add-on that was never added through the picker, and so
has no `bAddonEditor` entry to read from.

**Keep it.** It is an older read-only fallback, not dead code.

## Explicitly out of scope

- **Horizontal panel layout.** Assessed and scoped out. It would mean
  forking `ED_region_panels_layout_ex`'s internals. A candidate future
  item, not a task on this list.

- **Per-area add-on internal tab state** (the SourceOps case, code review
  §5). Rejected. A scoped per-area value swap around layout is
  technically viable, but it mutates document data as a side effect of
  drawing, which crosses a line the design otherwise holds. See
  `docs_ui/legacy/code_review.md` §5 for the full reasoning.

- **A custom icon per add-on** (code review §8.2). Investigated and
  rejected. No source (legacy `bl_info`, extension `blender_manifest.toml`,
  or `Panel.bl_icon`) declares a usable icon in practice. Keep
  `ICON_PLUGIN`.

- Every item raised by an automated audit that did not survive
  verification (fabricated APIs, or findings already deliberately
  considered and rejected in the code's own comments). See
  `audits/audit_reconciliation_summary.md` for the full accounting.
