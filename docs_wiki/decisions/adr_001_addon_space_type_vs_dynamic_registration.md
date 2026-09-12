---
type: decision
title: "ADR 001: Space Subtype vs Dynamic Registration"
description: "Register one SPACE_ADDON type with add-ons as subtypes, instead of registering a new SpaceType per add-on at runtime"
tags: [decision, addon-editor, space-type, dna]
last_updated: 2026-09-12
---

# ADR 001: Space Subtype vs Dynamic Registration

## Context

The original design (`legacy/custom_python_space_types_architecture.md`)
proposed extending `BKE_spacetype_register()` / `BKE_spacetype_from_id()`
to manage space types registered dynamically at runtime from Python, with
full lifecycle and teardown management. Estimated at ~80-120 LOC of new
kernel machinery, part of an ~850-1,200 LOC total design.

## Decision

Register one new space type, `SPACE_ADDON`. Every hosted add-on is a
subtype of it, using the existing subtype mechanism at
`BKE_screen.hh:156-158` (`space_subtype_get`/`_set`/`_item_extend`) and the
existing subtype-folding logic in `rna_Area_ui_type_itemf`
(`rna_screen.cc:216-233`).

This is the same mechanism the Node Editor uses
to present Shader, Compositor, and Geometry Nodes as three dropdown
entries while being one registered space type.

## Alternatives considered

- **Dynamic per-add-on `SpaceType` registration**: rejected. It requires
  new kernel lifecycle and teardown machinery, and introduces an
  unregister-crash risk from a torn-down `SpaceType` while an area still
  displays it.

## Consequences

- No dynamic registration, no runtime `SpaceType` allocation, no teardown
  lifecycle, and no dangling-`SpaceType` crash risk, because no
  `SpaceType` is ever created or destroyed at runtime.

- Subtype indices are not stable across sessions, since add-on
  enable/disable reorders them. The index is a view concern only. The DNA
  stores the add-on's module name as a string
  (`SpaceAddon::addon_id`), mirroring how `SpaceNode` stores the node-tree
  type idname rather than its index.

- Estimated code volume for space registration dropped from ~80-120 LOC of
  new kernel machinery to near zero. The total design estimate dropped
  from ~850-1,200 LOC to ~600-800 LOC.

## Related

- [Add-on Space Type](../architecture/addon_space_type.md)
- [Fork Mergeability](../architecture/fork_mergeability.md)

