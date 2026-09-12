---
type: research
title: "Add-on Space Editor Pruning Audit"
description: "Merged internal and external audit of dead code, stale comments, and duplication in the Add-on Space Editor"
tags: [audit, addon-editor, pruning]
last_updated: 2026-09-12
sources:
  - id: internal-pruning
    resource: docs_ui/audits/audit_pruning_candidates.md
    title: "Internal audit: pruning candidates"
    author: claude-sonnet/agent
    last_modified: 2026-08-17T00:00:00Z
  - id: external-pruning
    resource: docs_ui/audits/Gemini_pruning_and_cleanup_candidates.md
    title: "External audit: pruning and cleanup candidates"
    author: gemini/reviewer
---

# Add-on Space Editor Pruning Audit

This file merges two audits of the same branch, `pyareas/addon-space-editor`
versus `main` (`027ef661892c1234de0eb8d44bf5bb189eb39d81`):

- The internal audit, `audit_pruning_candidates.md`, dated 2026-08-17. A Claude
  Sonnet agent wrote it.
- The external audit, `Gemini_pruning_and_cleanup_candidates.md`. Gemini wrote it
  as an external reviewer.

Where the two audits disagree, this file states both verdicts and names the
disagreement. See [`reconciliation_summary.md`](./reconciliation_summary.md) for
the cross-check that produced these verdicts, and
[`native_equivalents.md`](./native_equivalents.md) for the companion audit of
hand-rolled code versus native APIs.

## Method

**Internal audit.** Read the full `git diff main...pyareas/addon-space-editor --
source/ scripts/` (2540 lines), then cross-checked claims against:

- `source/blender/editors/space_addon/space_addon.cc` (900 lines)
- `source/blender/editors/space_addon/addon_intern.hh`
- `scripts/startup/bl_ui/space_addon.py` (about 490 lines)
- `source/blender/blenkernel/intern/{context,screen}.cc`, `BKE_screen.hh`
- `source/blender/makesrna/intern/{rna_screen,rna_space,rna_userdef}.cc`
- `source/blender/makesdna/{DNA_screen_types.h,DNA_space_types.h,DNA_userdef_types.h}`
- Commits `a61db0db55f` ("Add-on Editor: never delegate context around the empty-state fallback panel") (never delegate around the empty-state panel) and
  `af9bcda50c7` ("Add-on Editor: hide the Add-ons menu section in non-main windows") (hide the Add-ons menu section in non-main windows)
- `docs_ui/addon_space_editor_plan.md`, sections "Not fixed yet," "Accepted
  limitation," and "recorded, not built"

For every candidate, the internal audit grepped the whole `source/` and
`scripts/` trees to confirm the code is genuinely unused, not just locally quiet.

**External audit.** Reviewed the same branch and diff.

## Findings

### 1. Dead RNA property: `Area.context_delegate_spacetype`

**File**: `source/blender/makesrna/intern/rna_screen.cc`, around lines 2226 to
2239 in the diff (`rna_def_area`). The property's own comment states it exists
"so Python-drawn UI (presently only the Add-on editor) can show which real editor
an area is currently borrowing context from, without duplicating the borrowing
logic itself."

A repo-wide grep of `scripts/` for `context_delegate_spacetype` returns zero
matches. `scripts/startup/bl_ui/space_addon.py` computes what it needs (the
header info button, the empty-state panel's message) via its own independent
logic (`_addon_has_open_delegate`, `_addon_supported_spaces`), which checks
`area.type for area in context.screen.areas` directly, never reading this RNA
property.

**Why this is a pruning candidate**: exposed, documented, Python-readable API
surface whose sole stated justification is unused.

**Risk of removing**: low to medium. The underlying `ScrArea::context_delegate_spacetype`
C field and its use in `ctx_wm_area_effective`/`addon_main_region_layout` are
load-bearing and must stay. Only the RNA exposure (`RNA_def_property` block) is
unused. Confidence: high that it is unused, medium on whether removing it is
worth the churn given it is cheap to keep. Found by the internal audit only.

### 2. Stale doc comment on `BKE_paneltypes_addon_space_types_get`

**File**: `source/blender/blenkernel/BKE_screen.hh`, around lines 801 to 810. The
comment claims the function is "shared by the Add-on editor's own delegate
resolution and the `preferred_delegate_spacetype` RNA property's dynamic item
list."

But `preferred_delegate_spacetype` (`rna_space.cc`, `rna_def_space_addon`) uses
the plain, static `rna_enum_space_type_items`, the same static list `Area.type`
uses, not a dynamic `itemf`. The property's own comment explains why: an `itemf`
approach was tried, hit a compiler bug, and was dropped in favor of restricting
and prefixing the list in Python
(`_preferred_delegate_spacetype_items` in `space_addon.py`, a Python-side dynamic
`EnumProperty` items callback on the operator, not on the RNA property).

`BKE_paneltypes_addon_space_types_get` itself is real and used once, in
`addon_delegate_spacetype_find` (`space_addon.cc:470`). Only its doc comment is
wrong, because it claims a second consumer that does not exist in the shipped
code.

**Risk of fixing**: essentially none. This is a one-paragraph comment edit.
Confidence: high. Found by the internal audit only.

### 3. Triplicated "which panels belong to add-on X" scan

**Files and functions**:

1. `space_addon.cc:325`, `addon_panel_types_collect()` (collects `PanelType`
   copies)
2. `space_addon.cc:706`, `addon_has_registered_panels()` (boolean check)
3. `blenkernel/intern/screen.cc:377`, `BKE_paneltypes_addon_space_types_get()`
   (distinct `space_type` values)

All three walk the same shape: for each `SpaceType` (skip `SPACE_ADDON`), for
each `ARegionType` with region id `RGN_TYPE_UI` or `RGN_TYPE_WINDOW`, for each
`PanelType`, skip sub-panels (`pt.parent != nullptr`), resolve `addon_id` via
`BPY_class_module_name_get(pt.rna_ext.data)` (or `addon_panel_owner_get`, a thin
wrapper of the same call), and skip if `addon_id` does not match the target.

`addon_panel_owner_get` (`space_addon.cc:289`) is a one-line wrapper around
`BPY_class_module_name_get`, used by #1 and #2. #3 in `screen.cc` calls
`BPY_class_module_name_get` directly, because it cannot call
`addon_panel_owner_get`, which is `static` in a different translation unit.

**Internal audit**: three near-identical nested-loop bodies (about 15 to 20 lines
each) that must be updated in lockstep if the filtering rule changes. A shared
helper, for example a `BKE_screen.hh` function taking a predicate callback or
returning a `Vector<const PanelType*>`, would remove two of the three copies.
Risk of removing: low to medium, since the three call sites have slightly
different needs (full `PanelType` copies, a `Vector` of `short` space types, and
a short-circuiting boolean). Confidence: high that duplication exists, medium on
the best-shaped fix.

**External audit**: framed the same duplication as "duplicate Python/C++
discovery," and additionally recommended moving Python's crawling
(`_registered_panel_classes()`, `bpy.types.Panel.__subclasses__()`) to query the
C++ panel registry via RNA instead.

**Disagreement, resolved.** The filtering-logic duplication is real and both
audits agree on it. The external audit's recommendation to also move Python's
display-name resolution to the C++ side is an overreach: resolving a
human-readable add-on name needs `addon_utils.module_bl_info()` or extension
manifest data, reachable only from Python, and both consumers of that data (the
header, the empty-state panel) are already Python-side. Crossing the language
boundary for it would add plumbing without moving where the answer is needed.
Verdict: consolidate the space-type filtering logic; leave name resolution in
Python.

### 4. `"ADDON_PT_empty_state"` hardcoded in three places

**Files**:

- `space_addon.cc:178` (`addon_empty_state_paneltype_find`):
  `STREQ(pt.idname, "ADDON_PT_empty_state")`
- `space_addon.cc:568` (`addon_main_region_layout`, the `only_fallback_panel`
  check from the "never delegate around the empty-state fallback panel" commit):
  `STREQ(static_cast<PanelType *>(...)->idname, "ADDON_PT_empty_state")`
- `scripts/startup/bl_ui/space_addon.py`: `class ADDON_PT_empty_state(Panel):`,
  implicit `bl_idname` from the class name

Both C++ occurrences are independent literal string comparisons, not a shared
`#define`/`constexpr`. Python has no way to export its `bl_idname` to C++ short
of also hardcoding it, which is not unusual for Blender's own panel registration
idiom.

**Why this is a pruning candidate**: if `ADDON_PT_empty_state`'s `bl_idname` is
ever renamed in Python, both C++ literals must be updated by hand, with nothing
enforcing that at compile time. A silent partial rename would reintroduce the
crash `a61db0db55f` ("Add-on Editor: never delegate context around the empty-state fallback panel") fixed (`ADDON_PT_empty_state` swapped to a foreign space's
`context.area`, raising on `space.addon_id`).

**Risk of fixing**: low. The two C++ literals could become one
`constexpr const char *ADDON_PT_EMPTY_STATE_IDNAME = "ADDON_PT_empty_state";` in
`addon_intern.hh`. The cross-language coupling to Python's `bl_idname` cannot be
fully removed without a runtime registration handshake, which would be
over-engineering for one string. Confidence: high that the duplication exists,
low priority since both C++ call sites compile together and a stale rename would
likely be caught by testing. Found by the internal audit only.

### 5. Two `BKE_screen_find_big_area` scans per layout pass

**File**: `space_addon.cc`, inside `addon_main_region_layout` (around line 506
for the first call via `addon_delegate_spacetype_find`, around lines 575 to 580
for the second, direct call that gets `area_delegate`).

`addon_delegate_spacetype_find` internally calls `BKE_screen_find_big_area(screen,
..., 0)` to decide whether an editor of a candidate type is open. Immediately
after, `addon_main_region_layout` calls `BKE_screen_find_big_area(screen,
area_orig->context_delegate_spacetype, 0)` again to get the actual `ScrArea*` to
swap into.

**Why this is a candidate**: the second call re-resolves a fact the first call
already established, redoing the linear scan over `screen->areabase`.

**Risk of removing**: low value, low risk. `BKE_screen_find_big_area`'s scan is
over a typically small area list, and it runs once per main-region layout pass,
itself cached against several state signatures. Confidence: high that it is a
redundant call, low significance. Found by the internal audit only.

### 6. String sentinel byte hack (`SPACE_ADDON_ID_PICK_MARKER`)

**Files**: `DNA_space_types.h` defines `#define SPACE_ADDON_ID_PICK_MARKER
'\x01'`. `addon_space_subtype_set` (`space_addon.cc`) prefixes `\x01` onto
`SpaceAddon::addon_id` when the user selects "Add an Add-on...". `rna_screen.cc`
(`rna_Area_ui_type_update`) detects `saddon->addon_id[0] == '\x01'`, strips it
with `memmove`, and calls `WM_operator_name_call(C, "ADDON_OT_pick_and_host",
...)`.

- External audit: called this a fragile hack, and recommended triggering the
  picker directly through a standard header operator instead of intercepting
  enum assignments.
- Internal audit / reconciliation: verified the fragility is real. The plan
  doc's own log records a shipped bug from exactly this mechanism, a sentinel
  collision between `-1` and `0x7FFF`. But the "just use a standard header
  operator" framing skips why the mechanism is shaped this way: the picker entry
  must live inside the same enum dropdown as the curated add-on list, because
  the explicit design goal is one unified editor-type selector, not a list plus
  a separate button.

**Verdict, both audits agree the issue is real, disagree on the fix.** External
audit: replace the mechanism. Internal audit / reconciliation: harden it, for
example a named constant instead of a raw `'\x01'` byte plus more validation,
without necessarily abandoning the unified-dropdown design.

### 7. Global poll-failure blacklist (`addon_poll_failed_get`)

**File**: `space_addon.cc`. A static `Set<std::string> failed` stores id names of
panels whose `poll()` raised a Python exception when evaluated out of context.
`addon_panel_poll_guarded()` wraps RNA poll execution and permanently blacklists
matching panels until panel types change.

- External audit: flagged this as global state causing cross-window and
  cross-workspace side effects, and recommended relying on standard Blender
  error handling at the Python/RNA boundary instead.
- Verdict: **left genuinely open.** Neither the internal audit nor the
  reconciliation pass traced through closely enough to confirm or refute whether
  standard RNA-boundary error reporting alone would be sufficient. This poll
  wrapper exists specifically to stop a raising `poll()` from being re-invoked
  every redraw. A plain "let the standard reporting handle it" approach might
  just mean the same exception logs every frame instead of once. Worth a
  dedicated look before acting.

### 8. Indiscriminate redraw listener (`addon_main_region_listener`)

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

- External audit: called this an overhead problem, redrawing on every notifier
  in Blender, and recommended delegating to the active space type's listener or
  listening for specific categories only.
- Verdict: **incorrect, already considered and deliberately rejected.** The
  listener's own comment (`space_addon.cc:601-620`) states explicitly:
  forwarding to the delegate editor's own listener was the obvious alternative
  and is not safe, because several listeners (`space_clip.cc`, `space_action.cc`,
  `space_buttons.cc`) cast `params->area->spacedata` to their own space type, and
  substituting the delegate's area would silently give those listeners a
  different area than the region they are tagging. Redrawing too often is the
  cheaper accepted mistake. This is exactly the alternative the external audit
  proposes, already tried in reasoning and rejected for a concrete, cited cause.
  Note: an earlier internal pass in the same session called this finding "worth
  taking seriously" without cross-checking it against this already-read comment.
  That was a mistake, corrected in the reconciliation.

### 9. Heavyweight DNA persistence for the curated editor list

**Files**: `DNA_userdef_types.h`, `readfile.cc`, `writefile.cc`,
`versioning_userdef.cc`, `rna_userdef.cc`. Adds `struct bAddonEditor`, a linked
list on `UserDef`, `UserDef::addon_editors`, `active_addon_editor_index`,
`addon_editor_max_visible`, and `USER_ADDON_EDITOR_SHOW_BUNDLED` in `uiflag2`.

- External audit: called this schema bloat, and recommended `IDProperty` or
  Python `AddonPreferences` instead of a dedicated DNA struct.
- Verdict: **incorrect.** `bAddonEditor` was deliberately modeled on the
  existing `bAddon` struct, already in `DNA_userdef_types.h`, which is Blender's
  own established convention for a persistent list of user-curated add-on-related
  entries in `UserDef`. The claim that DNA structs are unconventional here
  contradicts the precedent already in the same file. This follows existing
  house style, not novel schema bloat.

## Doc-versus-code discrepancy

Not a code finding, but flagged per the guidance to not trust a design doc's own
rationale. `docs_ui/addon_space_editor_plan.md` (lines 503 to 518) describes
"Mixed-editor add-ons break the single, area-wide delegate" as "Not fixed yet,
recommended approach recorded for when it is," with the recommended fix being to
resolve the delegate per panel rather than once per area.

The shipped `addon_panel_types_collect` (`space_addon.cc:325`, comment block
around lines 381 to 408) does address the mixed-editor case, but through a
different mechanism than the doc's recorded future plan: it resolves one
delegate per area, as before, but drops any panel whose `bl_space_type` does not
match that delegate, rather than showing it against the wrong space data. This is
a real fix for the reported crash, just not the per-panel-delegate approach the
doc says is still pending. No code action needed. The doc's "Not fixed yet"
framing is stale relative to what shipped.

## Examined and found not to be pruning candidates

- **`ScrArea::context_delegate_spacetype` plumbed through about 15
  `CTX_wm_space_*` accessors**, even though only `SpaceAddon` sets it today. This
  looks like speculative generality, but it is not: an add-on's panels can
  legitimately target any of those space types (Node Editor, Image Editor,
  View3D, Sequencer, and others), so the breadth is required for arbitrary
  third-party add-ons to work, not merely convenient. Only one setter exists
  (`space_addon.cc:530`); `area.cc:2887` only clears it.
- **No leftover Python-push panel registry, no dead itemf scaffolding.**
  Consistent with the plan doc's note that these were built, found unnecessary,
  and removed cleanly. Grepping the current tree found no trace of either.
- **`ADDON_OT_supported_editors_info.execute()` returning `{'CANCELLED'}`** with
  a comment "Exists for its tooltip; nothing to do on click." A standard Blender
  idiom, an operator that exists solely to host a dynamic `description()`
  classmethod for a header info-icon button. Not a pruning candidate.
- **`addon_panels_type_detach` / panel-type-copy-reuse machinery** in
  `addon_panel_types_collect`. Looks elaborate (previous-list set-aside,
  pop-by-name, detach-before-free) but each piece is justified by a concrete
  failure mode: preserving collapsed/drag state across re-collection, and not
  leaving `Panel::type` dangling. No dead branches found.

## Summary

| Confidence | Count | Findings |
|---|---|---|
| High | 4 | #1 (dead RNA property), #2 (stale comment), #3 (triplicated scan), #4 (magic string duplication) |
| Medium | 1 | #3's best fix shape is less certain than its existence |
| Low significance, but confirmed | 1 | #5 (redundant area lookup) |
| Real issue, disputed fix | 1 | #6 (sentinel byte: harden versus replace) |
| Left genuinely open | 1 | #7 (poll-failure blacklist) |
| External claim, incorrect on verification | 2 | #8 (redraw listener), #9 (DNA persistence) |

No findings sat at "not sure, could be load-bearing" confidence in the internal
audit. Everything it flagged was verified against actual current usage, grepped
for callers and readers, not inferred from the design doc alone.
