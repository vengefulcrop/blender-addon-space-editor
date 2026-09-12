---
type: operation
title: "Add-on Space Editor — Session Handoff"
description: "Current state of the pyareas/addon-space-editor branch: what works, what is in progress, and what is next"
tags: [addon-space-editor, handoff]
last_updated: 2026-09-12
---

# Session Handoff

Branch: `pyareas/addon-space-editor`. Based on
`docs_ui/punch_list.md`, `docs_ui/legacy/code_review.md`,
`docs_ui/addon_space_editor_testing.md`, and the git log up to commit
`9ba0d6b569c` ("Add-on Editor: tree row activation, bundled add-ons, and a drawing-nothing notice").

## What works

The Add-on Space Editor (`SPACE_ADDON`) hosts real add-on panels in a
real editor area, with context delegation borrowing an open editor's
context so hosted panels poll and draw correctly. The following defects
are fixed. See [operations/bugfix.md](../operations/bugfix.md) for each
item's full symptom and cause.

- `U.addon_editors` now saves and loads correctly, with proper blenloader
  support and a template swap (bugfix item 1).
- The reused userpref flag bit is versioned and cleared on load
  (bugfix item 2).
- Panel enumeration correctly filters to registered, non-duplicated panel
  classes on the Python side (bugfix item 3).
- Panel layout, collapse state, and drag order survive a file load and a
  mid-session screen-signature change (bugfix item 4).
- Mixed-editor add-ons, such as ucupaint, no longer get polled against
  the wrong space, and no longer hang Blender (bugfix items 6 and 7).
- Switching an Add-on editor area to a stock editor type no longer
  crashes (bugfix item 8).
- The empty-state context-delegation crash and the multi-window dropdown
  leak are both fixed (bugfix items 9 and 10).
- Context delegation itself moved from a `SpaceAddon`-specific field to a
  generic `ScrArea::context_delegate_spacetype`, so `blenkernel` no
  longer names this editor by string, except in `CTX_wm_space_addon()`'s
  own one-line body.
- The curated-list add-on picker was removed. A tree view replaces it.
  See commit `06d699437bb` ("Add-on Editor: remove the curated-list
  picker, tree replaces it").

## What is in progress

The most recent four commits on the branch touch context delegation and
the tree-based picker directly:

- `9ba0d6b569c` ("Add-on Editor: tree row activation, bundled add-ons, and a drawing-nothing notice")
- `06d699437bb` ("Add-on Editor: remove the curated-list picker, tree
  replaces it"). This changes the scope of the sentinel-byte picker
  marker, `SPACE_ADDON_ID_PICK_MARKER = ''`. See
  [operations/todo.md](../operations/todo.md) item 25.
- `d60d293448d` ("Add-on Editor: scope context delegation to panel callbacks, fix extension names")
- `5608e75d78e` ("UI: let ED_region_panels_layout_ex run panel callbacks under an overridden context")

The two commits `d60d293448d` and `5608e75d78e` are **not** the
per-panel delegate resolution that `docs_ui/punch_list.md` item 9 asked
for. A code trace on 2026-09-12 settled this.

They narrowed the *scope* of the context override. The override now wraps
each panel callback, instead of the whole layout pass. The *resolution*
did not change. `space_addon.cc:571-583` still builds one
`PanelDrawContextOverride` per layout pass, from the single
`ScrArea::context_delegate_spacetype` field.

The ucupaint mixed-editor gap stays open. See
[operations/todo.md](../operations/todo.md) item 13 for the evidence and
the three parts a real fix needs, and
[architecture/context_delegation.md](../architecture/context_delegation.md#single-delegate-per-area-not-per-panel)
for the design note.

The commits do deliver part of what item 8 asked for. Panel callbacks now
run under an overridden context, in the same
`CTX_wm_area_set`/`CTX_wm_region_set` shape that
`addon_main_region_layout` already used.

Neither commit's message mentions the modal-operator warning wrapper or
the `uiBlock`/`uiBut` rebind walk, so the modal-operator handling in
[operations/todo.md](../operations/todo.md) items 10 to 12 is treated as
still not built.

## What is next

In priority order, from [operations/todo.md](../operations/todo.md):

1. Confirm the scope of the two recent context-delegation commits against
   punch-list items 8 and 9, and close out whichever parts they cover.
2. Multi-window delegation: widen `addon_delegate_spacetype_find()` to
   search every window's screen, following the
   `CTX_wm_window_set`/`_screen_set`/`_area_set`/`_region_set` order
   established in `bpy_rna_context_temp_override_enter`.
3. Modal-operator handling: the warning wrapper and the `uiBlock`/`uiBut`
   rebind walk, or the `temp_override`-based real fix, whichever is
   chosen.
4. Small cleanups: the shared panel-scan helper, the shared
   `"ADDON_PT_empty_state"` constant, and the shared `panel_poll` guard
   (`operations/todo.md` section 1).
5. Per-panel hosting design (`operations/todo.md` section 4), which
   absorbs the earlier per-area slot design.
6. The hosting-detection API and its author-facing documentation
   (`operations/todo.md` sections 5 and 6).

## Test status

No automated tests exist. Three manual scripts exist under `docs_ui/`
(`test_addon_editor.py`, `test_addon_editor_delegate.py`,
`test_addon_editor_demo.py`). None were executed as part of this review.
See [operations/testing.md](../operations/testing.md) for what each
covers and which `ui_simulate` tests to write first.

## Publishing notes

Not a code task, but recorded for whoever handles distribution: the GPL
permits publishing this fork with binaries, but the executable and
project must be renamed away from "Blender" for trademark reasons, the
bundled third-party license notices must ship alongside the binary, and
the fork should be published on top of an unmodified upstream commit
with `origin` kept pointed at
`projects.blender.org/blender/blender`. Full reasoning, including the
minimal "overlay" distribution analysis, is in
`docs_ui/legacy/code_review.md` §4.
