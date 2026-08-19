# Add-on Space Editor — Punch List

Consolidated from `audits/audit_reconciliation_summary.md`, `audits/audit_pruning_candidates.md`,
`audits/audit_native_equivalents.md`, and this session's design discussions. Status as of
2026-08-18.

---

## Small, ready-to-do cleanups

All verified real via direct source inspection (not just asserted by an audit).

1. **Triplicated panel-scan logic.** The "which top-level panels belong to add-on X"
   walk (skip `SPACE_ADDON`, iterate `RGN_TYPE_UI`/`RGN_TYPE_WINDOW`, skip sub-panels,
   resolve owner, compare) is implemented nearly identically three times:
   `addon_panel_types_collect`, `addon_has_registered_panels` (both `space_addon.cc`),
   and `BKE_paneltypes_addon_space_types_get` (`screen.cc`). Extract to one shared
   helper.
2. **`"ADDON_PT_empty_state"` hardcoded independently** in two C++ locations
   (`space_addon.cc:178`, `:568`) plus implicitly via Python's `bl_idname`. Share a
   named constant (`addon_intern.hh`) so a rename can't silently desync the two sides.
3. **`Area.context_delegate_spacetype` RNA property should be wired up, not removed**
   (revised 2026-08-18 — previously listed as dead/remove-candidate; that call reverses
   here). It's real, registered RNA (`rna_screen.cc:530`), currently unread anywhere in
   `scripts/`, but two live uses have since surfaced:
   - `ADDON_HT_header`'s `_addon_has_open_delegate()` (`space_addon.py:104-118`)
     re-derives "is a delegate available" by manually scanning `context.screen.areas`
     for a matching `area.type` — duplicating a search the C side has already done via
     `BKE_screen_find_big_area` inside `ctx_wm_area_effective()` and cached on this
     exact property. `context_delegate_spacetype != 'EMPTY'` is the simpler, already-
     authoritative equivalent for "is there something to draw" and should replace the
     rescan (not a pure equivalence in general — `_addon_has_open_delegate` asks "is
     *any* of my supported space types open" for multi-type add-ons, while this
     property gives the *one* resolved delegate for *this* area — but resolution can
     only pick from what's open, so `!= 'EMPTY'` implies the rescan's answer, which is
     all the header actually needs).
   - Add-on authors appending custom buttons to `ADDON_HT_header` (see item 11 below)
     need this property to tell *which* of their own declared space types is currently
     the active delegate, when they support more than one (e.g. show a viewport-mode
     button set vs. a node-editor button set depending on which panel set is showing).
     `addon_id` alone doesn't disambiguate that case.
4. **Stale doc-comment** on `BKE_paneltypes_addon_space_types_get` — still references
   the dynamic-itemf approach that was attempted and dropped (see the "itemf saga" in
   the implementation log). Fix the comment; the function itself is correct.
5. **Redundant double `BKE_screen_find_big_area` lookup** per layout pass in
   `addon_main_region_layout` (once inside `addon_delegate_spacetype_find`, once
   directly). Cheap, low priority.
6. **`addon_panel_poll_guarded` duplicates `rna_ui.cc`'s `panel_poll`** RNA-call
   boilerplate — but is actually *better* (distinguishes a raised exception from a
   plain `False` return, which the original doesn't). Extract into a shared helper and
   upgrade `rna_ui.cc`'s own `panel_poll` to the same safety, rather than just treating
   this as a duplicate to delete.

## Bigger, already-scoped items — not built

7. **Multi-window delegation gap.** `addon_delegate_spacetype_find()` only searches the
   current window's own screen (`BKE_screen_find_big_area` takes a single `bScreen*`).
   An editor open in the main window is invisible to an Add-on Editor area in a second
   window. Currently mitigated by hiding the Add-on Editor option entirely in non-main
   windows (commit `e8144bfee62`), not solved. Estimated ~half a day once the
   cache-signature widening and Python-side mirroring are counted.

   **Precedent researched and found** (`docs_ui/research_multiwindow_context.md`):
   `find_area_showing_render_result()` (`editors/render/render_view.cc:75-105`) is a
   real, shipping cross-window area search — loops `wm->windows`, pulls each window's
   own active screen via `WM_window_get_active_screen()`, walks its `areabase`,
   returns both the match and its owning window; its caller then explicitly raises the
   foreign window if the match wasn't local. Not a generic reusable utility (static,
   single-purpose, first-match not biggest-match) — new code would still be needed —
   but the *technique* is established, audited, shipping engine code, not a novel
   pattern this fork would be pioneering alone.

   **The window-mismatch risk has a precedented answer, not just an open question.**
   Read `bpy.context.temp_override()`'s C implementation
   (`python/intern/bpy_rna_context.cc`) closely: it independently supports
   `window=`/`screen=`/`area=`/`region=` overrides, explicitly including crossing
   windows, and applies them in a fixed order —
   `CTX_wm_window_set` → `CTX_wm_screen_set` → `CTX_wm_area_set` → `CTX_wm_region_set`
   (`bpy_rna_context_temp_override_enter`, ~line 344-355) — always swapping the window
   and screen alongside area/region when the target lives elsewhere, never area/region
   alone with a stale window. That directly answers the "does any of the 18 chained
   context accessors assume area-belongs-to-current-window" concern raised earlier:
   the established pattern is to swap all four together, not just two. A cross-window
   version of `addon_main_region_layout`'s swap should follow the same shape
   (`CTX_wm_window_set`/`CTX_wm_screen_set` added alongside the existing
   `CTX_wm_area_set`/`CTX_wm_region_set`), rather than needing a from-scratch audit of
   what happens when window and area disagree.
8. **Modal-operator warning.** Confirmed direction: block always, no "run anyway."
   Needs a wrapper operator (stash the real operator's idname + properties, warn
   instead of invoking) plus a post-layout walk of the region's `uiBlock`/`uiBut` lists
   to find and rebind buttons whose `wmOperatorType::modal != nullptr`. Not started.

   **Detection scope confirmed narrower than "all modal operators," and confirmed to
   stay coarse anyway.** Verified with two real operators (plan doc §4a, "The
   limitation is narrower than 'modal operators break'"): `TRANSFORM_OT_translate`
   (Blender's own G/R/S tool) genuinely breaks - its `convertViewVec()`
   (`transform.cc:185-231`) needs the real region's live `RegionView3D` projection state
   to scale a mouse delta correctly, and falls into the exact `"called in an invalid
   context"` failure already logged when it doesn't get one. The installed DreamUV
   add-on's `view3d.dreamuv_uvscale`, by contrast, works fine hosted - its `invoke()`/
   `modal()` never reads region/space-data at all, only raw window-space mouse deltas
   and mesh data. So a blanket `ot->modal != nullptr` check would false-positive on
   DreamUV's tool. Confirmed this doesn't change the plan: the actual distinguishing
   signal (does the operator body read region/view-space state) isn't staticly
   detectable without reading the operator's own code - the same class of problem the
   plan's "dynamic context routing" discussion already ruled out as unreliable to solve
   generically. Keep the coarse `ot->modal != nullptr` check and accept the false
   positives, rather than chasing a finer classifier that can't actually be built.

   **A real fix (not just a warning) is now precedented, not just plausible** (plan doc
   §4a, "How a vanilla add-on already solves the same problem"). Vanilla add-ons already
   solve this exact class of problem with `bpy.context.temp_override(area=..., region=
   ...)` wrapped around one operator call. Traced the C call chain to confirm the scoping
   holds even though the drag continues after the override exits:
   `WM_operator_call_py` returns immediately once `invoke()` reports `RUNNING_MODAL`
   (doesn't block for the drag), and `WM_event_add_modal_handler` captures its area/
   region from `CTX_wm_area(C)`/`CTX_wm_region(C)` synchronously at that moment - a
   one-time snapshot, not a live read - so a tightly-scoped swap is sufficient for the
   whole operator lifetime. The underlying primitive
   (`CTX_wm_window_set`/`_screen_set`/`_area_set`/`_region_set`) is plain C++, not
   Python-exclusive - `addon_main_region_layout` already calls two of the four directly.
   **What's actually missing is the interception point**: a Python author chooses where
   to wrap their own `bpy.ops.xxx()` call; a button in a re-hosted panel is invoked by
   Blender's generic dispatch (`interface_handlers.cc`), which has no hook for "this
   button lives in a re-hosted foreign panel." That's the same missing piece this item's
   warning feature already needs (the `uiBlock`/`uiBut` rebind walk) - only what the
   wrapper *does* once installed would differ (swap-and-invoke vs. warn-and-block). This
   can't be expected of the hosted add-ons themselves: `temp_override` is an opt-in
   escape hatch authors reach for only when deliberately doing something unusual: most
   operators never need it because vanilla Blender guarantees a panel only ever draws in
   its declared editor, so context is always correct by construction. This editor breaks
   that guarantee systematically, for panels whose authors never anticipated or
   accommodated it - so a fix, if built, has to happen automatically on our side, for
   every qualifying button. One simplification over the warning feature: the swap is
   harmless to apply even when unneeded (DreamUV would just get context it never asked
   for), so unlike the warning this needs no `ot->modal` classifier at all - just always
   wrap using the delegate the panel's own collection already resolved (the same data
   item 9 below already needs).
9. **Per-panel delegate resolution** (the ucupaint mixed-editor gap — an add-on
   registering panels for two different editor types only gets one delegate per area,
   so the non-matching panel set breaks). Recommended fix already recorded: resolve the
   delegate per panel from its own declared `bl_space_type`, not once per area. Not
   built. Directly synergistic with item 8's swap-based fix above — solving this one
   supplies the "which delegate does this panel/button need" data the other requires.
10. **Sentinel-byte picker marker** (`SPACE_ADDON_ID_PICK_MARKER = '\x01'`). Real
    fragility — already caused one shipped bug (the `-1`/`0x7FFF` sentinel-collision
    issue). Worth hardening (named constant, more validation) independent of whether
    the larger "separate header button" UX rework ever happens.
11. **Hosting-detection API for add-on authors** (2026-08-18, not built). Give hosted
    add-ons an official way to know they're running inside this editor, so cooperative
    authors can defensively guard the region/view-space-dependent code that breaks
    under delegation (the Transform-vs-DreamUV distinction from item 8/9) themselves,
    rather than every hosted add-on needing us to catch it after the fact.

    **`context.area.type` is not a valid signal and must not be exposed that way** -
    during a panel's own `poll()`/`draw()`, `CTX_wm_area(C)` has already been swapped to
    the delegate (`addon_main_region_layout`'s `CTX_wm_area_set`, bracketing exactly the
    `ED_region_panels_layout_ex` call where panel code runs), so it deliberately reads as
    the *native* editor type, not `'ADDON'`. That's the whole point of the swap - a naive
    `context.area.type == 'ADDON'` check would silently get the opposite of the right
    answer at exactly the moment it matters.

    **Recommended shape**: a small, self-contained, runtime-only WM-level flag (not
    persisted DNA), set/cleared symmetrically around the same `ED_region_panels_layout_ex`
    call already being bracketed for the area/region swap - mirrors an existing pattern,
    touches no shared code. Considered and rejected: exposing it via the generic
    screen-layer context (`screen_context.cc`, alongside `scene`/`object`/`mode`) would be
    the more "idiomatic" location since that layer already survives delegation untouched,
    but doing so means teaching genuinely generic core code about `SPACE_ADDON` by name -
    exactly the coupling the "context delegation made generic" refactor (§4a) already
    spent real effort removing. Stay self-contained instead.

    Worth exposing more than a bare boolean: also surface the resolved delegate's editor
    type and/or the curated add-on's own display name, so authors can write real
    messaging ("this panel works best in its native editor"), not just an on/off check.

13. **Header-hosting is already possible today for add-on authors — document it, don't
    build it** (2026-08-18). Distinct from item 11 (which is about *panel* code
    detecting delegation): `ADDON_HT_header` (`space_addon.py:187`) is an ordinary
    `bpy.types.Header` with `bl_space_type = 'ADDON'`, so the stock Blender extension
    mechanism already applies unmodified — any add-on can already do
    `bpy.types.ADDON_HT_header.append(my_draw_func)` and have it fire only while hosted
    in this editor (registering for `'ADDON'` *is* the detection signal, no flag
    needed). The draw func can further gate on which of its own panel sets is
    currently showing via `context.area.spaces.active.addon_id` (identity) plus
    `context.area.context_delegate_spacetype` (which of the add-on's supported space
    types is the active delegate, needed once an add-on declares panels for more than
    one — see item 3's second bullet). Nothing to build; worth writing up as an actual
    recipe in the plan doc / a future add-on-author-facing doc, since none of this is
    discoverable without reading our source.

    **Accessor correction, important enough to flag explicitly**: `context.space_data`
    is *not* the right accessor here, in header draw or anywhere else, including for
    the add-on's own hosted panels. `CTX_wm_space_data()` (`context.cc:990`) routes
    through `ctx_wm_area_effective()` unconditionally for every caller — it is not
    specific to the panel-draw swap in `addon_main_region_layout`. So
    `context.space_data` always resolves to the *delegate's* space when one is active,
    never to `SpaceAddon`, regardless of which region is currently drawing. The correct
    accessor for reading `SpaceAddon`'s own state (`addon_id`,
    `preferred_delegate_spacetype`) is always `context.area.spaces.active` — exactly
    what `ADDON_HT_header.draw()` itself already does (`space_addon.py:199`, with a
    comment explaining why) — never `context.space_data`. Worth a one-line callout in
    the plan doc so this doesn't have to be independently rediscovered later.

14. **Fold the "drawing nothing" notice into the empty-state panel** (2026-08-19, not
    built). There are two message systems saying overlapping things: the Python
    `ADDON_PT_empty_state` panel ("this add-on needs one of these editors open"), drawn
    when *no panel types were collected*, and a C++ overlay via
    `ED_region_info_draw_multiline` (`addon_main_region_draw`), drawn when panels were
    collected but produced nothing. They look different and are written in different
    languages for what the user experiences as one situation.

    **Why it wasn't done that way to begin with**: injecting the notice as a panel
    oscillates. The region is judged empty, so the panel is added, so the region is no
    longer empty, so the panel is removed - a notice that flickers rather than shows.

    **The fix that makes it work**, and it is the same trick the removed
    `only_fallback_panel` check used, for a better reason: have the "did anything draw"
    judgement *ignore this editor's own fallback panel*. Then the state is stable - real
    panels drawing nothing keeps the fallback up, real panels drawing keeps it away, and
    neither flips the other. That is inherent to the feature rather than a workaround.

    Sketch: `ui::region_panels_drew_nothing()` takes an idname to disregard (or this
    editor filters it out itself); `addon_panel_types_collect` injects the fallback when
    the list is empty *or* the previous pass drew nothing; the reason is exposed to
    Python as a read-only RNA bool on `SpaceAddon` backed by a getter reading
    `SpaceAddon_Runtime::drew_nothing` - **no DNA field needed**, RNA getters can read
    runtime state; `ADDON_PT_empty_state.draw()` branches on it. Then `art->draw` goes
    back to plain `ED_region_panels_draw` and the C++ overlay is deleted.

    Accept a one-pass lag: `drew_nothing` is computed after
    `ED_region_panels_layout_ex` returns, while the fallback panel draws inside it, so
    the panel reads the previous pass's value. Harmless for state this stable.

    Payoff beyond consistency: the message becomes Python, so wording and layout are
    iterable without a rebuild, and it can use the icons and structure the empty-state
    panel already has.

## Open, not yet investigated

12. **Global poll-failure blacklist** (`addon_poll_failed_get`, a static
    `Set<std::string>` shared across all areas/windows, permanently blacklisting a
    panel whose `poll()` raised until panel types change). A plausible concern was
    raised (cross-window side effects, silent permanent blacklisting) but not
    independently traced through to confirm or refute whether standard RNA-boundary
    error reporting alone would be a sufficient replacement.

---

## Not on this list, deliberately

- **Horizontal panel layout** — assessed and explicitly scoped out; would mean forking
  `ED_region_panels_layout_ex`'s internals. Recorded as a candidate future item, not a
  punch-list item.
- Every item Gemini's two audits raised that didn't survive verification (fabricated
  APIs, or findings already deliberately considered and rejected in the code's own
  comments) — see `audits/audit_reconciliation_summary.md` for the full accounting. Not
  repeated here.
