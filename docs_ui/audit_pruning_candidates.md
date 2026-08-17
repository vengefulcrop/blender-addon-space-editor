# Add-on Space Editor — Pruning Audit

Date: 2026-08-17. Branch audited: `pyareas/addon-space-editor` (base: `main`).

## Method

Read the full `git diff main...pyareas/addon-space-editor -- source/ scripts/` (2540
lines) in its entirety, then cross-checked specific claims against the current state of:

- `source/blender/editors/space_addon/space_addon.cc` (900 lines)
- `source/blender/editors/space_addon/addon_intern.hh`
- `scripts/startup/bl_ui/space_addon.py` (~490 lines)
- `source/blender/blenkernel/intern/{context,screen}.cc`, `BKE_screen.hh`
- `source/blender/makesrna/intern/{rna_screen,rna_space,rna_userdef}.cc`
- `source/blender/makesdna/{DNA_screen_types.h,DNA_space_types.h,DNA_userdef_types.h}`
- The two recent commits `421cdb04abf` (never delegate around the empty-state panel)
  and `e8144bfee62` (hide Add-ons menu section in non-main windows)
- `docs_ui/addon_space_editor_plan.md`, specifically the sections marked "Not fixed
  yet", "Accepted limitation", and "recorded, not built"

For every candidate I grepped the whole `source/` and `scripts/` trees to confirm
whether something is genuinely unused rather than merely locally quiet. Findings are
ranked by how much they'd actually simplify the code if removed/merged.

Overall impression: the implementation is unusually well-documented and, structurally,
tight — no leftover Python-push registry, no half-finished itemf scaffolding was found
in the shipped code (those were apparently removed cleanly, matching the plan doc's
account). The findings below are real but modest: one dead RNA property, one stale
doc-comment referencing a design that was dropped, and one instance of triplicated
scan logic across two files.

---

## Finding 1 — `Area.context_delegate_spacetype` RNA property is defined, documented as
purpose-built for Python UI, and never read from any Python code

**File**: `source/blender/makesrna/intern/rna_screen.cc`, lines ~2226–2239 in the diff
(search `context_delegate_spacetype` in `rna_def_area`).

```cpp
/* Generic per-area context-resolution override - see #ScrArea::context_delegate_spacetype.
 * Exposed read-only so Python-drawn UI (presently only the Add-on editor) can show which
 * real editor an area is currently borrowing context from, without duplicating the
 * borrowing logic itself. 'EMPTY' means the area is not delegating. */
prop = RNA_def_property(srna, "context_delegate_spacetype", PROP_ENUM, PROP_NONE);
...
```

A repo-wide grep of `scripts/` for `context_delegate_spacetype` returns zero matches.
`scripts/startup/bl_ui/space_addon.py` computes what it needs to show (the header
info button, the empty-state panel's "editor X is required" message) via its own
independent Python-side logic (`_addon_has_open_delegate`, `_addon_supported_spaces`),
never by reading this RNA property. The comment's stated purpose — "so Python-drawn
UI... can show which real editor an area is currently borrowing context from" — is not
actually happening anywhere in the shipped code.

**Why this is a pruning candidate**: this is exposed, documented API surface (readable
from Python, from the RNA doc generator, from any script) whose sole justification is
unused. It is speculative flexibility: built for a UI affordance ("tell the user which
editor they're currently borrowing from") that was apparently never wired up on the
Python side, which instead reimplements the same fact independently and more directly
(`_addon_has_open_delegate` checks `area.type for area in context.screen.areas`
directly rather than reading `context_delegate_spacetype`).

**Risk of removing**: low-to-medium. The underlying `ScrArea::context_delegate_spacetype`
C field and its use in `ctx_wm_area_effective`/`addon_main_region_layout` are very much
load-bearing — that part must stay. Only the *RNA exposure* (the `RNA_def_property`
block in `rna_def_area`) is the unused part. Removing it is safe as far as this repo's
own code is concerned; the only external risk is if some out-of-tree script already
depends on it, which is unlikely this soon after introduction (subversion 11, "2026").
Confidence: **high** that it's unused; **medium** on whether removing it is worth the
churn given it's cheap to keep and harmless if never called.

---

## Finding 2 — Stale doc-comment on `BKE_paneltypes_addon_space_types_get` describes a
design that was not shipped (a "dynamic item list" that doesn't exist)

**File**: `source/blender/blenkernel/BKE_screen.hh`, lines ~801–810.

```cpp
/**
 * Distinct #PanelType::space_type values among the currently registered top-level panels
 * attributed to \a addon_id (by top-level Python module, see #BPY_class_module_name_get).
 * Empty if \a addon_id is empty or has no such panels.
 *
 * Shared by the Add-on editor's own delegate resolution and the `preferred_delegate_spacetype`
 * RNA property's dynamic item list, so the two cannot disagree on which editor types an
 * add-on declares panels for.
 */
Vector<short> BKE_paneltypes_addon_space_types_get(const char *addon_id);
```

But `preferred_delegate_spacetype` (`rna_space.cc`, `rna_def_space_addon`) is defined
with plain, static `rna_enum_space_type_items` — the same static list `Area.type`
uses — not a dynamic `itemf`:

```cpp
prop = RNA_def_property(srna, "preferred_delegate_spacetype", PROP_ENUM, PROP_NONE);
RNA_def_property_enum_sdna(prop, nullptr, "preferred_delegate_spacetype");
RNA_def_property_enum_items(prop, rna_enum_space_type_items);
```

The RNA property's own comment even explains why: it deliberately stays a "plain sdna
get/set over the ordinary, static... list... rather than a dynamic itemf restricted to
this add-on's own declared types" — matching the task brief's note that an itemf
approach was tried, hit a compiler bug, and was dropped in favor of doing the
restriction/prefixing in Python (`_preferred_delegate_spacetype_items` in
`space_addon.py`, which is a Python-side dynamic `EnumProperty` items callback on the
*operator*, not on the RNA property itself).

`BKE_paneltypes_addon_space_types_get` is real and is used exactly once, in
`addon_delegate_spacetype_find` (`space_addon.cc:470`) — so the function itself is not
dead. Only its doc comment is wrong: it claims a second consumer ("the RNA property's
dynamic item list") that does not exist in the code as shipped.

**Why this is a pruning candidate**: exactly the kind of stale rationale the task
description warns about — a comment whose justification survived a design change
(itemf → static enum + Python-side operator items) without being updated. A reader
trusting this comment would go looking for a dynamic itemf on `preferred_delegate_spacetype`
that isn't there.

**Risk of removing/fixing**: essentially none — this is a one-paragraph comment edit
(drop the "and the RNA property's dynamic item list" clause, or point instead at
`_preferred_delegate_spacetype_items` in `space_addon.py`, which is the real second
consumer of "what editor types does this add-on declare"). No code changes needed.
Confidence: **high**.

---

## Finding 3 — The "top-level panel belongs to add-on X" scan is implemented three
times with near-identical bodies, in two different files

**Files / functions**:
1. `source/blender/editors/space_addon/space_addon.cc:325` –
   `addon_panel_types_collect()` (collects `PanelType` copies)
2. `source/blender/editors/space_addon/space_addon.cc:706` –
   `addon_has_registered_panels()` (boolean "does any exist")
3. `source/blender/blenkernel/intern/screen.cc:377` –
   `BKE_paneltypes_addon_space_types_get()` (distinct `space_type` values)

All three do the same walk:
```
for each SpaceType (skip SPACE_ADDON):
  for each ARegionType where regionid in {RGN_TYPE_UI, RGN_TYPE_WINDOW}:
    for each PanelType:
      skip if pt.parent != nullptr   // sub-panels
      addon_id = BPY_class_module_name_get(pt.rna_ext.data)  // or addon_panel_owner_get, a thin wrapper of the same call
      skip if addon_id != target
      ... do something with pt
```

`addon_panel_owner_get` (space_addon.cc:289) is itself just a one-line wrapper around
`BPY_class_module_name_get`, used by both #1 and #2 in `space_addon.cc`, while #3 in
`screen.cc` calls `BPY_class_module_name_get` directly (it can't call
`addon_panel_owner_get`, which is `static` in a different translation unit) — so even
that thin wrapper doesn't unify things across the file boundary that matters most.

**Why this is a pruning candidate**: three near-identical nested-loop bodies (about
15-20 lines each) that would need to be updated in lockstep if the filtering rule ever
changes (e.g., a new region type becomes eligible, or the sub-panel exclusion rule
changes). This is exactly the "same check done in more than one place" pattern the task
asks to look for. A shared helper — e.g. a `BKE_screen.hh` function taking a callback
`bool(const PanelType&)` or returning a `Vector<const PanelType*>` of top-level panels
attributed to `addon_id`, from which both `addon_panel_types_collect`'s copy step and
`addon_has_registered_panels`'s any-match check could be built — would remove two of
the three copies (screen.cc's version already lives in blenkernel and could become the
single shared implementation; `space_addon.cc`'s two could call into it instead of
re-scanning).

**Risk of removing**: low-medium. The three call sites have slightly different needs
(one wants full `PanelType` copies for reuse across redraws, one wants a Vector of
`short` space types, one wants a short-circuiting boolean), so unifying them requires a
bit of design care to avoid over-abstracting into a fourth, more complex shared
function — but the walk itself (which spaces/regions/panels count) is identical and
safe to factor out. Confidence: **high** that the duplication exists; **medium** on the
best-shaped fix (a naive merge risks becoming its own "speculative flexibility" if it
grows parameters for hypothetical future callers).

---

## Finding 4 — `"ADDON_PT_empty_state"` is a magic string hardcoded independently in
three places (two C++, one Python), with only comments — not a shared constant — tying
them together

**Files**:
- `source/blender/editors/space_addon/space_addon.cc:178` (in
  `addon_empty_state_paneltype_find`): `if (STREQ(pt.idname, "ADDON_PT_empty_state"))`
- `source/blender/editors/space_addon/space_addon.cc:568` (in
  `addon_main_region_layout`, the `only_fallback_panel` check added by the recent
  "never delegate context around the empty-state fallback panel" commit):
  `STREQ(static_cast<PanelType *>(...)->idname, "ADDON_PT_empty_state")`
- `scripts/startup/bl_ui/space_addon.py`: `class ADDON_PT_empty_state(Panel):` /
  `bl_idname = "ADDON_PT_empty_state"` (implicit, from the class name, per Blender's
  registration convention)

This is the exact duplication flagged as worth checking in the task brief. It is
real: both C++ occurrences are independent literal string comparisons, not a shared
`#define`/`constexpr`. The Python side has no way to "export" its `bl_idname` to C++
short of also hardcoding it (Blender's own panel-registration idiom already ties a
class's `bl_idname` to a plain string at the Python level, so this isn't unusual by
itself).

**Why this is a pruning candidate**: not dead code, but exactly the kind of coupling
that can silently drift — if `ADDON_PT_empty_state`'s `bl_idname` is ever renamed in
Python (e.g. during a refactor of `space_addon.py`), both C++ string literals must be
updated by hand, and nothing enforces that at compile time; a typo or partial rename
would silently break the "never delegate around the fallback panel" fix from the recent
commit (the crash-safety code from `421cdb04abf`), reintroducing the very bug that
commit fixed (`ADDON_PT_empty_state` getting swapped to `context.area` from a foreign
space, raising on `space.addon_id`).

**Risk of "fixing"**: low, but this is more a "worth centralizing" note than a
"prune" — the two C++ literals could trivially become one
`constexpr const char *ADDON_PT_EMPTY_STATE_IDNAME = "ADDON_PT_empty_state";` in
`addon_intern.hh`, removing the duplication *within* C++. The cross-language coupling to
Python's `bl_idname` cannot be fully eliminated without a runtime registration
handshake, which would be over-engineering for a single string. Confidence: **high**
that the duplication exists and is worth a one-line fix; **low** priority since it's
currently only two call sites in one file, both compiled together, so a stale rename
would very likely be caught by testing before shipping (it's not silently-drifting
config in the way a serialized string would be).

---

## Finding 5 — Two `BKE_screen_find_big_area` scans per layout pass for the same delegate
type

**File**: `source/blender/editors/space_addon/space_addon.cc`, inside
`addon_main_region_layout` (~line 506 for the first call via
`addon_delegate_spacetype_find`, ~line 575-580 for the second, direct call to get
`area_delegate`).

`addon_delegate_spacetype_find(C, addon_id, preferred_spacetype)` internally calls
`BKE_screen_find_big_area(screen, ..., 0)` to decide *whether* an editor of a candidate
type is open (to pick `delegate_spacetype`, a `short`). Immediately after, once
`area_orig->context_delegate_spacetype` is set to that same value,
`addon_main_region_layout` calls `BKE_screen_find_big_area(screen,
area_orig->context_delegate_spacetype, 0)` *again* — this time to get the actual
`ScrArea*` to swap `CTX_wm_area`/`CTX_wm_region` into.

**Why this is a candidate**: the second call re-resolves a fact the first call already
established (that an area of `delegate_spacetype` exists) and re-does the linear scan
over `screen->areabase` to find it. It's not incorrect (the screen can't change between
the two calls within one layout pass), just a redundant second walk of the same list
for the same type.

**Risk of removing**: low value / low risk either way. `BKE_screen_find_big_area`'s
scan is over a screen's area list, typically small (single digits), and this only runs
once per main-region layout pass (which is itself cached against several state
signatures, so it's not per-panel or per-frame). This is a genuine micro-duplication
but not worth restructuring the API for — flagging for completeness per the task's
"redundant computation" category, but I would not prioritize this fix. Confidence:
**high** that it's a redundant call; **low** significance.

---

## Doc-vs-code discrepancy (not a code finding, but worth flagging per the task's
guidance not to trust the doc's own rationale)

`docs_ui/addon_space_editor_plan.md` lines 503–518 describe "Mixed-editor add-ons break
the single, area-wide delegate" as **"Not fixed yet, recommended approach recorded for
when it is"** — the recommended fix being to resolve the delegate *per panel* rather
than once per area. But the shipped `addon_panel_types_collect`
(`space_addon.cc:325`, comment block ~381–408) actually *does* address the mixed-editor
case, via a different mechanism than the one the doc recorded as the future plan: it
resolves one delegate per area (as before) but then **drops** any panel whose
`bl_space_type` doesn't match that delegate, rather than showing it against the wrong
space data. This is a real fix for the ucupaint crash the doc describes, just not the
per-panel-delegate approach the doc says is still pending. This means the doc's "Not
fixed yet" framing is stale relative to what shipped — not a code-pruning issue, but
exactly the kind of "design log gone stale" case the task asked to watch for. No code
action needed; flagging so the doc itself doesn't mislead a future reader into thinking
the mixed-editor crash is still open when it's actually mitigated (via filtering, not
per-panel resolution).

---

## Examined and found NOT to be pruning candidates (for completeness)

- **`ScrArea::context_delegate_spacetype` plumbed generically through ~15
  `CTX_wm_space_*` accessors in `context.cc`**, even though only `SpaceAddon` sets it
  today. This looks like speculative generality at first glance, but it is not: an
  add-on's panels can legitimately target *any* of those space types (Node Editor,
  Image Editor, View3D, Sequencer, ...), so the breadth is required for the feature to
  work for arbitrary third-party add-ons, not merely convenient. Confirmed only one
  setter exists (`space_addon.cc:530`); `area.cc:2887` only clears it. This is a
  single-consumer mechanism, but not an over-general one — narrowing it to
  "addon-only" would require threading `SpaceAddon`-specific knowledge into
  `blenkernel`'s context resolution, which the current design explicitly avoids (per
  its own comment, "has no knowledge of which space type, if any, uses the mechanism").
- **No leftover Python-push panel registry, no dead itemf scaffolding**: consistent
  with the task's note that these were built, found unnecessary, and removed. Grepping
  the current tree found no trace of either — the removal appears to have been clean.
- **`ADDON_OT_supported_editors_info.execute()` returning `{'CANCELLED'}` with a
  comment "Exists for its tooltip; nothing to do on click"** — this reads like it could
  be dead/pointless code, but it's a standard Blender idiom (an operator that exists
  solely to host a dynamic `description()` classmethod for a header info-icon button);
  not a pruning candidate.
- **`addon_panels_type_detach` / panel-type-copy-reuse machinery** in
  `addon_panel_types_collect` — looks elaborate (previous-list set-aside, pop-by-name,
  detach-before-free) but each piece is justified by a concrete, stated failure mode
  (preserving collapsed/drag state across re-collection, not leaving `Panel::type`
  dangling). No dead branches found.

---

## Summary

| Confidence | Count | Findings |
|---|---|---|
| High | 4 | #1 (dead RNA property), #2 (stale comment), #3 (triplicated scan), #4 (magic string duplication) |
| Medium | 1 | #3's "best fix shape" is less certain than its existence |
| Low significance (but confirmed) | 1 | #5 (redundant area lookup) |

No findings at "not sure / could be load-bearing" confidence — everything flagged above
was verified against actual current usage (grepped for callers/readers), not inferred
from the design doc alone. The codebase is comparatively tight for a ~900-line new
editor plus DNA/RNA/context plumbing: the most substantial finding (#3) is a
duplicated-scan pattern rather than genuinely dead code, and the least substantial (#5)
is a cheap redundant list walk, not a design flaw.
