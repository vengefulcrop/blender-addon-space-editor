---
type: architecture
title: "Multi-Window Context Search"
description: "How Blender's own engine searches for an area across windows, and the gap this leaves for the Add-on Editor"
tags: [architecture, addon-editor, multi-window, context]
last_updated: 2026-09-12
---

# Multi-Window Context Search

## The gap

Context delegation (see [Context Delegation](./context_delegation.md))
uses `BKE_screen_find_big_area()`, which searches a single `bScreen`. Each
`wmWindow` owns its own active screen. `addon_delegate_spacetype_find()`
calls it via `CTX_wm_screen(C)`, which only returns the current window
screen.

If a user opens a new window and hosts an add-on there that needs, for
example, `NODE_EDITOR`, and the only open Node Editor is in the main
window, delegation does not find it, even though both windows belong to
the same Blender session. This is not fork-specific: every one of the 9
other upstream call sites of `BKE_screen_find_big_area()` shares the same
single-screen assumption. The Add-on Editor inherited the limitation
rather than introducing it.

## Does Blender's own engine already do cross-window area search?

Yes, in two places, neither a generic reusable utility, but both an
established pattern.

### `find_area_showing_render_result()` — strongest precedent

`source/blender/editors/render/render_view.cc:75-105`:

```cpp
static ScrArea *find_area_showing_render_result(bContext *C, Scene *scene, wmWindow **r_win)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area_render = nullptr;
  wmWindow *win_render = nullptr;

  /* find an image-window showing render result */
  for (wmWindow &win : wm->windows) {
    if (WM_window_get_active_scene(&win) != scene) {
      continue;
    }
    const bScreen *screen = WM_window_get_active_screen(&win);
    for (ScrArea &area : screen->areabase) {
      if (area.spacetype == SPACE_IMAGE) {
        SpaceImage *sima = static_cast<SpaceImage *>(area.spacedata.first);
        if (sima->image && sima->image->type == IMA_TYPE_R_RESULT) {
          area_render = &area;
          win_render = &win;
          break;
        }
      }
    }
    if (area_render) break;
  }
  *r_win = win_render;
  return area_render;
}
```

This is the exact shape of search cross-window delegation in the Add-on
Editor needs: iterate every `wmWindow`, pull the active `bScreen` of each
window, walk `areabase` looking for a specific space type satisfying an
extra predicate, and return both the matching `ScrArea*` and the `wmWindow*`
it lives in.

The caller, `render_view_open()` (`render_view.cc:212-224`), handles the
second half of the pattern: if the match lives in a different window than
the one the operator runs in, it raises that window
(`wm_window_raise(win_show)`) so the result is visible.

**Not a generic utility.** It is `static` and hardcoded to `SPACE_IMAGE`
plus a render-result predicate. It does not compute "biggest" the way
`BKE_screen_find_big_area` does. It takes the first match per window in
insertion order, and the first window with a hit. A reusable version for
the Add-on Editor would need to add size comparison across all candidates.

### `render_update.cc` — same mechanics, different goal

`ED_render_scene_update()` / `ED_render_view3d_pause_resume()`
(`render_update.cc:109-151, 193-201`) walk windows, then screens, then
areas, then regions, looking for `SPACE_VIEW3D` areas with live
`RegionView3D::view_render` engines, and build a throwaway `bContext` per
area via `render_view3d_context_create()` (`render_update.cc:68-80`),
which manually sets `CTX_wm_window/screen/area/region`.

This solves "notify every matching area in every window" (broadcast), not
"find the single best match." There is no size comparison and no "pick
one." But the manual `bContext` construction is the recipe for the
borrowing half of the problem: building a context object that borrows
state from an area in a non-current window.

### Weaker or non-matches, for completeness

- `ED_undo_object_editmode_validate_scene_from_windows()`
  (`ed_undo.cc:834-849`) is cross-window but only inspects `win.scene`/
  `win.view_layer`, never a window screen or areabase.

- The XR session code (`wm_xr_session.cc:246-257`) pins a single
  `wmWindow*` at session-start time rather than re-deriving "the right
  editor" later. It does no area/screen inspection.

- `bContext` itself has no cross-window accessor: `CTX_wm_window`,
  `CTX_wm_screen`, `CTX_wm_area`, and `CTX_wm_manager` only expose the
  single current window/screen/area/region. Any cross-window reasoning
  goes through `CTX_wm_manager(C)->windows` manually.

## What fixing the gap would cost

1. A new cross-window search helper in `blenkernel/intern/screen.cc`,
   iterating `wm->windows` and each active window screen, picking the
   biggest match across all of them (roughly 20-30 lines).
   `BKE_screen_find_big_area()` itself should not change signature: 9
   existing upstream callers depend on its current single-screen contract.

2. Two call sites in `addon_delegate_spacetype_find()` to swap over, plus
   `addon_screen_signature_get()` (the cache-invalidation signature), which
   currently only hashes the current screen's open space types.

3. The Python side (`_addon_has_open_delegate()`, header/empty-state
   messaging) checks `{area.type for area in context.screen.areas}`,
   single-screen too. Widening only the C++ side would reproduce the same
   C/Python disagreement the empty-state fix (see
   [Context Delegation](./context_delegation.md)) exists to prevent — both
   sides have to move together.

4. **The real risk**: once the delegate can resolve to an area in a
   different `wmWindow`, `CTX_wm_area_set`/`CTX_wm_region_set` would set
   the area/region of bContext to something that no longer belongs to
   `CTX_wm_window(C)`. No accessor in the 18-strong chain through
   `ctx_wm_area_effective()` audited against that mismatch, because that
   condition was never possible before. This needs a deliberate audit, not
   an assumption that it is fine.

**Not started.** Recorded as a deliberate future item, estimated at
roughly half a day to a day once the cache-signature widening, Python
mirroring, and the window-mismatch audit are all counted.

## Related

- [Context Delegation](./context_delegation.md)
