# Preliminary Code Review — `pyareas/addon-space-editor`

Reviewed 2026-08-01 against `main` @ `027ef661` (Blender 5.3.0 alpha).
Scope: the 8 commits on this branch, +1391/-22 across 25 files.

**Method and its limits.** Primarily a static review: the full branch diff plus a close
read of [`space_addon.cc`](../source/blender/editors/space_addon/space_addon.cc),
[`space_addon.py`](../scripts/startup/bl_ui/space_addon.py), and the touched
`blenkernel`/`makesrna`/`python` files. Findings are reasoned from the source unless
marked otherwise. The three test scripts in this directory were not executed, and no
interactive GUI testing was done.

Everything marked *(fixed)* below was built clean and committed. §2.1 and §2.3 were
additionally verified at runtime, headlessly; §2.2 and §2.4 were not, for reasons given
under each. See §0 for the current ledger.

---

## 0. Status ledger

Current as of 2026-08-01.

### Fixed (built clean, 0 errors / 0 warnings)

| # | Item | Verified |
| :-- | :--- | :--- |
| §2.1 | `U.addon_editors` blenloader support — write, read, free, app-template swap | Yes — headless save/restart/read round-trip |
| §2.2 | Userpref flag bit `USER_UIFLAG2_UNUSED_2` reuse — subversion bump + versioning clear | No — needs a pre-2.92 `userpref.blend` |
| §2.3 | Panel enumeration counted unregistered base classes and missed indirect subclasses | Yes — verified against installed add-ons |
| §2.4 | Panel layout destroyed on load and on any screen change — now reuses `PanelType` copies | Yes — interactive GUI testing |
| §3a | Mixed-editor add-ons polled against the wrong space, hanging Blender — delegate now resolved before collection, which is then filtered to it | No — needs GUI testing |
| §6 | A raising `poll()` was reported every redraw; now reported once and the panel dropped | No — needs GUI testing |
| §3 | Context delegation moved from `SpaceAddon`-specific to a generic `ScrArea` field; `blenkernel` no longer names this editor | No — full rebuild only, mechanism itself unchanged (behavior already covered by §3a/§6's verification) |
| — | `PanelType::addon_id` removed; attribution computed on demand in `space_addon.cc` instead of at every panel's registration | No — full rebuild only, same underlying `BPY_class_module_name_get()` call, same output |
| — | Crash switching an Add-on editor area to any stock editor type — stale `ScrArea::context_delegate_spacetype` never cleared on area-type change, mismatching a new editor's own region against an unrelated area's space data | Yes — interactive; user reported the crash, fix confirmed no crashes since |
| — | User preference for which editor an add-on delegates to (`SpaceAddon::preferred_delegate_spacetype`), honored strictly (no silent substitution when the chosen type isn't open); targeted empty-state message names the specific unsatisfied choice | Partial — build clean, behavior not yet re-confirmed interactively after the strict-honoring change specifically |

### Open — worth fixing

| # | Item | Cost |
| :-- | :--- | :--- |
| §2.5 | Add-on attribution rule implemented twice (C++ / Python), free to drift. Narrowed by the §2.3 fix, which aligned Python on the registered-panel set, but still two implementations. | Refactor |

### Open — known, deliberately deferred

| # | Item | Status |
| :-- | :--- | :--- |
| §3b | Modal operators need the delegate's region at invoke time; nothing swaps it. `convertViewVec: called in an invalid context` spam. | Standing, unconditional |
| §3c | Properties-style panels get no `bl_context` filtering (`contexts` passed as `nullptr`) | Accepted in the plan |
| §5 | Per-area add-on internal tab state (the SourceOps case) | Won't do — see §5 |

### Architectural, not a defect

~~§3 — global context delegation through `ctx_wm_area_effective`. Correct and cheap, but
couples `blenkernel` to one editor; the main upstream-review risk.~~ **Resolved
2026-08-05**: the delegate field moved to a generic `ScrArea::context_delegate_spacetype`;
`blenkernel` no longer names this editor anywhere. See §3.

### Incidents

**2026-08-01, hosting ucupaint — Blender hung.** Diagnosed live from the stuck process,
root-caused to §3a, fixed. Full write-up in §6.

**2026-08-05, switching an Add-on editor area to a stock editor — Blender crashed.**
Reported directly by the user immediately after the multi-editor preference feature
shipped. Root-caused to a residual gap in the earlier context-delegation refactor (§3):
`ScrArea::context_delegate_spacetype` was set and read but never cleared, so it leaked
across an area's editor-type change. Fixed in `ED_area_newspace()` generically - see
`addon_space_editor_plan.md`'s "Crash switching an Add-on editor area to a stock editor
type" entry for the full mechanism. User confirmed no further crashes since.

### Verification gaps

The §2.4 panel-layout fix was confirmed by interactive testing in the built Blender.
Everything else marked verified above was checked headlessly, in-process, against the
installed add-on set — which is stronger than static reasoning but is not a test of the
drawn UI. The three test scripts in this directory have still not been run, and there is
no automated regression coverage for any of this: each fix was verified once, by hand,
and nothing would catch a later regression. Everything not marked verified is static
analysis only.

---

## 1. Overall assessment

The patch reads like upstream Blender code rather than a bolt-on, which is the main
thing that makes it a plausible submission rather than just a working fork.

- Correct idioms throughout: `MEM_new`/`MEM_delete`, `ListBaseT<>`, `STRNCPY`, `ELEM`,
  `BKE_spacetype_register`, doxygen `\name` / `\{` section banners, SPDX headers.
- The DNA design is right. Identifying the hosted add-on by module string rather than
  by enum sub-type index — with the `SpaceNode` node-tree-type precedent cited in the
  struct comment — is the call upstream would make, and the same reasoning correctly
  produces `delegate_spacetype` as a *type* rather than a `ScrArea *`, so closing the
  borrowed editor cannot dangle.
- Comments explain *why*, not *what*, and pre-empt the objections a reviewer would
  actually raise: why `ADDON_SUBTYPE_PICK` is `0x7FFF` and not `-1`; why panel types are
  copied rather than linked into the runtime list; why the pick-marker byte has to
  round-trip through `set` → `update`. This is rare, and it is the single biggest reason
  the branch reads as mergeable.

The load-bearing architectural risk is the global context delegation in `context.cc`;
see §3.

---

## 2. Defects

### 2.1 Blocker — `U.addon_editors` is never saved or loaded *(fixed)*

A new `ListBase` was added to `UserDef` with no corresponding blenloader support:

- [`writefile.cc:1301`](../source/blender/blenloader/intern/writefile.cc#L1301) writes
  `userdef->addons`; nothing writes `addon_editors`.
- [`readfile.cc:4079-4088`](../source/blender/blenloader/intern/readfile.cc#L4079-L4088)
  has a `BLO_read_struct_list` call for every other `UserDef` list — none for
  `bAddonEditor`.

Two consequences, the second worse than the first:

1. The curated editor list does not survive a restart, so the feature's whole
   persistence story — the stated reason `bAddonEditor` exists rather than deriving the
   menu from enabled add-ons — does not actually hold.
2. The raw `UserDef` struct *is* written, including the `ListBase` head/tail pointers.
   A `userpref.blend` saved while entries exist stores live in-memory addresses that are
   never remapped on read, because no `BLO_read_struct_list` claims them. Iterating
   `U.addon_editors` in `addon_ids_get()`
   ([`space_addon.cc:462`](../source/blender/editors/space_addon/space_addon.cc#L462))
   after such a load dereferences stale pointers. That is a crash, not a lost setting.

**Fixed** — added the write loop in `writefile.cc`, the `BLO_read_struct_list` in
`readfile.cc`, and `addon_editors.free_no_destruct()` in
`BKE_blender_userdef_data_free` (which also leaked the list). `addon_editors` is
additionally now `VALUE_SWAP`ped in `BKE_blender_userdef_app_template_data_swap`
alongside `addons`, which it mirrors — an app template brings its own enabled add-ons,
so the editors curated against them should travel with them.

**Verified at runtime**, headless, against an isolated `BLENDER_USER_RESOURCES`
directory. One process created two entries (a plain module and a dotted
`bl_ext.<repo>.<addon>` extension id), set the bundled flag, and called
`wm.save_userpref()`; a second, fresh process read back both entries with module, name,
order and flag intact, and exited cleanly. That exercises the write loop, the pointer
remap on read, and the free path.

*Not* verified: the §2.2 versioning clear, which needs a `userpref.blend` written by a
pre-2.92 Blender with Natural Trackpad enabled. No such file was available.

### 2.2 Reused userpref flag bit — confirmed unsafe *(fixed)*

`USER_ADDON_EDITOR_SHOW_BUNDLED` takes over `USER_UIFLAG2_UNUSED_2`
([`DNA_userdef_types.h:412`](../source/blender/makesdna/DNA_userdef_types.h#L412)).
Bit 2 carried no `/* cleared */` annotation, unlike its neighbours — and the history
shows why. It held `USER_TRACKPAD_NATURAL` until commit `055ed335a11` (Nov 2020, 2.92
dev) removed that preference, renaming the bit to `USER_UIFLAG2_UNUSED_2` *without*
adding any versioning clear. `grep uiflag2 versioning_userdef.cc` finds one line, for a
different bit. So any `userpref.blend` predating 2.92 whose owner had "Natural Trackpad"
enabled — effectively every long-time macOS trackpad user — still has bit 2 set, and
would have got `show_addon_editor_bundled` silently on.

**Fixed** — bumped `BLENDER_FILE_SUBVERSION` to 11 and added a
`!USER_VERSION_ATLEAST(503, 11)` block in `versioning_userdef.cc` clearing the bit, plus
the `/* cleared */` annotation on the enum so the next person to reuse it knows.

### 2.3 Panel enumeration was wrong in two ways *(fixed)*

`_addon_top_level_panel_space_types` used `bpy.types.Panel.__subclasses__()`, which is
both direct-only and unfiltered. Measured against the installed add-on set: 1268 direct
subclasses, 1277 recursively, of which 1267 are registered.

**Unregistered base classes were counted — the live bug.** Ten classes in the tree are
`Panel` subclasses that are never registered: an add-on's own panel base, which usually
carries a `bl_space_type` for its children to inherit. All ten happen to be *direct*
subclasses, so the original code counted them. Concretely, ucupaint's unregistered
`Y_PT_UDIM_Atlas_menu` made the add-on report `IMAGE_EDITOR`, which no registered
ucupaint panel declares — so with only an Image Editor open, the empty-state panel would
list "Image Editor" among the editors it needs while one was already open and nothing
drew. The C++ side, scanning registered `PanelType`s, never made that claim.

**Indirect subclasses were missed — latent.** An add-on deriving its panels from its own
base is invisible; mio3_uv has five such panels. In practice it changed nothing there,
because mio3_uv also has four direct panels declaring the same `IMAGE_EDITOR`, so the
resulting set was identical. The gap only bites an add-on whose top-level panels are
*all* indirect, which would vanish from the picker entirely. Not present in the tested
set, but free to fix while in the function.

**Fixed** — `_registered_panel_classes()` walks the subclass tree iteratively and filters
on `is_registered`, which is exactly the set the C++ side scans.

**Verified** against the live build: ucupaint now reports `['NODE_EDITOR', 'VIEW_3D']`
instead of `['IMAGE_EDITOR', 'NODE_EDITOR', 'VIEW_3D']`, mio3_uv is unchanged at
`['IMAGE_EDITOR']`, and the walk finds all 1267 registered classes.

### 2.4 Panel layout is saved to disk, then destroyed on load *(fixed)*

Applies to every native mechanism that persists a screen layout — Save Startup File,
`Add Workspace`, app templates, appending a `WorkSpace` datablock. They all share one
path: `bScreen` → `ScrArea` → `SpaceLink` plus regions.

The good news is that nothing needed doing to save it. [`write_area`
(screen.cc:1412)](../source/blender/blenkernel/intern/screen.cc#L1412) calls
`write_panel_list(writer, &region.panels)` unconditionally for every space type, with no
per-editor opt-in, capturing collapse state (`PNL_CLOSED`), drag-reorder `sortorder`,
and `layout_panel_states` — the `layout.panel()` sub-sections — at
[screen.cc:1399-1403](../source/blender/blenkernel/intern/screen.cc#L1399-L1403).
`direct_link_panel_list` reads it back at
[screen.cc:1507](../source/blender/blenkernel/intern/screen.cc#L1507). The hosted
`addon_id` persists too, via `addon_blend_write`. So the Add-on editor inherits full
layout persistence for free.

It is then thrown away on the first redraw. `addon_blend_read_data` allocates a fresh
`SpaceAddon_Runtime` whose `cached_addon_id` is empty
([`addon_intern.hh:25`](../source/blender/editors/space_addon/addon_intern.hh#L25)), so
the first `addon_main_region_layout` compares `STREQ("", "node_wrangler")`, misses the
cache, and calls
[`BKE_area_region_panels_free(&region->panels)`](../source/blender/editors/space_addon/space_addon.cc#L336).
Every `Panel` just read from the file is freed before `panel_find_by_type` can match it
by `panelname`. The same wipe fires mid-session on any screen-signature change, so
opening or closing an unrelated editor also re-collapses the panels.

The free is not gratuitous: `Panel::type` points into the `PanelType` copies that were
just freed, and the code comment says so. It is a correct guard around a collection
strategy that did not anticipate reuse.

**Fix — match-and-reuse.** On re-collection, keep the existing copy for any idname still
present, allocate only for new ones, free only copies whose type genuinely disappeared,
and free only the `Panel`s bound to those. `Panel::type` stays valid for survivors, so
loaded state is preserved. Fixes the load case and the spurious mid-session wipe
together. A cheaper stopgap — skip the `panels_free` when the collected idname set is
unchanged — covers the common cases but still wipes on any real change.

**Fixed** — `addon_panel_types_collect` now sets the previous list aside, moves back any
entry whose ID name is still wanted (refreshing its contents in place from the registered
type, since reloading an add-on registers a whole new `PanelType`), and allocates only
for genuinely new ones. `Panel::type` therefore stays valid and `region->panels` is left
untouched. Whatever is left over is what really went away: its panels are detached first
(`Panel::type = nullptr`, the same state `direct_link_panel_list` leaves every panel in
after a file read, and which `panel_begin` re-binds by ID name if the type returns), then
the copies are freed. The `BKE_area_region_panels_free` call is gone.

As predicted, no DNA recompile — rebuilt in **1m07s**.

**Verified** by interactive testing in the built Blender — the only way to check this,
since panels are instantiated only by a real layout pass.

**Cross-application caveat.** A `.blend` or workspace containing an Add-on editor opened
in *stock* Blender hits
[screen.cc:1620](../source/blender/blenkernel/intern/screen.cc#L1620), which degrades
the unregistered space type to `SPACE_EMPTY` and stashes the original in
`butspacetype`. No crash, but `SpaceAddon` is an unknown DNA struct there, so `addon_id`
is lost if that user resaves. Workspace presets are one-way.

### 2.5 Minor — two implementations of one attribution rule

C++ `BPY_class_module_name_get` truncates to the top-level module (three segments for
`bl_ext.`) and matches with `STREQ`; Python `_addon_top_level_panel_space_types` matches
by module prefix. They agree today, but the comment in `addon_panel_types_collect`
asserts they are the *same* filter, which invites changing one without the other. Worth
either a cross-reference note on both sides or, better, exposing the C++ answer to
Python so there is one implementation.

---

## 3. Architectural note: global context delegation *(resolved 2026-08-05)*

`ctx_wm_area_effective()` used to put a branch on `area->spacetype != SPACE_ADDON` — and,
for add-on areas, a `BKE_screen_find_big_area` scan — into every typed `CTX_wm_space_*`
accessor in Blender. The escalation that forced the mechanism to be global rather than
per-call is recorded in [`addon_space_editor_plan.md` §"Context delegation had to be
global, not per-call"](addon_space_editor_plan.md), and the reasoning was, and remains,
sound: `CTX_wm_space_data` reads `area->spacedata.first` directly with no hook point, so a
`SpaceType.context` callback structurally *cannot* intercept it, and layout-scoped
swapping leaves menus and button-press-time operator polls seeing the undelegated
`SpaceAddon`. None of that changed.

What changed is *where the coupling lives*. The delegate type moved from
`SpaceAddon::delegate_spacetype` to a new, generic `ScrArea::context_delegate_spacetype`
(see `addon_space_editor_plan.md` §4a, "Context delegation made generic"), and
`ctx_wm_area_effective()` now reads it with no spacetype check at all — `blenkernel` no
longer contains the string `SPACE_ADDON` anywhere except `CTX_wm_space_addon()`'s own
one-line body, which is the same shape every other typed accessor uses for its own type.
The mechanism, the resolution order, and the fallback behavior are byte-for-byte
unchanged; only the field's owner and the accessor's knowledge of who uses it are
different. This directly addresses the objection this section raised — a reviewer can no
longer point at `context.cc` and say it was taught the name of one editor, because it
wasn't.

The related known gaps — modal operators needing the delegate's *region* at invoke time,
and mixed-editor add-ons collapsing to a single area-wide delegate — are unaffected by
this change and are not re-litigated here; they are still open per §0's deferred list.

**§9's `bContextStore` proposal is superseded by this, not built.** It was the intended
reduction of the 18-accessor coupling down to one generic touch point; this achieves the
same reduction (one generic field, zero editor-specific knowledge in `blenkernel`) without
needing the `bContextStore` mechanism or its three unverified failure cases (menus,
operator polls, modal invoke) at all, because the *mechanism* that already passed all
three in practice (this editor has been draggable, poll-safe, and menu-safe since §3a's
fix) was kept — only its storage location changed. §9 is left in place below as a record
of the alternative that was considered and not needed, not as a live recommendation.

---

## 4. Publishing and distribution

### 4.1 Licensing

The GPL permits publishing this fork with binaries. Blender is GPL-2.0-or-later, this
branch is a derivative work and must be GPL-2+, and shipping source and binary together
in one tagged release satisfies GPLv2 §3(a) cleanly.

The practical constraints are not copyright ones:

1. **Trademark.** "Blender" and the logo are Blender Foundation trademarks; the GPL
   grants nothing there. Do not ship an executable named `blender.exe` under a project
   called "Blender ___". Rename the build and project, and state plainly that it is an
   unofficial fork, not endorsed by the Blender Foundation. This is the usual way forks
   attract a takedown request.
2. **Bundled third-party notices.** The build already emits `bin/Release/license/` and
   `blender.crt/`. Both must be in the shipped archive — the binary links dozens of
   Apache/BSD/MIT/zlib components whose notices are a distribution condition.
3. **Reviewable history.** Keep `origin` on
   `projects.blender.org/blender/blender`, push the fork to a second remote, and publish
   this branch on top of an unmodified upstream commit — never squash upstream history.
   Record the base (`v4.3.0-21581-g027ef661`) in the release notes so the whole change
   is one `git diff`. That also keeps the branch in submittable shape.

### 4.2 Minimal ("overlay") distribution

Feasible, but it saves less than it appears to. Every C++ change compiles into the
monolithic `blender.exe`, which is **95 MB** by itself. An overlay is therefore:

| Component | Size |
| :--- | :--- |
| `blender.exe` | 95 MB — unavoidable, holds every C++ change |
| `5.3/scripts/startup/bl_ui/space_addon.py` | new file |
| `5.3/scripts/startup/bl_ui/__init__.py` | one line |

Roughly 95 MB against a ~350 MB full archive. There is no way below the executable; the
C++ changes are not separable from it.

The binding requirement is exact build matching. The overlay works only if `blender.exe`
was built from the same upstream commit, the same `lib/windows_x64` revision
(`v5.2.0` / `60d6e96b`, per [`building.md`](building.md)), the same MSVC toolchain and
the same CMake options as the official build the user already has. It dynamically links
the bundled DLLs and loads a `5.3/` data directory containing a Python 3.12 tree whose
ABI must match; drift gives a startup crash or, worse, quiet misbehaviour. Daily alpha
builds also move every day, so "download the official build" is not a stable target.

**Recommendation:** ship the full archive as the primary release — it is what makes bug
reports reproducible, and 350 MB is unremarkable for a Blender build — and offer the
overlay as a secondary convenience for people iterating against a known build. The
overlay is still a binary distribution and carries the same source-availability and
notice obligations; "it's just a patch" does not exempt it.

---

## 5. Per-area instances: the slot design, and why it stops short

Raised as a design question: let the user open several Add-on editors for the same
add-on, each showing a different part of it. Recorded here with the evidence, because
the conclusion is a *won't do* and the reasoning is the valuable part.

### Three layers of state, only one of which is shared

1. **Panel collapse state, drag order, `layout.panel()` sub-section states** — already
   per-area. They live in `region->panels`, and each area owns its regions. Nothing to
   design; this works once §2.4 stops wiping it.
2. **Which panels are hosted** — currently identical across areas, because hosting is
   keyed on `addon_id` alone. Ours to fix; see the slot design below.
3. **The add-on's own internal tab state** — shared, and not fixable from our side when
   the add-on stores it in scene/window-manager data.

### The slot design (not implemented)

Change the hosting key from `addon_id` to **(addon_id, space_type, category)**. That
triple is what a user already recognises as "an add-on's tab" — an N-panel presence
*is* `(bl_space_type, bl_region_type, bl_category)`.

Most of the machinery exists: `ED_region_panels_layout_ex` already takes `contexts[]`
and `category_override`, both of which this editor currently passes as `nullptr`, and
`panel_add_check` filters on `STREQ(panel_type->category, category_override)`
([area.cc:3300](../source/blender/editors/screen/area.cc#L3300)). DNA cost is two fields
on `SpaceAddon` and two on `bAddonEditor`; empty means "everything", so no versioning
code is needed.

**Its real value is not UX — it retires §3a.** With `space_type` fixed by the user's
choice, the delegate is correct by construction: no first-match heuristic, no per-panel
resolution. It also lets the cache's screen signature narrow from "which space types are
open anywhere" to "is an area of *my* space type open", cutting spurious invalidation.

### Why it does not solve the motivating case

Checked against SourceOps (`scripts/addons/SourceOps`), the add-on actually being tested:

- **One** top-level panel, `SOURCEOPS_PT_MainPanel` — `VIEW_3D` / `UI` / category
  `SourceOps`. No sub-panels, no siblings.
- Its eleven tabs are a single `EnumProperty` named `panel` on `SOURCEOPS_GlobalProps`,
  attached as `bpy.types.Scene.sourceops`.
- `draw()` is one `if/elif` chain over that enum; the tab strip is
  `row.prop(sourceops, 'panel', expand=True, icon_only=True)`.

So the triple has exactly one value for SourceOps, and two areas necessarily show the
same tab. This shape — one monolithic panel plus a scene-stored enum tab strip — is a
common way to build a Blender add-on UI, not an outlier.

### The mechanism that would work, and why it was rejected

The tab is a plain RNA property with no `update=` callback, so a **scoped per-area value
swap** around layout is technically viable: raw-set the property, lay out, restore, in
the same RAII shape as the existing context swap. Raw `RNA_property_*_set` does not tag
the depsgraph or push undo — those come from `RNA_property_update`, which you simply do
not call. Clicks resolve themselves: the tab button writes the true value outside the
swap, so an area can adopt any post-redraw difference as its own new override.

Rejected because it **mutates document data as a side effect of drawing**, crossing the
line the design otherwise holds — delegation only ever borrows real, visible editors and
never fabricates or rewrites state. An exception unwinding mid-layout leaves the wrong
value in the scene, and the saved `.blend` records whichever area drew last. That is
harmless for a tab index and not harmless for a property whose meaning we cannot know in
advance.

If revisited, the constraints to hold: opt-in per add-on and never inferred; the
nominated property remembered on the `bAddonEditor` entry rather than per area;
restricted to enum and int properties with no update callback; exception-safe restore.

---

## 6. Incident: hang while hosting ucupaint (2026-08-01)

Blender stopped responding after closing and reopening a 3D Viewport while an Add-on
editor hosted ucupaint. Recorded in full because the diagnosis was non-obvious and the
root cause was a gap this document had already described as theoretical.

### Diagnosis

The process was alive but unresponsive with **7 seconds of CPU** since launch — so it was
blocked, not spinning, which rules out an infinite loop before any code is read. A
non-invasive `cdb -pv` attach gave the main thread:

```
addon_main_region_layout
  → ED_region_panels_layout_ex
    → panel_add_check → panel_poll → bpy_class_call
      → PyErr_Print → PyErr_Display
        → KERNELBASE!WriteConsoleW      <- blocked
```

Reading the frames' locals identified the participants exactly:

| Value | Read from |
| :--- | :--- |
| `bl_ext.blender_org.ucupaint` | `saddon->addon_id` |
| `NODE_PT_YPaintUI` ("Ucupaint 2.4.9") | `pt->idname` |
| `0x0001` = `SPACE_VIEW3D` | `saddon->delegate_spacetype` |

A `NODE_EDITOR` panel was being polled while the area delegated to `SPACE_VIEW3D`.

### Root cause — §3a, exactly as predicted

ucupaint registers panels for both the Node Editor and the 3D Viewport. With the viewport
closed, only its `NODE_EDITOR` panels were collected and the delegate was `NODE_EDITOR`,
so everything worked. Reopening the viewport brought the `VIEW_3D` panels back into
collection, and the old `addon_context_delegate_find` returned the *first* match — now
`VIEW_3D`. `NODE_PT_YPaintUI.poll()` then ran against a `SpaceView3D`, accessed
`context.space_data` unguarded, and raised on every redraw.

The plan had described this failure, naming ucupaint, before it happened. It was filed as
deferred because it looked cosmetic. It is not: the amplifier below turns it into a hang.

### The amplifier — a missing guard the original design called for

Blender prints a Python traceback and continues, which is correct once and ruinous at
redraw rate. On Windows the console applies backpressure and the main thread blocks in
`WriteConsoleW`. The result presents as the worst kind of failure: unresponsive, no CPU
use, no crash log, nothing in the console but a wall of identical tracebacks.

The design had anticipated this — §2.1 of the plan asked for "trapping `poll()`
exceptions (report once to the console, don't spam per redraw)" — and it was never built.
Because this editor deliberately hosts panels outside their native context, a raising
`poll()` is an *expected* condition here, not an anomaly.

### Fixes

**A — resolve the delegate before collecting, then collect only what it can satisfy.**
`addon_delegate_spacetype_find` picks the editor type up front, and
`addon_panel_types_collect` keeps only panels declaring that type (plus the
space-agnostic ones). No panel is ever polled against a space it was not written for.

The "first declared type with an editor open wins" rule is kept unchanged, deliberately:
the fix changes which panels are *shown* without also changing which editor is borrowed.
The visible consequence is that with both editors open, ucupaint's Node Editor panels no
longer appear in an area delegating to the viewport — which is precisely the choice the
slot design (§5) would hand to the user. Automatic for now, explicit later.

The per-panel delegate resolution the plan recommended was considered and rejected as
written: `ctx_wm_area_effective` reads a single `delegate_spacetype` off the space, and
`panel_poll` runs *inside* `ED_region_panels_layout_ex`, so there is no seam at which to
vary it per panel without adding a hook to generic code.

**B — guard `poll()` so a raise is reported once, not every frame.** Panel type copies get
their `poll` replaced with `addon_panel_poll_guarded`, which mirrors `rna_ui.cc`'s
`panel_poll` but checks the return code of `rna_ext.call` instead of discarding it — the
only way to distinguish "returned false" from "raised". A panel that raises is recorded
by ID name and not polled again until the registered panel types change, so re-enabling
or reloading an add-on gives it a clean slate.

B is defence in depth: A removes this cause, B stops the next one from hanging Blender.

### What this says about the deferred list

§3a sat under "known, deliberately deferred" and cost a hung session. §3b (modal
operators needing the delegate's region) is still there, is unconditional, and currently
manifests as console spam — the same amplifier that turned §3a into a hang. It should be
re-read in that light rather than left as a cosmetic annoyance.

---

## 7. Planned: per-panel hosting

Supersedes the slot design in §5. Recorded as a design to return to; not implemented.

### What it is

Store a **list of panel ID names** on `SpaceAddon`. Empty means today's behaviour - host
everything belonging to `addon_id` - so existing areas and saved files keep working.
Non-empty means host exactly those panels.

The motivating case is Lumos. Its four registered panels are the Light Editor, a
one-button panel that opens the same thing as a popup, and two Manager panels. Hosting
"the add-on" gets all four; what the user wants is one of them.

This absorbs the §5 slot design rather than complementing it. Selecting a whole
`(editor, category)` tab becomes a *picker convenience* that expands to individual
entries at pick time, so there is nothing extra to store - no `space_type` or `category`
fields in DNA, just the list.

### The invariant that makes it safe

An area has one delegate, so every hosted panel must declare the same `bl_space_type`
(or be space-agnostic). Enforce it at pick time - once an area holds its first panel,
offer only compatible ones - and the entire §3a/§6 class of bug becomes *unrepresentable*
rather than merely avoided. The runtime filter from fix A stays as a safety net but stops
having anything to do.

Note what that constrains: not "one add-on per area", but "one editor type per area".
Composing panels from several add-ons falls out for free if they agree on space type.
Worth deciding deliberately rather than discovering by accident.

### Settled details

- **Top-level panels only.** Collection already skips `pt.parent != nullptr`, and
  children come along through `PanelType::children`. Hosting a sub-panel standalone means
  lifting that skip, and its `draw()` may assume the parent ran first.
- **Ordering needs no UI.** With §2.4 fixed, drag order persists per area. The list is a
  set; initial draw order is list order and the user rearranges from there.
- **Picker placement.** The editor-type drop-down stays add-on-level - it is the coarse
  "what is this area for" choice. Panel selection is per-area, so it belongs on the
  Add-on editor's header.

### Traps

**This is §2.1 again.** A `ListBase` on `SpaceAddon` needs explicit `blend_write` and
`blend_read_data` handling; `addon_blend_write` currently writes only the flat struct.
Getting it wrong reproduces the dangling-pointer crash fixed for `addon_editors` - same
shape, same cause. Write the read/write in the same commit as the DNA field.

**One DNA touch, so batch it.** The panel list is the only DNA change needed, but fold in
anything else queued for `SpaceAddon`: 8m49s versus 1m07s for everything downstream.

### Open scope question

List or single panel? A list is "compose my own editor". A single `char panel_id[64]`
is dramatically simpler - no ListBase, no blend read/write, none of the §2.1 trap - and
may be all the Lumos case needs. Unresolved.

---

## 8. Open investigations

### 8.1 Tall panels overlapping collapsed panels below

**Symptom.** With many lights, the hosted Lumos Light Editor draws over/behind the
collapsed Lumos panels beneath it in the Add-on editor.

**Not yet attributed.** Reading Lumos's table code found nothing suspicious - it is
ordinary `layout.row(align=True)` with `column()` children, no fixed offsets, no
`scale_y`, nothing that misreports height - and the fault needs real drawing, so it
cannot be reproduced headlessly.

**The discriminator to run first.** `LUMOS_EDITOR_PT_LightEditor` is now registered in
the regular 3D Viewport sidebar too (category "Lumos"). Load many lights and look at it
*there*, with a collapsed panel beneath.

- Overlaps in the sidebar as well → nothing to do with this editor. Either the panel's
  own content or a general Blender issue with very tall panels; not our bug to fix.
- Only in the Add-on editor → ours.

**Prior: roughly 70/30 that it is not ours.** The refactor moved this content out of a
popup, where height is effectively unconstrained, into a panel that must report its
height correctly - a classic way a latent sizing problem becomes visible.

**If it is ours, look here first.** The §2.4 change made `Panel` instances persist across
re-collection instead of being freed and recreated. `sortorder`, `sizey` and `ofsy` now
carry over where they previously reset on every rebuild, and `PANEL_NEW_ADDED` no longer
fires for a reused panel. `ui::panels_end` recomputes positions each layout, so this
*should* be safe - but it is the one behaviour this editor changed in that area.

Second suspect, cheap to rule out: `RGN_FLAG_INDICATE_OVERFLOW`, set in
`addon_main_region_init` to match the Properties editor. It should only affect the
overflow indicator, not layout.

### 8.2 Using an add-on's own icon for the editor — not possible

Investigated and rejected: nothing declares one, at any level.

| Source | Icon field? |
| :--- | :--- |
| Legacy `bl_info` | No. Basis keys are name, author, version, blender, location, description, doc_url, support, category, warning, show_expanded. |
| Extension `blender_manifest.toml` | No. Known keys are id, schema_version, name, tagline, version, type, maintainer, license, blender_version_min/max, website, copyright, permissions, tags, platforms, wheels. |
| `Panel.bl_icon` / `bl_icon_value` | Exists, but unused in practice. |

`Panel.bl_icon` looked promising - it maps to `PanelType::icon`, is
`PROP_REGISTER_OPTIONAL`, and `bl_icon_value` even accepts a custom preview icon id. But
it is documented as "Icon override for the panel category tab", and Blender's own tabs
are text-only, so nobody sets it: **0 of 738 registered top-level panels** across the
installed add-on set declare either field.

Blender's own Extensions UI does show an icon per add-on, but it is derived from the
add-on *type* - `COMMUNITY` / `BLENDER` / `FILE_FOLDER` / `PACKAGE`, see `addon_type_icon`
in `bl_extension_ui.py` - which describes provenance, not identity. Every extension would
get the same icon, which is less informative than the current generic `ICON_PLUGIN`.

Add-ons that do ship artwork (Lumos has an `icons/` directory) load it into a runtime
`bpy.utils.previews` collection under arbitrary keys. There is no convention marking one
as "the add-on's icon", so nothing is discoverable.

**Conclusion:** keep `ICON_PLUGIN`. Worth revisiting only if `bl_icon` adoption changes,
which would most plausibly follow from §7 per-panel hosting giving panel authors a reason
to set it.

---

## 9. Future direction: carry the delegate in a `bContextStore` *(superseded, not built — see §3)*

Written when the only known replacement for §3's global accessor change was a heavier
mechanism with unverified failure modes. §3 has since been resolved a cheaper way — a
generic `ScrArea` field instead of a `SpaceAddon`-specific one, with the same
`ctx_wm_area_effective` logic otherwise untouched — which needed none of what follows.
Kept below as a record of the alternative that was considered, in case a future need
(per-panel delegates, §5/§7's slot design) reopens the question of whether the *mechanism*
itself, not just its storage, should change.

### Why §3 wanted replacing

`ctx_wm_area_effective` used to teach all 18 typed `CTX_wm_space_*` accessors in
`blenkernel` about one editor in `editors/` by name. It was correct and cheap, but it
inverted the normal dependency direction, and was the change most likely to be rejected
upstream. (Resolved: the accessors still call one shared helper, but the helper itself no
longer names the editor — see §3.)

Two framings worth having ready before that conversation:

**The layering already exists.** `CTX_wm_area` is not a field read:

```cpp
ScrArea *CTX_wm_area(const bContext *C)
{
  return ctx_wm_python_context_get(C, "area", RNA_Area, C->wm.area);
}
```

`ctx_wm_python_context_get`
([context.cc:423](../source/blender/blenkernel/intern/context.cc#L423)) consults the
Python override dict first - that is how `bpy.context.temp_override(area=...)` works, and
since `CTX_wm_space_data` derives from `CTX_wm_area`, space data already redirects with
it. "The area is not always the real area" is upstream behaviour. This fork added a
second, C-side redirection *below* the existing Python one; it did not invent the idea.

**Only one of the three `blenkernel` changes is actually contentious.**
`BKE_paneltypes_tag_changed` is a five-line revision counter. `PanelType::addon_id` is a
field that Python could derive instead (see `_registered_panel_classes`), at the cost of
duplicating the rule - §2.5. `ctx_wm_area_effective` is the one to defend or replace.

### The mechanism

`bContextStore` is Blender's existing answer to the exact problem that killed the
layout-scoped delegation: carrying draw-time context into deferred execution.

A store is captured on a button when the block is built, and re-established when that
button is *activated* ([interface_handlers.cc:1150](../source/blender/editors/interface/interface_handlers.cc#L1150),
[:5701](../source/blender/editors/interface/interface_handlers.cc#L5701)) and when a
context menu opens from it
([interface_context_menu.cc:578](../source/blender/editors/interface/interface_context_menu.cc#L578)).
It is what keeps `layout.context_pointer_set("modifier", md)` meaningful at click time -
draw-time knowledge surviving into a later, separate pass.

It does not currently help us: the store holds *named* entries (`name` →
`PointerRNA`/string/int) read through `CTX_data_pointer_get`, while `CTX_wm_space_data`
bypasses it entirely.

### The proposal

Carry an **area override** in the store, and have `CTX_wm_area` consult it - one touch
point instead of eighteen, with no mention of `SPACE_ADDON` anywhere in `blenkernel`.
Still a `context.cc` edit, but a generic capability rather than a special case, which is
a different conversation with a reviewer entirely. It also composes with the Python
override already sitting at that call site rather than duplicating it.

### What must be proven before adopting it

Any replacement has to survive the same three failures that drove the escalation, all of
which were observed, not theorised:

1. **Layout** - already handled by the boundary swap in `addon_main_region_layout`.
2. **Menus opened from a hosted panel**, drawn later in their own pass. The store covers
   context menus; whether it covers `layout.menu()` popups raised from a panel is the
   open question.
3. **Operator polls at button-press time.** Covered in principle by the activation path
   above, but only for buttons that carry a store.

Unverified in all three cases. Worth a spike before it is treated as the plan, and the
current implementation should not be removed until the spike passes.

### The zero-kernel fallback

If the accessor change is rejected outright and the store route does not work, the
fallback is what Blender itself does for cross-editor UI - duplicate registration. Cycles
states it plainly:

```python
# Adapt properties editor panel to display in node editor. We have to
# copy the class rather than inherit due to the way bpy registration works.
def node_panel(cls):
    node_cls = type('NODE_' + cls.__name__, cls.__bases__, dict(cls.__dict__))
    node_cls.bl_space_type = 'NODE_EDITOR'
```

Cloning an add-on's panel classes with `bl_space_type = 'ADDON'` at host time needs no
kernel change at all and works for data-only panels, which the plan's own analysis says
is most of them. It fails exactly where delegation was invented: a panel reading
`space_data.overlay` gets a `SpaceAddon` and breaks, with nothing to borrow from. A
narrower feature in exchange for no core change.

There is, notably, **no precedent anywhere in Blender for cross-editor panel hosting**.
Duplicate registration is the established practice this fork is deliberately departing
from.
