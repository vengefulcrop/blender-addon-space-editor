---
type: architecture
title: "Fork Mergeability"
description: "Which upstream files this fork touches, how risky each touch is on rebase, and why the design stays close to upstream mechanisms"
tags: [architecture, addon-editor, upstream, rebase]
last_updated: 2026-09-12
---

# Fork Mergeability

## Distribution goal

The fork publishes its diff so a user can compile a personal build. This
sets a constraint on the design. The diff must stay a small, reviewable,
rebasable patch series against upstream `main`. It must not become a
sprawling refactor.

## File-level touches, as of the point this was last measured

25 files touched, +1126/-21 across 4 commits. New files
(`editors/space_addon/`, `bl_ui/space_addon.py`) carry the bulk of this
total. They can never conflict with upstream changes.

| File | Nature of the touch | Conflict risk on rebase |
|:---|:---|:---|
| `blenkernel/intern/context.cc` | 17 accessor bodies call one static helper instead of `CTX_wm_area` directly. The helper itself names no editor. | Low-medium, down from highest. If upstream touches an accessor, it still needs a one-line re-merge. But the helper's own body no longer names `SPACE_ADDON`. |
| `blenkernel/BKE_screen.hh`, `blenkernel/intern/screen.cc` | A 5-line panel-types revision counter, plus one new query function, `BKE_paneltypes_addon_space_types_get` | Near zero. The one-time hook once added to `rna_Panel_register` is gone (see [Add-on Panel Attribution](./addon_panel_attribution.md)). The new function adds code. It does not hook into an existing code path. |
| `makesdna/DNA_space_enums.h` | `SPACE_TYPE_NUM` rebases onto a new enumerator, `SPACE_ADDON = 25` | Low-medium on the text, **high on the meaning**. Git reports no conflict if upstream also takes 25, and the build still succeeds. Check the slot by hand on every rebase. See [Upstream Fragility](./upstream_fragility.md). |
| `blenkernel/BKE_blender_version.h`, `blenloader/intern/versioning_530.cc` | `BLENDER_FILE_SUBVERSION` raised from 10 to 12 | **High on the meaning.** The `#define` conflicts loudly, but the two versioning blocks do not. A shared subversion number makes a stock build skip its own versioning. See [Upstream Fragility](./upstream_fragility.md). |
| `makesdna/DNA_screen_types.h` | One `short` field on `ScrArea`, replacing 2 bytes of existing padding, no struct size change | Low. The field adds code and names no specific editor. |
| `makesdna/DNA_space_types.h` | One more `short` field on `SpaceAddon` (`preferred_delegate_spacetype`), same padding-reuse pattern | Low. This field only adds to the fork's own struct. |
| `editors/screen/area.cc` | One line in `ED_area_newspace()` resetting `ScrArea::context_delegate_spacetype` on any area-type change | Low. This is a single, generic reset, not specific to this editor, in a function upstream touches occasionally. It fixes a real crash, so it is worth defending as a correctness fix regardless of rebase cost. |
| `makesrna/intern/rna_space.cc`, `rna_screen.cc`, `spacetypes.cc` | Additive entries in existing lists and switches. Two new plain enum properties (`Area.context_delegate_spacetype`, `SpaceAddon.preferred_delegate_spacetype`), no custom itemf | Low. These are insertions, not edits to existing lines. |
| `anim_filter.cc`, `grease_pencil_convert_legacy.cc`, `resources.cc` | One `case` label added to an exhaustive switch each | Near zero. |
| `makesdna/DNA_userdef_types.h`, `rna_userdef.cc` | New struct and field, additive | Low, with one caveat: DNA requires 8-byte alignment for pointer-containing members file-wide, so an inserted field needs correct padding. |

**Net assessment**: this stays a rebasable patch series. It does not
diverge from upstream structurally. Nothing overrides upstream behavior
for any area type other than `SPACE_ADDON`. Every other space type's code
path stays byte-for-byte what it was.

## Why the generic-context refactor mattered

`context.cc` was the one file with real ongoing maintenance cost and a
design objection: it once branched directly on
`area->spacetype != SPACE_ADDON`. The fix made
`ScrArea::context_delegate_spacetype` a generic field with no spacetype
check in the accessor (see
[Context Delegation](./context_delegation.md#made-generic-no-core-code-names-this-editor)).
After this fix the design objection is gone, since no kernel code names
this editor. What remains is an ordinary, low-risk rebase surface.

## Estimated LOC, compared to the superseded design

| # | Change | File(s) | Est. LOC |
|:-:|:---|:---|:---:|
| 1 | `SPACE_ADDON` enum value; `SpaceAddon` struct | `makesdna/DNA_space_enums.h`, `makesdna/DNA_space_types.h` | ~40 |
| 2 | `UserDef.addon_editors` list and RNA | `makesdna/DNA_userdef_types.h`, `makesrna/intern/rna_userdef.cc` | ~80 |
| 3 | New editor module: space callbacks, region init/draw, context delegation | `editors/space_addon/` (new) | ~250 |
| 4 | `space_subtype_get` / `_set` / `_item_extend` | `editors/space_addon/space_addon.cc` | ~90 |
| 5 | C++ registry: add-on id to filtered `ListBaseT<PanelType>`; RNA push API | `editors/space_addon/addon_panel_registry.cc` (new, later removed, see [Add-on Panel Attribution](./addon_panel_attribution.md)) | ~120 |
| 6 | Python: group `Panel.__subclasses__()` by `__module__`; picker operator; menu | `scripts/startup/bl_ui/space_addon.py` (new) | ~150 |
| 7 | `.blend` fallback when the add-on is absent | `blenloader/intern/versioning_*.cc`, `blenkernel/intern/screen.cc` | ~60 |
| 8 | Register the new space type in the editor init table | `editors/include/ED_space_api.hh`, `editors/space_api/spacetypes.cc` | ~10 |

**Total, original estimate**: ~600-800 LOC across 8 existing files plus 1
new module directory. The superseded design estimated ~850-1,200 LOC. See
[Add-on Space Type vs Dynamic Registration](../decisions/adr_001_addon_space_type_vs_dynamic_registration.md)
and
[Panel Re-Hosting via Region Layout Hook](../decisions/adr_002_panel_rehosting_via_region_layout_hook.md)
for why the estimate shrank.

## Repository conventions

- Work happens on a dedicated branch off `main`. The publishable diff is
  `git format-patch main..<branch>`.
- Nobody commits to `main`, so upstream rebases stay clean.
- Building requires `make update` first, to fetch precompiled libraries
  into `lib/`.

## Related

- [Add-on Space Type](./addon_space_type.md)
- [Context Delegation](./context_delegation.md)
- [Persistence and Compatibility](./persistence_and_compatibility.md)
