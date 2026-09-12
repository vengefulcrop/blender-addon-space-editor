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
list of add-ons the editor dropdown offers.

`UserDef.addon_editor_max_visible` (0 = no cap) limits how many entries the dropdown shows.
It lists entries in the order the user added them.
See [Curated Add-on List vs Auto-Derived List](../decisions/adr_006_curated_addon_list_vs_auto_derived.md).
See [Capping the Editor-Type Menu](../decisions/adr_008_capping_editor_type_menu.md).

## Disabled add-ons are filtered, not stored differently

Disabling an add-on unregisters its classes, so a curated entry for it can
never draw anything until it is re-enabled.

- **Sidebar tree**: `AddonTreeView::build_tree()`
  (`addon_tree_view.cc`) lists an add-on only when
  `BKE_paneltypes_addon_space_types_get()` returns at least one entry for
  its module. Disabling an add-on unregisters its panel types, so the call
  returns empty and the tree drops the row until the add-on is re-enabled.

`SpaceAddon::addon_id` itself remains untouched when the tree hides the
row for a since-disabled add-on, so the region's own content resolution
stays correct. There is no editor-type dropdown enum to desync against the
tree, since `ED_spacetype_addon()` sets no `space_subtype_get`,
`space_subtype_set`, or `space_subtype_item_extend`
(`space_addon.cc:719-725`).

## `.blend` fallback when the add-on is absent

Opening a fork-saved file in unmodified upstream Blender is safe, verified
against existing upstream mechanisms:

- `BLENDER_FILE_VERSION`/`BLENDER_FILE_SUBVERSION` (`readfile.cc:1177-1196`)
  gates file-version compatibility. The test reads `fg->minversion` and
  `fg->minsubversion`, not the subversion. This fork does not touch
  `BLENDER_FILE_MIN_VERSION` (405) or `BLENDER_FILE_MIN_SUBVERSION` (85), so
  the check never fires.

  **The fork does bump `BLENDER_FILE_SUBVERSION` from 10 to 12.** That bump
  does not trigger the refusal, but it collides with upstream numbering. See
  [Upstream Fragility](./upstream_fragility.md).

- SDNA is self-describing per-file: a reading build that does not know the
  `SpaceAddon` struct does not need to, since `SpaceLink`'s common header
  (`next`/`prev`/`regionbase`/`spacetype`/`link_flag`) is what every reader
  depends on.

- `ScrArea::spacetype` resolving to an unrecognized ID already has a
  generic, pre-existing fallback, documented as "Setup a known space type
  in the event Blender loads a file with an unknown space-type"
  (`area.cc:2240-2270`). It degrades the area to `SPACE_VIEW3D`. This is
  upstream behavior, not something this fork added.

Net effect: a vanilla build opening a fork-saved file loses the add-on
hosting for those areas (they become 3D Viewports) but does not crash and
does not corrupt unrelated data.

## Resaving through a build that does not know `SPACE_ADDON`

Opening a fork-saved file elsewhere is safe (above). Resaving it from
there causes real data loss, with no warning, per the actual write path
(`BKE_screen_area_map_blend_write`, `blenkernel/intern/screen.cc:1493`):

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
Blender recognizes the space-type. The space-specific struct — for `SpaceAddon`, `addon_id` —
only writes if `BKE_spacetype_from_id` resolves the type and it has a
`blend_write` callback. On a build with no `SPACE_ADDON` registered, that
condition is false, so Blender skips the write entirely, not writing
defaults or raw bytes.

Combined with the read-side fallback: opening a fork-saved file in vanilla
Blender promotes a `SpaceView3D` to active for any `SPACE_ADDON` area,

The original `SpaceAddon` entry survives in memory as a non-active
`spacedata` list member (Blender keeps one entry per editor type an area
has ever shown). If that session is then saved from vanilla, the write
loop reaches the orphaned entry, finds no recognized `SpaceType`, and drops
it. The area itself saves fine, with a working `SpaceView3D`, but the
`addon_id` and the fact the area was ever hosting an add-on is gone from
the file permanently.

**The primary compatibility cost**: not a crash, not corrupting
anything else in the file, but silent, irreversible loss of this fork's
own screen state during a round trip (saved here, opened and resaved
elsewhere). Opening without resaving, or resaving with this fork, is
unaffected either way.

## Identifying the fork without claiming to be newer

Upstream compatibility checks read `BLENDER_FILE_VERSION`, so this fork
must not bump it.

raises it to 12 because a versioning block needs a number, and the refusal
does not read it. That number can collide with upstream. See
[Upstream Fragility](./upstream_fragility.md).

The correct lever to identify the build is `BLENDER_VERSION_SUFFIX` in
`BKE_blender_version.h`.

empty, that does not feed into any file-compatibility check. Setting it
identifies the build in the UI without changing file-compatibility
semantics.

A de-facto identifier already exists with zero code changes: the build
hash and commit date shown by `blender --version` and in Help > About,
sourced from `buildinfo`.

## Versioning bumps this feature has needed

- `versioning_530.cc`, subversion 11 → 12: back-fills the sidebar region
  into `SPACE_ADDON` areas saved before it existed. Written to upstream
  idioms, but untested as of this writing: no test loaded a pre-sidebar
  `.blend` through it yet.

- An attempt to back-fill the sidebar region from `blend_read_data` was
  wrong and reverted: that hook only sees `SpaceLink::regionbase`, which is
  empty for the active space, since its regions live in
  `ScrArea::regionbase`.

- `ED_area_newspace` reuses a cached, non-empty region list rather than
  calling `create()` again, so an existing area never gains a newly added
  region on its own. The versioning back-fill exists for this reason.

## DNA struct-growth pitfalls during development

Struct-shape changes to `UserDef` across several rebuilds in one session
(for example, `active_addon_editor_index` and its padding landing in a
separate rebuild from `addon_editors`) can crash on startup if an
intermediate shape saved a persisted `userpref.blend` and a later shape
read it back: an uninitialized `bAddonEditor::module` string crashes

`strlen()` inside `addon_display_name()` (`addon_tree_view.cc`). This is
not a defect a clean install
hits, since a fresh install has no old-shaped `userpref.blend` to read. If
it recurs, move `%APPDATA%\Blender Foundation\Blender\5.3\config\userpref.blend`
aside and relaunch, or launch with `--factory-startup` while `UserDef`'s
shape is still moving.

DNA also requires 8-byte alignment for pointer-containing members
file-wide, so a developer must pad an inserted field correctly.

## The `USER_ADDON_EDITOR_SHOW_BUNDLED` bit, and its own DNA cost

The "show bundled add-ons" preference reuses an existing spare bit,
`eUserpref_UI_Flag2`'s `USER_UIFLAG2_UNUSED_2`, renamed to
`USER_ADDON_EDITOR_SHOW_BUNDLED`, at the same bit position — no `UserDef`
struct-shape change at all, avoiding another crash of this class.
`uiflag` itself, the field a code comment recommends for new flags,
was fully saturated at 32/32 bits before this fork touched it.

**Residual risk**: if a future upstream Blender version claims this same
bit for an unrelated flag, rebasing onto that commit produces an ordinary
merge conflict on that one enum line, not corruption.

## Related

- [Add-on Space Type](./addon_space_type.md)

- [Fork Mergeability](./fork_mergeability.md)