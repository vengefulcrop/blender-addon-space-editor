---
type: decision
title: "ADR 006: Curated Add-on List vs Auto-Derived List"
description: "Offer the editor-type dropdown from a persistent, user-curated list, instead of deriving it from every add-on that registers panels"
tags: [decision, addon-editor, ux, picker]
last_updated: 2026-09-12
---

# ADR 006: Curated Add-on List vs Auto-Derived List

## Context

The original plan used a cheap derivation. It listed every add-on that
happened to have panels registered. This shipped first. A review then
replaced it.

## Decision

`UserDef.addon_editors` (a new `bAddonEditor` list, mirroring the existing
`bAddon` pattern) is a persistent, user-curated list. The editor dropdown
gets an "Add-ons" heading and a first entry, "Add an Add-on...". This entry
opens a search popup (`ADDON_OT_pick_and_host`) over installed add-ons.
Picking one adds it to the curated list and hosts it immediately. A panel
and a `UIList` in Preferences > Add-ons also manage the same list.

## Alternatives considered

- **Auto-derived list, "every add-on with registered panels"**: rejected
  after shipping. It made the menu grow and shrink as unrelated add-ons
  were toggled, with no way to remove an unwanted entry.

## Consequences

- The dropdown stays stable across unrelated add-on enable and disable
  actions, except where an entry becomes fully unusable (see below).
- The team caught and fixed three bugs while building this. The "Add an
  Add-on..." entry's sentinel subtype value collided with reserved and
  packing-sensitive values; the fix set it to `0x7FFF`. Extension module
  identity collapsed to `"bl_ext"` for every extension before the `bl_`
  prefix rule filtered it out. The fix keeps three module-id segments for
  that prefix. `invoke_search_popup`'s `None` return value was mishandled;
  the fix now handles it.
- The system must filter disabled add-ons from both the curated dropdown
  and the picker, since a curated entry for a disabled add-on can never
  draw anything. See
  [Persistence and Compatibility](../architecture/persistence_and_compatibility.md#disabled-add-ons-are-filtered-not-stored-differently).
- This decision introduced the need for a display-name field
  (`bAddonEditor.name`), since the curated list's identifier (the module
  id) is not always readable as a display name for extensions.

## Related

- [UX: Editor-Type Picker](../design/ux_addon_picker.md)
- [Add-on Space Type](../architecture/addon_space_type.md)
