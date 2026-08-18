# Research: does Blender's own engine do cross-window area/context resolution?

## What was searched

Grepped `source/blender` for `wm->windows` iteration combined with per-window
`bScreen`/`areabase` inspection in the same loop (as opposed to loops that only
touch window-level state). Followed up by reading the full bodies of every
hit in `render_view.cc`, `render_update.cc`, `ed_undo.cc`, and
`wm_xr_session.cc`, plus checked `context.cc` for a `CTX_wm_windows`-style
"search all windows" accessor (none exists — `bContext` only exposes the
single "current" window/screen/area/region via `CTX_wm_window/screen/area`).

## Findings, ranked by relevance

### 1. `find_area_showing_render_result()` — source/blender/editors/render/render_view.cc:75-105 — STRONGEST MATCH

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

This is exactly the shape of search the Add-on Editor problem needs: iterate
every `wmWindow` in `wm->windows`, pull each window's own active `bScreen`,
walk its `areabase` looking for a specific space type (here `SPACE_IMAGE`)
satisfying an extra predicate (showing the render-result image), and return
both the matching `ScrArea*` and the `wmWindow*` it lives in.

The caller, `render_view_open()` (render_view.cc:212-224), then does the
second half of the pattern this fork needs:

```cpp
wmWindow *win_show = nullptr;
area = find_area_showing_render_result(C, scene, &win_show);
...
/* if area found in other window, we make that one show in front */
if (win_show && win_show != CTX_wm_window(C)) {
  wm_window_raise(win_show);
}
```

i.e. once a cross-window match is found, it explicitly handles the case
where the match lives in a *different* window than the one the operator is
running in, and raises that window so the result is visible. This is the
"found the editor, but it's in another window" case the Add-on Editor's
delegate-search inherits from `BKE_screen_find_big_area`'s single-screen
limitation.

**Verdict: directly reusable as a structural template.** It is not a
generic utility (it's `static` and hardcoded to `SPACE_IMAGE` +
render-result predicate), but the pattern — outer loop over `wm->windows`,
inner loop over that window's `screen->areabase`, filtered by spacetype and
an arbitrary extra predicate, returning both area and owning window,
capped with a "raise the other window if the match isn't local" step — maps
almost one-to-one onto "find the biggest open editor of type X across every
window, and if it's not in the current window, raise that window before
borrowing its context." Note it does not compute "biggest" (unlike
`BKE_screen_find_big_area`), it takes the first match per window in
insertion order and the first window with a hit; a reusable version would
need to add size comparison across all candidates the way
`BKE_screen_find_big_area` does within one screen.

### 2. `ED_render_scene_update()` / `ED_render_view3d_pause_resume()` — source/blender/editors/render/render_update.cc:109-151, 193-201 — STRONG MATCH, DIFFERENT GOAL

```cpp
for (wmWindow &window : wm->windows) {
  bScreen *screen = WM_window_get_active_screen(&window);
  for (ScrArea &area : screen->areabase) {
    if (area.spacetype == SPACE_VIEW3D) {
      ED_render_view3d_update(update_ctx->depsgraph, &window, &area, updated);
    }
  }
}
```

and the pause/resume variant walks windows -> screen -> areabase -> regions
looking for `SPACE_VIEW3D` areas with live `RegionView3D::view_render`
engines, building a throwaway `bContext` per area via
`render_view3d_context_create()` (render_update.cc:68-80), which manually
sets `CTX_wm_window/screen/area/region` — i.e. it constructs an ad hoc
per-area context exactly the way the Add-on Editor would need to construct
a "borrowed" context from a cross-window match.

**Verdict: same mechanics, different goal.** This solves "notify/update
*every* matching area in *every* window" (broadcast), not "find *the*
single best/authoritative matching area." There's no size comparison, no
"pick one," no "raise the window." But the two nested loops plus the
manual `bContext` construction (`CTX_wm_window_set` /
`CTX_wm_screen_set(C, WM_window_get_active_screen(window))` /
`CTX_wm_area_set` / `CTX_wm_region_set`) is precisely the recipe for
building a context object that borrows state from an area sitting in a
non-current window — useful as a second precedent for the "borrowing"
half of the fork's problem, independent of the "finding" half.

### 3. `ED_undo_object_editmode_validate_scene_from_windows()` — source/blender/editors/undo/ed_undo.cc:834-849 — WEAKER MATCH

```cpp
for (wmWindow &win : wm->windows) {
  if (win.scene == scene_ref) {
    *scene_p = win.scene;
    *view_layer_p = WM_window_get_active_view_layer(&win);
    return;
  }
}
```

Cross-window, and resolves a context member (a `ViewLayer*`) by searching
windows — but it only ever inspects `win.scene`/`win.view_layer`, never
`win`'s screen or areabase. It's solving "which window (and therefore which
view layer) currently has this scene active," not "which area of type X."
**Verdict: structurally cross-window, but not an area search — narrower
than it first looks. Confirms the "iterate windows to find one" vs.
"iterate windows AND their areas" distinction called out in the task: this
is the former.**

### 4. XR session code — source/blender/windowmanager/xr/intern/wm_xr_session.cc:246-257 — NOT A MATCH

```cpp
wmWindow *wm_xr_session_root_window_or_fallback_get(const wmWindowManager *wm,
                                                     const wmXrRuntimeData *runtime_data)
{
  wmWindow *xr_win = CTX_wm_window(runtime_data->b_context);
  if (xr_win && BLI_findindex(&wm->windows, xr_win) != -1) {
    return xr_win;
  }
  return static_cast<wmWindow *>(wm->windows.first);
}
```

XR does not resolve "the corresponding desktop editor" by searching areas
across windows at all. It just remembers the single `wmWindow*` that was
active when the XR session started (stored on `runtime_data->b_context`),
validates that pointer is still in `wm->windows` (i.e. the window wasn't
closed), and falls back to `wm->windows.first` otherwise. There is no
area/screen inspection anywhere in this file's window handling.
**Verdict: not applicable.** XR sidesteps the whole problem by pinning a
window reference at session-start time rather than re-deriving "the right
editor" later.

### 5. `bContext` itself — source/blender/blenkernel/intern/context.cc — NO cross-window accessor

Grepped for `CTX_wm_windows` and any "search all windows" accessor: none
exists. `bContext` only exposes the single current
`window`/`screen`/`area`/`region`/`manager` (`CTX_wm_window`,
`CTX_wm_screen`, `CTX_wm_area`, `CTX_wm_manager`, etc.). Any cross-window
reasoning has to go through `CTX_wm_manager(C)->windows` manually, as all
the examples above do — there's no built-in "context for all windows"
concept.

### Other loops checked and ruled out as non-matches

`wm_files.cc`, `wm_window.cc`, `screen_edit.cc`, `workspace_edit.cc`,
`scene_edit.cc`, `layer.cc`, `image.cc` all contain `wm->windows` loops, but
on inspection each is one of: closing/repositioning windows, syncing
`win->scene`/`win->workspace` pointers directly (not searching *inside* the
screen for a specific area type), or freeing per-window runtime data. These
are "iterate windows to do something window-level," the exact category the
task said to exclude.

## Bottom line

Precedent for the *finding* half does exist, and it's a strong one:
`find_area_showing_render_result()` in `render_view.cc` is a genuine,
shipping, cross-window area search — window loop, per-window screen/areabase
inspection, spacetype + predicate filter, and an explicit "raise the other
window" step when the match isn't local. `render_update.cc`'s scene-update
broadcast additionally demonstrates the *borrowing* half: manually
constructing a `bContext` (window/screen/area/region) for an area living in
a non-current window. Neither is a generic, reusable utility — both are
static, single-purpose functions hardcoded to one spacetype and one
selection policy (first-match, not biggest-match) — so building a
`BKE_screen_find_big_area`-style "biggest area of type X across all windows"
helper for the Add-on Editor would still be new code, not a call to an
existing function. But it would not be inventing an unprecedented pattern:
the core technique (loop `wm->windows`, inspect each window's own
`WM_window_get_active_screen()`/`areabase`, build/borrow a `bContext` from
whatever is found, raise the foreign window if needed) is already
established, audited, shipping engine code solving essentially the same
"editor might be in a different window than the one I'm running in"
problem. That lowers the audit burden relative to a from-scratch design:
the fork's cross-window delegation can be justified as "same pattern
`render_view.cc` already uses for render-result routing," not as a novel
mechanism the reviewer has to evaluate cold.
