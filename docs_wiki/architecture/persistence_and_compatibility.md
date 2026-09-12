---
type: architecture
title: "Persistence and Compatibility"
description: "How the Add-on Editor's state survives .blend save/load, and what an unmodified Blender build does with a fork-saved file"
tags: [architecture, addon-editor, dna, blend-file, versioning]
last_updated: 2026-09-12
---

# Persistence and Compatibility

## `SpaceAddon` is a normal `SpaceLink`

`SpaceAddon` participates in the same `blend_write`/`blend_read_data`
machinery as every other space. Which editor an area hosts, and its
`addon_id`, save as part of the screen layout precisely like any other
editor choice, not specially, not separately.

## Curated list persistence

`UserDef.addon_editors` (`bAddonEditor` list) is the persistent, curated
list of add-ons the editor dropdown offers. `UserDef.addon_editor_max_visible`
(0 = no cap) limits how many entries the dropdown shows, in the order they
were added. See
[Curated Add-on List vs Auto-Derived List](../decisions/adr_006_curated_addon_list_vs_auto_derived.md)
and
[Capping the Editor-Type Menu](../decisions/adr_008_capping_editor_type_menu.md).

## Disabled add-ons are filtered, not stored differently

Disabling an add-on unregisters its classes, so a curated entry for it can
never draw anything until it is re-enabled.

- **Curated dropdown**: `addon_ids_get()` (`space_addon.cc`) filters
  `UserDef.addon_editors` through `addon_has_registered_panels()` — does
  at least one currently-registered top-level `PanelType` have this
  `addon_id` — before returning entries to `get`/`set`/`item_extend`. This
  reuses the same attribution mechanism `addon_panel_types_collect` already
  uses for the active add-on, and needs no cross-language call.
- **Picker**: `_installed_addon_items()` (`space_addon.py`) filters
  separately, by `addon_utils.check(module_name)[1]` (`loaded_state`),
  since picking a disabled add-on would add a curated entry the dropdown
  filter then immediately hides.

**Accepted cosmetic tradeoff.** `get`/`set`/`item_extend` share one
filtered list, since enum values are indices into it. If the area
currently hosting a since-disabled add-on has that add-on hidden from the
filtered list, `addon_space_subtype_get` cannot find it and falls back to
`ADDON_SUBTYPE_PICK` (see [Add-on Space Type](./addon_space_type.md)), so
the dropdown's highlight can show the wrong entry. `SpaceAddon::addon_id`
itself is untouched, so the region's own content resolution stays correct.
Judged not worth solving further: the highlight is cosmetic and
self-correcting.

## `.blend` fallback when the add-on is absent

Opening a fork-saved file in unmodified upstream Blender is safe, verified
against existing upstream mechanisms:

- File-version compatibility is gated by `BLENDER_FILE_VERSION`/
  `BLENDER_FILE_SUBVERSION` (`readfile.cc:1177-1196`). The test reads
  `fg->minversion` and `fg->minsubversion`, not the subversion. This fork
  does not touch `BLENDER_FILE_MIN_VERSION` (405) or
  `BLENDER_FILE_MIN_SUBVERSION` (85), so the check never fires.
  **The fork does bump `BLENDER_FILE_SUBVERSION` from 10 to 12.** That bump
  does not trigger the refusal, but it collides with upstream numbering. See
  [Upstream Fragility](./upstream_fragility.md).
- SDNA is self-describing per-file: a reading build that does not know the
  `SpaceAddon` struct does not need to, since `SpaceLink`'s common header
  (`next`/`prev`/`regionbase`/`spacetype`/`link_flag`) is what every reader
  depends on.
- `ScrArea::spacetype` resolving to an unrecognized ID already has a
  generic, pre-existing fallback, documented as "Setup a known space type
  in the event a file with an unknown space-type is loaded"
  (`area.cc:2240-2270`). It degrades the area to `SPACE_VIEW3D`. This is
  upstream behavior, not something this fork added.

Net effect: a vanilla build opening a fork-saved file loses the add-on
hosting for those areas (they become 3D Viewports) but does not crash and
does not corrupt unrelated data.

## Resaving through a build that does not know `SPACE_ADDON`

Opening a fork-saved file elsewhere is safe (above). Resaving it from
there causes real, silent data loss, per the actual write path
(`BKE_screen_area_map_blend_write`, `blenkernel/intern/screen.cc:1435-1444`):

```cpp
for (SpaceLink &sl : area->spacedata) {
  for (ARegion &region : sl.regionbase) {
    write_region(writer, &region, sl.spacetype);      // always runs
  }
  SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);
  if (space_type && space_type->blend_write) {          // gated
    space_type->blend_write(writer, &sl);
  }
}
```

Region data writes unconditionally, regardless of whether the space type
is recognized. The space-specific struct — for `SpaceAddon`, `addon_id` —
only writes if `BKE_spacetype_from_id` resolves the type and it has a
`blend_write` callback. On a build with no `SPACE_ADDON` registered, that
condition is false, so the write is skipped entirely, not written with
defaults or as raw bytes.

Combined with the read-side fallback: opening a fork-saved file in vanilla
Blender promotes a `SpaceView3D` to active for any `SPACE_ADDON` area,
while the original `SpaceAddon` entry survives in memory as a non-active
`spacedata` list member (Blender keeps one entry per editor type an area
has ever shown). If that session is then saved from vanilla, the write
loop reaches the orphaned entry, finds no recognized `SpaceType`, and drops
it. The area itself saves fine, with a working `SpaceView3D`, but the
`addon_id` and the fact the area was ever hosting an add-on is gone from
the file permanently.

**The one real compatibility cost to name plainly**: not a crash, not
corruption of anything else in the file, but silent, irreversible loss of
this fork's own screen state, specific to the round trip "saved here →
opened and resaved elsewhere." Opening without resaving, or resaving with
this fork, is unaffected either way.

## Identifying the fork without claiming to be newer

`BLENDER_FILE_VERSION` must not be bumped, since it feeds the
newer-version refusal for anyone opening a fork-saved file.
`BLENDER_FILE_SUBVERSION` is a separate matter: the fork raises it to 12
because a versioning block needs a number, and the refusal does not read
it. That number can collide with upstream. See
[Upstream Fragility](./upstream_fragility.md).
The correct lever to identify the build is `BLENDER_VERSION_SUFFIX` in
`BKE_blender_version.h` — a free-form cosmetic string, currently empty,
that does not feed into any file-compatibility check. Setting it
identifies the build in the UI without changing file-compatibility
semantics.

A de-facto identifier already exists with zero code changes: the build
hash and commit date shown by `blender --version` and in Help > About,
sourced from `buildinfo`.

## Versioning bumps this feature has needed

- `versioning_530.cc`, subversion 11 → 12: back-fills the sidebar region
  into `SPACE_ADDON` areas saved before it existed. Written to the
  upstream idiom but untested as of this writing — no pre-sidebar `.blend`
  has been loaded through it yet.
- An attempt to back-fill the sidebar region from `blend_read_data` was
  wrong and reverted: that hook only sees `SpaceLink::regionbase`, which is
  empty for the active space, since its regions live in
  `ScrArea::regionbase`.
- `ED_area_newspace` reuses a cached, non-empty region list rather than
  calling `create()` again, so an existing area never gains a newly added
  region on its own — this is why the versioning back-fill exists at all.

## DNA struct-growth pitfalls during development

Struct-shape changes to `UserDef` across several rebuilds in one session
(for example, `active_addon_editor_index` and its padding landing in a
separate rebuild from `addon_editors`) can crash on startup if a persisted
`userpref.blend` was saved by an intermediate shape and read back by a
later one — an uninitialized `bAddonEditor::module` string crashes
`strlen()` inside `addon_ids_get()`. Not a defect a real install hits,
since a fresh install has no old-shaped `userpref.blend` to read. If it
recurs, move `%APPDATA%\Blender Foundation\Blender\5.3\config\userpref.blend`
aside and relaunch, or launch with `--factory-startup` while `UserDef`'s
shape is still moving.

DNA also requires 8-byte alignment for pointer-containing members
file-wide, so an inserted field must be padded correctly — this fork hit
the alignment bug once, contributing to the crash above.

## The `USER_ADDON_EDITOR_SHOW_BUNDLED` bit, and its own DNA cost

The "show bundled add-ons" preference reuses an existing spare bit,
`eUserpref_UI_Flag2`'s `USER_UIFLAG2_UNUSED_2`, renamed to
`USER_ADDON_EDITOR_SHOW_BUNDLED`, at the same bit position — no `UserDef`
struct-shape change at all, avoiding another instance of the crash class
above. `uiflag` itself, the field a code comment recommends for new flags,
was fully saturated at 32/32 bits before this fork touched it.

**Residual risk**: if a future upstream Blender version claims this same
bit for an unrelated flag, rebasing onto that commit produces an ordinary,
loud merge conflict on that one enum line, not silent corruption.

## Related

- [Add-on Space Type](./addon_space_type.md)
- [Fork Mergeability](./fork_mergeability.md)
