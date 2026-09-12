---
type: decision
title: "ADR 008: Capping the Editor-Type Menu without Capping Curation"
description: "Limit how many curated entries the dropdown shows with a simple visible-count cap, instead of recency tracking and a secondary search popup"
tags: [decision, addon-editor, ux, picker]
last_updated: 2026-09-12
---

# ADR 008: Capping the Editor-Type Menu without Capping Curation

## Context

Once the curated add-on list had no upper bound in practice, the question
arose: what happens once a user curates enough add-ons that the
editor-type menu becomes unwieldy?

## Decision, and the path to it

**First direction, reverted before implementation finished.** A persisted
per-entry `last_used_time` on `bAddonEditor`, a "visible count" preference
showing the N most-recently-used entries inline, and a "More Add-ons..."
entry opening a search popup over the full curated list. Three constraints ended it mid-implementation:

1. The curated list stays exactly as curated. Nothing reorders it as a side
   effect of normal use.
2. The list grows only when the user wants it to. This is a preference, not
   a forced behavior.
3. A full list never blocks the user from picking an add-on.

**What shipped**: `UserDef.addon_editor_max_visible` (0 = no cap) limits
how many entries `addon_ids_get()` returns for the menu, in the order they
were added — natural `ListBase` order, not alphabetical or by recency.

## Alternatives considered

- **Recency-based "More..." popup**: rejected. It reorders the curated list
  as a side effect of normal use. Constraint 1 rules that out.

## Consequences

- No new per-entry state: no `last_used_time`, no DNA growth on
  `bAddonEditor`, no second picker operator.
- The cap only ever shortens what `addon_ids_get()` returns.
  The cap never trims, reorders, or otherwise touches
  `UserDef.addon_editors`. An entry past the cap stays curated and editable
  in Preferences.
- `ADDON_OT_pick_and_host` adds an entry and switches the area to host it
  unconditionally, regardless of the cap. If the addition pushes the count
  past the cap, the operator reports a standard `{'INFO'}` message naming
  the add-on and where to manage it, rather than hiding it with no
  feedback.

## Related

- [UX: Editor-Type Picker](../design/ux_addon_picker.md)
- [ADR 006](./adr_006_curated_addon_list_vs_auto_derived.md)
