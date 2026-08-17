# Add-on Space Editor — Testing Notes

**Status**: no automated tests exist yet for this feature. This note records what
Blender's own test infrastructure looks like, why the two-tier split matters for this
specific feature, and which tests are worth writing first if that gap gets closed.

---

## 1. How Blender actually tests things (as found in this tree)

Two separate, deliberately different tiers - confirmed by reading actual examples in
this checkout, not assumed from general C++ project conventions.

### GTest (C++) - for pure logic and file I/O

Lives under `source/blender/<module>/tests/*.cc`, built via `tests/gtests/CMakeLists.txt`
(`WITH_GTESTS`), run through CTest. Example precedent:
[`blenloader/tests/blendfile_load_test.cc`](../source/blender/blenloader/tests/blendfile_load_test.cc) -
loads a real `.blend` fixture from `tests/files/` and asserts on the resulting in-memory
state (`EXPECT_NE(nullptr, this->depsgraph)`).

This tier suits code that doesn't need a live window/OpenGL context: DNA round-tripping,
versioning, dependency-graph correctness, isolated data-structure logic.

**No `editors/space_addon/tests/` directory exists**, and there is no GTest coverage for
any other editor module either (`editors/space_view3d`, `editors/space_node`, etc. have
none). This isn't an oversight specific to this fork - Blender doesn't GTest its editor
modules in isolation, because they're inherently window/context/OpenGL-dependent. The
absence of GTest coverage here is *expected*, not itself a gap.

### `ui_simulate` (Python) - for interactive/UI behavior

`tests/python/ui_simulate/`. Launches a real, on-screen Blender process
(`run.py --blender=... --tests '*'`, wired into CTest at
`tests/python/CMakeLists.txt:1784` via `run_blender_setup.py`) and drives it with
synthetic input events - real keystrokes, real cursor motion - through a generator-based
(`yield`) test format, asserting against live RNA state afterward.

Closest existing precedent:
[`tests/python/ui_simulate/test_search_in_editors.py`](../tests/python/ui_simulate/test_search_in_editors.py) -
loads a fixture `.blend`, switches an area's type via
`bpy.context.temp_override(area=area): area.type = area_type`, then asserts
`space.search_filter` matches what typing produced.

`test_workspace.py` and `test_fullscreen.py` establish window/screen-state testing as an
existing category here. **No existing test creates a second window**
(`bpy.ops.wm.window_new()`) - that would be new ground for the suite, not a copy-paste of
a precedent.

**This is the tier this feature actually belongs in.** Both bugs found and fixed in this
session (the empty-state context-delegation crash, and the multi-window dropdown leak)
are UI/context-integration bugs, not pure algorithmic bugs - GTest is the wrong tool for
either; `ui_simulate` is the right one, and the search-in-editors precedent shows the
shape a test would take almost directly.

---

## 2. Tests worth writing, ranked

1. **Multi-window hide regression** (`ui_simulate`, new-ish ground). `wm.window_new()`,
   then inspect `bpy.types.Area.bl_rna.properties['ui_type'].enum_items` for an area in
   the new window under a `temp_override`, asserting no `ADDON_*`-prefixed entries
   appear - paired with the same check against the main window asserting they *do*
   appear. Cheapest to write, and exercises new-window creation, one of the more
   fragile-to-unrelated-changes areas of the codebase to regress silently in.

2. **Empty-state crash regression** (`ui_simulate`, modeled on
   `test_search_in_editors.py`). Load a fixture with an Add-on Editor area hosting a
   fake add-on declaring `NODE_EDITOR` panels only, set `preferred_delegate_spacetype`
   explicitly, close every Node Editor in the screen so collection empties out while the
   delegate is still resolvable, then assert `area.spaces.active.addon_id` is readable
   without raising and that no Python error reaches the console (needs confirming
   whether the harness already captures child-process stderr, since it launches Blender
   as a subprocess).

3. **DNA/versioning round-trip** (GTest, modeled on `blendfile_load_test.cc`). Save a
   fixture `.blend` with a `SpaceAddon` area carrying a known `addon_id`, reload, assert
   the id survives. Cheap and mechanical - exactly the class of test most likely to catch
   a future accidental struct-layout regression before it ships, which is the highest-
   consequence silent-failure mode for anything touching DNA.

4. **Panel-collection filtering** (lower priority, no clean hook today). Ideally a
   focused test of "only matching-space-type panels get collected," but
   `bpy.types.Region.panels` isn't exposed via RNA (already noted in the implementation
   log), so there's no clean Python-level assertion point. Reaching this properly means
   either exposing new RNA for test purposes (scope creep for a test-only need) or a
   friend-class GTest linked directly against `editors/space_addon`. Only worth the cost
   if this feature keeps growing.

---

## 3. Why this matters more than it might look

Every verification recorded in `addon_space_editor_plan.md`'s implementation log is
manual - "confirmed against the bundled Node Wrangler," "verified: widget metrics were
measured in pixels." None of it is asserted in a test file anywhere. Six separate
shipped-then-patched bugs are on record in that log (sentinel value, extension identity,
`invoke_search_popup` return value, blank icon, area-switch crash, and the empty-state
crash from this session) - a pattern consistent with no automated regression coverage
catching any of them before a live reproduction did. Closing even items #1 and #2 above
would directly guard the two bugs found and fixed today against silent reintroduction.
