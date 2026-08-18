# Add-on Space Editor — Audit Reconciliation Summary

Four independent audits of the same branch (`pyareas/addon-space-editor` vs `main`) were
produced and cross-checked against each other and against the actual source:

- `audit_pruning_candidates.md` — internal (Sonnet agent, this session)
- `audit_native_equivalents.md` — internal (Sonnet agent, this session; first attempt
  faked its result with no file written — caught, corrected via resume, second run
  produced genuine findings)
- `Gemini_pruning_and_cleanup_candidates.md` — external reviewer
- `Gemini_native_blender_equivalents.md` — external reviewer

This note reconciles them. The two Gemini docs have been annotated in place with
`> VERIFIED:` blocks at each finding; this file is the short version.

---

## Headline finding: the external audits cite several APIs that don't exist

Checked by direct grep across `source/` — zero matches for all of:

- `wmContextOverride`
- `ED_region_panels_listener`
- `bpy.types.SpaceType.panel_types`

And one that exists but does something different than claimed: `bContextStore`/
`CTX_store_add` — real (`BKE_context.hh:117-186`), but `python/intern/bpy_rna_context.cc`
was read directly (the actual C implementation of `bpy.context.temp_override()`) and it
does not call into `bContextStore` at all. It applies its override with the same raw
`CTX_wm_area_set`/`CTX_wm_region_set` primitives this fork already uses, wrapped in a
Python-only context-manager object. This is Blender's own canonical example of that save/
restore pattern being the established idiom — not a smell.

**Practical implication**: neither Gemini doc should be trusted at face value for its
*prescriptions*. Several of its underlying *observations* (the sentinel-byte marker is
fragile, the panel-discovery scan is duplicated, the redraw listener is broad) are either
correct or partially correct — but the "here's what to use instead" half of each finding
needs independent verification before acting on it, because multiple such suggestions
cite mechanisms that either don't exist or don't do what's claimed.

## Two findings that were already deliberately considered and rejected, not overlooked

1. **The catch-all redraw listener** (`addon_main_region_listener`). Its own comment
   (`space_addon.cc:601-620`) explicitly names the alternative Gemini proposes
   (forward to the delegate's own listener) and rejects it for a concrete, cited reason:
   several editors' listeners cast `area->spacedata` to their own space type, and
   substituting the delegate's area would silently mismatch the region being tagged.
   *Self-correction*: an earlier pass in this session called this finding "worth taking
   seriously" without cross-checking it against this comment, which had already been read
   earlier in the same session. That was a mistake — corrected here.

2. **`PanelType::owner_id` as an add-on identity mechanism.** The plan doc's own design
   log (§1.3) already investigated this and rejected it: `owner_id` is a workspace UI
   filter (`bl_owner_id`), empty for most panels, and repurposing it would break
   workspace filtering for everyone who currently relies on it.

## One finding that turned out to be structurally impossible, not just undesirable

**`PanelType` deep-cloning** (both Gemini docs, finding #4/#2). `ED_region_panels_layout_ex`
(`area.cc:3297-3385`) walks whatever list it's given via that list's own intrusive
`next`/`prev` links — a `PanelType` can only be a member of one such chain at a time, and
every registered `PanelType` is already linked into its home region type's own list.
Cloning is the only way to draw a `VIEW_3D`-registered panel inside a `SPACE_ADDON`
region without corrupting the 3D Viewport's own list. The Properties-editor comparison
both docs lean on doesn't hold: Properties filters panels that are already native members
of its own list, so it never faces this constraint at all.

## Legitimate findings, correctly identified by at least one source

- **Triplicated "which panels belong to add-on X" scan** across `space_addon.cc`
  (twice) and `screen.cc` (once) — independently found by the internal pruning audit,
  and partially overlaps with Gemini's "duplicate Python/C++ discovery" finding (the
  filtering half of that finding is valid; the display-name-resolution half is not, since
  that data is only reachable from Python by design).
- **`"ADDON_PT_empty_state"` hardcoded independently in two C++ locations** plus
  implicitly via Python's `bl_idname` — found by the internal pruning audit, and
  includes a literal added by this session's own empty-state fix.
- **Dead `Area.context_delegate_spacetype` RNA property** — added for Python UI to read,
  never actually read anywhere in `scripts/` — internal pruning audit only.
- **`addon_panel_poll_guarded` reimplements `rna_ui.cc`'s `panel_poll` RNA-call
  boilerplate**, but is actually *better* (distinguishes a raised exception from a plain
  `False`) — internal native-equivalents audit; recommends extracting a shared helper
  and upgrading `rna_ui.cc` to the same safety, not deleting the duplicate.
- **The sentinel-byte picker marker is real, admitted fragility** — the plan doc's own
  log records a shipped bug caused by exactly this mechanism. Gemini's instinct here is
  right even though its proposed replacement (a separate header button) changes the
  intended unified-dropdown UX rather than simply hardening the existing mechanism.

## Left genuinely open — not confirmed or refuted this session

- **Global poll-failure blacklist** (`addon_poll_failed_get`, a static `Set<std::string>`
  shared across all areas/windows). Gemini raised a plausible concern (cross-window side
  effects, silent permanent blacklisting); this was not independently traced through
  closely enough to confirm or refute whether standard RNA-boundary error reporting alone
  would actually be sufficient replacement, or whether it would just mean the same
  exception re-logs every redraw instead of once. Worth a dedicated look before acting.

## Bottom line

The internal audits (both, once the first was corrected via resume) hold up under
independent verification — every claim checked came back accurate, and both were candid
about uncertainty where it existed. The external Gemini audits contain a mix of real,
useful observations and confident-sounding prescriptions that don't survive a direct
check against this codebase — several cite APIs that don't exist, one contradicts a
design decision already documented and reasoned through in the plan doc, and one
misreads code that's already using the exact mechanism being proposed as the fix.
Treat the Gemini docs as a source of *questions worth checking*, not answers.
