---
type: spec
title: "UX: Editor-Type Picker"
description: "How a user adds an add-on to the editor-type dropdown, curates it, and picks a delegate editor"
tags: [design, addon-editor, ux, picker]
last_updated: 2026-09-12
---

# UX: Editor-Type Picker

## The curated list, not an auto-derived list

The editor-type dropdown does not list every add-on that happens to
register panels. Listing every registered add-on made the menu grow and
shrink as unrelated add-ons were toggled, with no way to remove an unwanted
entry.

Instead, `UserDef.addon_editors` is a persistent, user-curated list. The
editor dropdown has an "Add-ons" heading and a first entry,
"Add an Add-on...", opening a search popup (`ADDON_OT_pick_and_host`) over
installed add-ons. Picking one adds it to the curated list and hosts it
immediately. The same list is manageable from Preferences > Add-ons, via a
panel and a `UIList`, so curation is not dropdown-only. See
[Curated Add-on List vs Auto-Derived List](../decisions/adr_006_curated_addon_list_vs_auto_derived.md).

## Picker mechanics

- `invoke_search_popup` returns `None`, not an operator result set.
  `invoke()` must call it and separately return `{'RUNNING_MODAL'}`.
- The operator needs `bl_property` naming which `EnumProperty` to search;
  omitting it fails with `"has no enum property set"`.

## Extension display names

Extension entries in the dropdown originally showed their raw module id
(`bl_ext.<repository>.<addon>`) instead of a human-readable name. Legacy
add-ons were unaffected, since their module id already reads as a name
(`node_wrangler`).

**Fix**: `bAddonEditor` gained a `name` field, captured once via
`addon_utils.module_bl_info()` when the picker operator adds the entry, not
resolved live at draw time, since only Python can resolve `bl_info`/
manifest data and the dropdown is built in C. `addon_ids_get()` returns
`id`/`label` pairs; the identifier used for matching stays the module id,
only the displayed text changes. Sorting moved from module-id order to
label order, since that is what is visible in the menu.

**Known limitation, accepted rather than solved**: the name is a snapshot
from add-time, not re-resolved if the add-on's declared name later
changes. Consistent with the curated-list design accepting
"remove and re-add" as the correction path for a stale entry generally.

## Disabled add-ons hidden from the picker and the dropdown

Disabling an add-on unregisters its classes, so an entry for it can never
draw anything. Both surfaces filter it out; see
[Persistence and Compatibility](../architecture/persistence_and_compatibility.md#disabled-add-ons-are-filtered-not-stored-differently)
for the mechanism.

## Bundled add-ons: opt-in, not filtered out

A first instinct was to filter Blender's own bundled add-ons (Cycles, Pose
Library, format importers/exporters) out of the picker entirely, since
they appeared to draw nothing when picked. Checked before building that:
`CyclesButtonsPanel` gates on `COMPAT_ENGINES = {'CYCLES'}`, so its panels
only draw when Cycles is the active render engine — a legitimate, working
`poll()` condition, confirmed by manually switching the render engine and
seeing the panels draw.

Filtering these out permanently would discard real, functioning capability
over a condition (which render engine happens to be active) this fork has
no business predicting.

**Resolved as an opt-in preference**: `Preferences.show_addon_editor_bundled`,
off by default, with a checkbox. The distinction is deterministic and
path-based, not a `poll()` prediction: `_addon_is_bundled()` checks whether
the module's `__file__` contains `addons_core`, versus a user's Extensions
or legacy add-ons directory. See
[Bundled Add-ons as Opt-In Preference](../decisions/adr_009_bundled_addons_opt_in.md).

Bundled add-ons, when enabled, are listed at the very top of the picker.

## Capping the editor-type menu

Once curation had no upper bound in practice, the design considered a
richer solution — a persisted per-entry `last_used_time`, a "visible
count" preference, and a "More Add-ons..." popup — then reverted it before
implementation finished, once three constraints were stated: the curated
list must stay exactly as curated (never reordered as a side effect of
normal use), it must never grow out of control only when the user wants
it to (a preference, not forced), and picking an add-on must never be
blocked by a full list.

**What shipped**: `UserDef.addon_editor_max_visible` (0 = no cap) limits
how many entries `addon_ids_get()` returns for the menu, in the order they
were added. `UserDef.addon_editors` itself is never trimmed, reordered, or
otherwise touched by the cap. Picking always works regardless of the cap;
if an addition pushes the count past it, the operator reports a standard
`{'INFO'}` message naming the add-on and where to manage it. See
[Capping the Editor-Type Menu](../decisions/adr_008_capping_editor_type_menu.md).

## Letting the user pick which editor an add-on delegates to

An add-on registering panels for more than one space type (for example,
ucupaint, for `VIEW_3D` and `NODE_EDITOR`) had its delegate resolved by an
automatic "first declared type with an editor open wins" scan, with no way
for the user to see or change the outcome.

**The UI**, after iterating through three designs:
1. A row of `prop_enum` icon buttons, one per supported type — dropped in
   favor of a dropdown, compared against the 3D Viewport's own
   interaction-mode selector.
2. `layout.prop(space, "preferred_delegate_spacetype")` directly — works,
   but item labels are Blender's plain editor names ("UV/Image Editor"),
   and the request was to prefix each with the add-on's own name
   ("Lumos: UV/Image Editor"), which a directly-drawn property cannot
   customize per item.
3. **Landed on**: `ADDON_OT_set_preferred_delegate_spacetype`, a small
   Python operator with its own dynamic `items` callback (the same idiom
   `ADDON_OT_pick_and_host` already uses), drawn via
   `layout.operator_menu_enum()`. The real, C-defined property stays the
   single source of truth for storage and resolution; only its
   presentation moved to Python.

See [Context Delegation](../architecture/context_delegation.md#user-chosen-delegate-preference)
for the storage and resolution mechanism this UI controls.

## Related

- [Add-on Space Type](../architecture/addon_space_type.md)
- [UX: Sidebar and Bookmarks](./ux_sidebar_bookmarks.md)
