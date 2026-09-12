---
type: operation
title: "Add-on Space Editor — Testing"
description: "How to run the Add-on Space Editor test scripts, what each covers, and the known coverage gaps"
tags: [addon-space-editor, testing, ui-simulate]
last_updated: 2026-09-12
---

# Testing

Consolidated from `docs_ui/addon_space_editor_testing.md` and the three
manual test scripts under `docs_ui/`. No automated tests exist yet for
this feature. This page records how Blender's own test infrastructure
works, why one tier fits this feature and the other does not, which
manual scripts exist today, and which automated tests are worth writing
first.

## 1. How Blender tests things

Blender uses two separate, deliberately different tiers.

### GTest (C++), for pure logic and file I/O

Lives under `source/blender/<module>/tests/*.cc`, built through
`tests/gtests/CMakeLists.txt` (`WITH_GTESTS`), run through CTest.
Precedent: `blenloader/tests/blendfile_load_test.cc` loads a real
`.blend` fixture from `tests/files/` and asserts on the resulting
in-memory state (`EXPECT_NE(nullptr, this->depsgraph)`).

This tier suits code that needs no live window or OpenGL context: DNA
round-tripping, versioning, dependency-graph correctness, isolated
data-structure logic.

No `editors/space_addon/tests/` directory exists, and no other editor
module has GTest coverage either. Blender does not GTest its editor
modules in isolation, because they depend inherently on window, context,
and OpenGL state. The absence of GTest coverage here is expected, not a
gap specific to this fork.

### `ui_simulate` (Python), for interactive and UI behaviour

Lives under `tests/python/ui_simulate/`. Launches a real, on-screen
Blender process (`run.py --blender=... --tests '*'`, wired into CTest at
`tests/python/CMakeLists.txt:1784` through `run_blender_setup.py`) and
drives it with synthetic input events, real keystrokes and real cursor
motion, through a generator-based (`yield`) test format, then asserts
against live RNA state.

Closest precedent: `tests/python/ui_simulate/test_search_in_editors.py`.
It loads a fixture `.blend`, switches an area's type through
`bpy.context.temp_override(area=area): area.type = area_type`, then
asserts `space.search_filter` matches what typing produced.

`test_workspace.py` and `test_fullscreen.py` already establish
window/screen-state testing as a category in this suite. No existing test
creates a second window (`bpy.ops.wm.window_new()`); that would be new
ground, not a copy of a precedent.

**This is the tier this feature belongs in.** The empty-state
context-delegation crash and the multi-window dropdown leak, both fixed
and recorded in [bugfix.md](./bugfix.md) items 9 and 10, are
UI/context-integration bugs, not pure algorithmic bugs. GTest is the
wrong tool for either; `ui_simulate` is the right one, and the
search-in-editors precedent shows the shape a test would take almost
directly.

## 2. The manual test scripts that exist today

None of these are automated. Each is a standalone script, run by hand
from Blender's Text Editor or Python Console with
`exec(open(<path>).read())`. They exercise the feature interactively but
assert nothing programmatically.

### `docs_ui/test_addon_editor.py`

Enables a target add-on if needed (default `node_wrangler`, set through
the `ADDON` module-level variable), converts the largest suitable area
into an Add-on editor, and points it at that add-on. `ensure_enabled()`
checks and enables the add-on through `addon_utils`. `panel_count()`
counts the add-on's registered sidebar (`UI` region) panels and warns if
the count is zero, since the editor will then be empty. `pick_area()`
reuses an existing `ADDON` area if one exists, otherwise picks the
largest area not in the `KEEP` set (`CONSOLE`, `TEXT_EDITOR`, `OUTLINER`,
`PROPERTIES`), so the script stays usable while testing.

### `docs_ui/test_addon_editor_delegate.py`

Tests context delegation specifically. Node Wrangler's panel polls
`space.type == 'NODE_EDITOR' and space.node_tree is not None`. Without
delegation the panel is collected but never drawn, because
`space_data` in an Add-on editor is a `SpaceAddon`. With delegation the
editor borrows an open Node Editor for the duration of the panel layout,
and the panel draws. `ensure_material()` gives the active object a
material so a shader node tree exists, since the poll needs one. `main()`
then needs at least two non-`KEEP` areas (`CONSOLE`, `TEXT_EDITOR`):
the largest becomes the Add-on editor hosting `node_wrangler`, the second
largest becomes a Node Editor set to `ShaderNodeTree` / `OBJECT`, which
the Add-on editor should borrow.

### `docs_ui/test_addon_editor_demo.py`

Proves the Add-on editor actually draws panels, independently of whether
any real add-on's `poll()` happens to pass. It registers a throwaway
module named `addon_editor_demo` at runtime, containing three panels
with no `poll()`: `DEMO_PT_main` and `DEMO_PT_second` (both `VIEW_3D` /
`UI`, category "Demo"), and `DEMO_PT_child`, a sub-panel of
`DEMO_PT_main`. Panels are attributed to an add-on by the module they are
defined in, so these panels register as belonging to `addon_editor_demo`
exactly as a real add-on's would. `unregister_existing()` cleans up a
previous run's classes before re-registering. `main()` reuses an
existing `ADDON` area or converts the largest suitable one, then sets
`area.spaces.active.addon_id = "addon_editor_demo"`. Expected result: two
top-level panels and one sub-panel drawn.

## 3. Tests worth writing, ranked

None of these exist yet.

1. **Multi-window hide regression** (`ui_simulate`, new ground for the
   suite). Call `wm.window_new()`, then inspect
   `bpy.types.Area.bl_rna.properties['ui_type'].enum_items` for an area
   in the new window under a `temp_override`, asserting no
   `ADDON_*`-prefixed entries appear, paired with the same check against
   the main window asserting they do appear. Cheapest to write, and
   guards [bugfix.md](./bugfix.md) item 10 against silent
   reintroduction.
2. **Empty-state crash regression** (`ui_simulate`, modeled on
   `test_search_in_editors.py`). Load a fixture with an Add-on Editor
   area hosting a fake add-on that declares `NODE_EDITOR` panels only,
   set `preferred_delegate_spacetype` explicitly, close every Node
   Editor in the screen so collection empties out while the delegate is
   still resolvable, then assert `area.spaces.active.addon_id` is
   readable without raising and that no Python error reaches the
   console. Confirm first whether the harness captures child-process
   stderr, since it launches Blender as a subprocess. Guards
   [bugfix.md](./bugfix.md) item 9.
3. **DNA/versioning round-trip** (GTest, modeled on
   `blendfile_load_test.cc`). Save a fixture `.blend` with a
   `SpaceAddon` area carrying a known `addon_id`, reload, assert the id
   survives. Cheap and mechanical, and the class of test most likely to
   catch a future accidental struct-layout regression before it ships.
4. **Panel-collection filtering** (lower priority, no clean hook today).
   A focused test of "only matching-space-type panels get collected" has
   no clean Python-level assertion point, since `bpy.types.Region.panels`
   is not exposed through RNA. Reaching this properly means either
   exposing new RNA for test purposes, which is scope creep for a
   test-only need, or a friend-class GTest linked directly against
   `editors/space_addon`. Worth the cost only if this feature keeps
   growing.

## 4. Why this matters

Every verification recorded in the implementation log behind
`docs_ui/legacy/code_review.md` is manual: confirmed against the bundled
Node Wrangler, widget metrics measured in pixels by hand. None of it is
asserted in a test file. Six separate shipped-then-patched bugs are on
record (sentinel value, extension identity, `invoke_search_popup` return
value, blank icon, area-switch crash, and the empty-state crash), a
pattern consistent with no automated regression coverage catching any of
them before a live reproduction did. Closing items 1 and 2 above would
directly guard the two bugs fixed in the same session,
[bugfix.md](./bugfix.md) items 9 and 10, against silent reintroduction.
