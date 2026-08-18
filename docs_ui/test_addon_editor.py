# SPDX-License-Identifier: GPL-2.0-or-later
"""
Manual test helper for the Add-on editor (SPACE_ADDON).

Run from Blender's Text Editor (Alt-P) or the Python Console:

    exec(open(r"d:\\gamedev\\_blender_fork\\latest_pyareas\\docs_ui\\test_addon_editor.py").read())

Enables the target add-on if needed, converts the largest suitable area into an
Add-on editor, and points it at that add-on.
"""

import addon_utils
import bpy

# Add-on module name to host. Change this to test a different one.
ADDON = "node_wrangler"

# Areas that should never be taken over, so the script stays usable while testing.
KEEP = {"CONSOLE", "TEXT_EDITOR", "OUTLINER", "PROPERTIES"}


def ensure_enabled(module):
    """Enable `module` if it is not already, returning whether it is usable."""
    enabled = {m.__name__ for m in addon_utils.modules() if addon_utils.check(m.__name__)[1]}
    if module in enabled:
        return True

    available = {m.__name__ for m in addon_utils.modules()}
    if module not in available:
        print(f"[addon-editor] add-on {module!r} is not installed")
        return False

    addon_utils.enable(module, default_set=True)
    return addon_utils.check(module)[1]


def panel_count(module):
    """Number of registered sidebar panels belonging to `module`."""
    count = 0
    for cls in bpy.types.Panel.__subclasses__():
        if cls.__module__.partition(".")[0] != module:
            continue
        if getattr(cls, "bl_region_type", "") == "UI":
            count += 1
    return count


def pick_area(screen):
    """Largest area that is safe to convert, preferring one already set to ADDON."""
    for area in screen.areas:
        if area.type == "ADDON":
            return area

    candidates = [a for a in screen.areas if a.type not in KEEP]
    if not candidates:
        return None
    return max(candidates, key=lambda a: a.width * a.height)


def main():
    if not ensure_enabled(ADDON):
        return

    count = panel_count(ADDON)
    print(f"[addon-editor] {ADDON!r} has {count} sidebar panel(s) registered")
    if count == 0:
        print("[addon-editor] the editor will be empty, this add-on registers no sidebar panels")

    screen = bpy.context.screen
    area = pick_area(screen)
    if area is None:
        print("[addon-editor] no suitable area to convert, split one and rerun")
        return

    area.type = "ADDON"
    area.spaces.active.addon_id = ADDON
    area.tag_redraw()

    print(f"[addon-editor] area {area.width}x{area.height} now hosting {ADDON!r}")


main()
