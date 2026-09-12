---
type: operation
title: "Add-on Space Editor — Defect List"
description: "Open and fixed defects for the Add-on Space Editor, from the code review and testing notes"
tags: [addon-space-editor, bugfix, defects]
last_updated: 2026-09-12
---

# Defect List

Consolidated from `docs_ui/legacy/code_review.md` (reviewed 2026-08-01
against `main` @ `027ef661`) and `docs_ui/addon_space_editor_testing.md`.
The document marks an item FIXED when the source or the git log shows the fix
landed. See [todo.md](./todo.md) for open enhancement work, not defects.

## 1. `U.addon_editors` was never saved or loaded — FIXED

**Symptom.** The developer added a new `ListBase` to `UserDef` with no
corresponding blenloader support. `writefile.cc:1301` writes
`userdef->addons` but nothing wrote `addon_editors`. `readfile.cc:4079-4088`
has a `BLO_read_struct_list` call for every other `UserDef` list, but none
for `bAddonEditor`.

**Consequence.** The curated editor list did not survive a restart. Worse,
the raw `UserDef` struct was still written, including the `ListBase`
head/tail pointers, which are live in-memory addresses never remapped on
read. Iterating `U.addon_editors` in `addon_ids_get()`
(`space_addon.cc:462`) after such a load dereferenced stale pointers,
which is a crash, not a lost setting.

**Fix.** Added the write loop in `writefile.cc`, the
`BLO_read_struct_list` call in `readfile.cc`, and
`addon_editors.free_no_destruct()` in `BKE_blender_userdef_data_free`,
which also leaked the list. `addon_editors` is now `VALUE_SWAP`ped in
`BKE_blender_userdef_app_template_data_swap` alongside `addons`.

**Verified.** Headlessly, against an isolated `BLENDER_USER_RESOURCES`
directory. One process wrote two entries and called
`wm.save_userpref()`. A second, fresh process read both entries back with
module, name, order, and flag intact.

## 2. Reused userpref flag bit was unsafe — FIXED

**Symptom.** `USER_ADDON_EDITOR_SHOW_BUNDLED` reused
`USER_UIFLAG2_UNUSED_2` (`DNA_userdef_types.h:412`). The bit previously
held `USER_TRACKPAD_NATURAL` until commit `055ed335a11` (November 2020,
2.92 development) removed that preference and renamed the bit without
adding a versioning clear.

**Suspected cause.** Any `userpref.blend` predating 2.92 whose owner had
Natural Trackpad enabled still has bit 2 set, and would get
`show_addon_editor_bundled` silently turned on.

**Fix.** Bumped `BLENDER_FILE_SUBVERSION` to 11 and added a
`!USER_VERSION_ATLEAST(503, 11)` block in `versioning_userdef.cc` that
clears the bit, plus a `/* cleared */` annotation on the enum.

**Verification gap.** Not verified at runtime. This needs a
`userpref.blend` written by a pre-2.92 Blender with Natural Trackpad
enabled, which was not available.

## 3. Panel enumeration was wrong in two ways — FIXED

**Symptom.** `_addon_top_level_panel_space_types` used
`bpy.types.Panel.__subclasses__()`, which is direct-only and unfiltered.

**Suspected cause, confirmed.** The function counted unregistered base
classes. Ten classes in the tree are `Panel` subclasses that are never
registered, usually an add-on's own panel base. All ten are direct
subclasses, so the original code counted them.

Concretely, ucupaint's unregistered `Y_PT_UDIM_Atlas_menu` made the add-on
report `IMAGE_EDITOR`, which no registered ucupaint panel declares.
Indirect subclasses were also missed: an add-on deriving its panels from
its own base, such as mio3_uv's five such panels, was invisible.

**Fix.** `_registered_panel_classes()` now walks the subclass tree
iteratively and filters on `is_registered`, matching the set the C++ side
scans.

**Verified.** Against the live build. ucupaint now reports
`['NODE_EDITOR', 'VIEW_3D']` instead of
`['IMAGE_EDITOR', 'NODE_EDITOR', 'VIEW_3D']`. The walk finds all 1267
registered classes.

## 4. Panel layout was saved to disk, then destroyed on load — FIXED

**Symptom.** Panel layout persistence (`bScreen` to `ScrArea` to
`SpaceLink` plus regions) worked for saving, since `write_area`
(`screen.cc:1412`) calls `write_panel_list` unconditionally for every
space type. The interface then discarded the layout on the first redraw.

**Suspected cause.** `addon_blend_read_data` allocates a fresh
`SpaceAddon_Runtime` whose `cached_addon_id` is empty
(`addon_intern.hh:25`), so the first `addon_main_region_layout` compares
`STREQ("", "node_wrangler")`, misses the cache, and calls
`BKE_area_region_panels_free(&region->panels)`, freeing every `Panel`
just read from the file before `panel_find_by_type` can match it.

The same wipe fired mid-session on any screen-signature change, including
opening or closing an unrelated editor.

**Fix.** `addon_panel_types_collect` now sets the previous list aside,
moves back any entry whose ID name is still wanted, refreshing its
contents in place, and allocates only for genuinely new ones. `Panel::type`
stays valid for survivors. The code detaches panels that really went away
(`Panel::type = nullptr`) before freeing their copies. The
`BKE_area_region_panels_free` call is gone.

**Verified.** Interactive testing in the built Blender confirmed this
behavior. Only a real layout pass instantiates panels. The rebuild
required no DNA recompile and took 1m07s.

**Remaining caveat.** A `.blend` or workspace containing an Add-on editor
opened in stock Blender degrades the unregistered space type to
`SPACE_EMPTY` (`screen.cc:1620`) and stashes the original in
`butspacetype`. There is no crash, but `SpaceAddon` is an unknown DNA
struct there, so the file loses `addon_id` if that user resaves. Workspace
presets are one-way.

## 5. Two implementations of one attribution rule — OPEN

**Symptom.** C++ `BPY_class_module_name_get` truncates to the top-level
module, three segments for `bl_ext.`, and matches with `STREQ`. Python
`_addon_top_level_panel_space_types` matches by module prefix. They agree
today, but the comment in `addon_panel_types_collect` asserts they are
the same filter, which invites changing one without the other.

**Suggested fix.** A cross-reference note on both sides, or expose the
C++ answer to Python so there is one implementation. See
[todo.md](./todo.md) item 5.

## 6. Mixed-editor add-ons polled against the wrong space, hanging Blender — FIXED

**Incident.** 2026-08-01, while hosting ucupaint. Blender stopped
responding after closing and reopening a 3D Viewport. The process was
alive with only 7 seconds of CPU time, so a wait blocked it, not looping.

**Root cause.** ucupaint registers panels for both the Node Editor and
the 3D Viewport. With the viewport closed, collection gathered only its
`NODE_EDITOR` panels, and the delegate was `NODE_EDITOR`. Reopening the
viewport brought the `VIEW_3D` panels back into collection, and
`addon_context_delegate_find` returned the first match, now `VIEW_3D`.
`NODE_PT_YPaintUI.poll()` then ran against a `SpaceView3D`, accessed
`context.space_data` unguarded, and raised on every redraw.

**Amplifier.** Blender prints a Python traceback and continues, which is
correct once and ruinous at redraw rate. On Windows the console applies
backpressure, and the main thread blocks in `WriteConsoleW`, presenting
as unresponsive with no CPU use and no crash log.

**Fix (A).** `addon_delegate_spacetype_find` now picks the editor type up
front, and `addon_panel_types_collect` keeps only panels declaring that
type, plus space-agnostic ones. No panel is ever polled against a space
it was not written for.

The design kept the "first declared type with an editor open wins" rule:
with both editors open, ucupaint's Node Editor panels no longer appear in
an area delegating to the viewport.

**Fix (B), defence in depth.** See item 7 below.

## 7. A raising `poll()` was reported every redraw — FIXED

**Symptom.** Same incident as item 6. A raising `poll()` is an expected
condition in this editor, since it deliberately hosts panels outside
their native context, but the console spam at redraw rate blocked the
main thread through `WriteConsoleW` backpressure.

**Fix.** Panel type copies get their `poll` replaced with
`addon_panel_poll_guarded`, which mirrors `rna_ui.cc`'s `panel_poll` but
checks the return code of `rna_ext.call` to distinguish "returned false"
from "raised." The editor records a panel that raises by ID name, and does
not poll it again until the registered panel types change.

## 8. Crash switching an Add-on editor area to a stock editor type — FIXED

**Incident.** 2026-08-05, reported after the multi-editor preference
feature shipped.

**Suspected cause, confirmed.** A residual gap in the earlier
context-delegation refactor. `ScrArea::context_delegate_spacetype` was
set and read but never cleared, so it leaked across an area's editor-type
change, mismatching a new editor's own region against an unrelated
area's space data.

**Fix.** Cleared generically in `ED_area_newspace()`. The issue produced
no further crashes since.

## 9. Empty-state context-delegation crash — FIXED

**Symptom.** Recorded in `docs_ui/addon_space_editor_testing.md` as one
of two bugs found and fixed in the testing session: a crash in the
empty-state path during context delegation.

**Status.** Fixed in that session, alongside item 10. No automated
regression test exists yet for this case. See
[testing.md](./testing.md) item 2 for the recommended `ui_simulate` test.

## 10. Multi-window dropdown leak — FIXED

**Symptom.** Recorded alongside item 9 in
`docs_ui/addon_space_editor_testing.md`: an `ADDON_*`-prefixed dropdown
entry leaked into a second window's `ui_type` enum.

**Status.** Fixed in the same session as item 9. Currently mitigated at
the area-type-picker level by hiding the Add-on Editor option in
non-main windows (commit `af9bcda50c7` ("Add-on Editor: hide the Add-ons menu section in non-main windows")). The underlying multi-window
delegation gap is still open, see [todo.md](./todo.md) items 7 to 9. No
automated regression test exists yet. See [testing.md](./testing.md)
item 1.

## 11. Tall panels overlap collapsed panels below — OPEN, not yet attributed

**Symptom.** With many lights, the hosted Lumos Light Editor draws
over or behind the collapsed Lumos panels beneath it, in the Add-on
editor.

**Suspected cause, unconfirmed.** Lumos's own table code (an ordinary
`layout.row(align=True)` with `column()` children) shows nothing
suspicious. The fault needs real drawing, so the test suite cannot
reproduce it headlessly.

**Discriminator to run first.** `LUMOS_EDITOR_PT_LightEditor` is also
registered in the regular 3D Viewport sidebar, under category "Lumos."
Load many lights and check it there, with a collapsed panel beneath. If
it overlaps there too, the fault belongs to the panel's own content or
to a general Blender issue with tall panels, not to this editor. If it
overlaps only in the Add-on editor, the fault belongs here.

**Estimate, if it is ours.** Roughly 70/30 that it is not ours. The
refactor moved this content out of a popup, where height is effectively
unconstrained, into a panel that must report its height correctly.

If it is ours, the first suspect is the item 4 fix above: `Panel`
instances now persist across re-collection instead of freeing and
recreated, so `sortorder`, `sizey`, and `ofsy` now carry over where they
previously reset on every rebuild, and `PANEL_NEW_ADDED` no longer fires
for a reused panel. `ui::panels_end` recomputes positions each layout, so
this should be safe, but it is the one behaviour this editor changed in
that area.

Second suspect, cheap to rule out: `RGN_FLAG_INDICATE_OVERFLOW`, set in
`addon_main_region_init` to match the Properties editor. It should only
affect the overflow indicator, not layout.

## 12. `U.addon_bookmarks` was never freed and never swapped — FIXED

**Symptom.** `bAddonBookmark` got only half of the treatment that item 1
gave `bAddonEditor`. Blender reads the list (`readfile.cc:4085`) and
writes it (`writefile.cc:1312`). The code missed two calls:
- `BKE_blender_userdef_data_free()` did not free it. The
  `addon_editors.free_no_destruct()` call sat one line above with no
  match for the bookmarks.
- `BKE_blender_userdef_app_template_data_swap()` did not swap it. The
  `VALUE_SWAP(addon_editors)` call sat one line above.

**Consequence.** Two faults:
1. A leak. Every `bAddonBookmark` leaks each time Blender frees a
   `UserDef`. This happens on a preferences reload, on
   `wm.read_userpref()`, and on an application template change.
2. The wrong data. Change the application template. The add-ons swap.
   The editors curated against them swap. The bookmarks stay. The
   Bookmarks sidebar panel (`ADDON_PT_bookmarks`) then shows, and can
   activate, the bookmarks of the other template.

**Fix.** Added `userdef->addon_bookmarks.free_no_destruct()` next to the
`addon_editors` call, and `VALUE_SWAP(addon_bookmarks)` next to its own.
`bAddonBookmark` holds no owned pointer, the same as `bAddonEditor`, so
`free_no_destruct` is correct. Extended the swap comment to name both
lists.

**Found by.** A pre-alpha audit on 2026-09-12. Verified by reading all
four call sites, not by running Blender.

## 13. Two stale comments — FIXED

**Symptom.** Two comments described code that does not exist:
- `space_addon.cc`, the file header, said developers added panel collection and drawing
  in a later step. The file holds
  `addon_panel_types_collect`, `addon_main_region_layout`, and
  `addon_main_region_draw`.
- `DNA_space_types.h`, the `SpaceAddon::addon_id` comment, said the
  editor treats a leftover `0x01` byte as unset. It does not. Code tests
  emptiness as `addon_id[0] == 0` (`screen.cc:380`,
  `space_addon.cc:627`), so the byte reads as set.

**Consequence.** The second comment misleads a reader about the failure
mode. A `0x01` byte resolves to no add-on, and the editor draws the empty
state. The screen the user sees is the same. The reason in the comment
was wrong.

**Fix.** Rewrote both comments.

## Deliberately deferred, not open defects

- **Modal operators need the delegate's region at invoke time**
  (`convertViewVec: called in an invalid context` console spam).
  Standing and unconditional. See [todo.md](./todo.md) items 10 to 12
  for the planned fix.
- **Properties-style panels get no `bl_context` filtering** (`contexts`
  passed as `nullptr`). Accepted in the plan.
- **Per-area add-on internal tab state** (the SourceOps case). Developers left this
  item not fixed. See [todo.md](./todo.md), "Explicitly out of scope."
