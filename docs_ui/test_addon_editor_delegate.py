# SPDX-License-Identifier: GPL-2.0-or-later
"""
Tests context delegation in the Add-on editor.

Node Wrangler's panel polls:

    space.type == 'NODE_EDITOR' and space.node_tree is not None

Without delegation it is collected but never drawn, because `space_data` in an Add-on
editor is a SpaceAddon. With delegation the editor borrows an open Node Editor for the
duration of the panel layout, and the panel draws.

This sets up every precondition: enables the add-on, gives the active object a material
so a shader node tree exists, opens a Shader Editor, and hosts Node Wrangler in an
Add-on editor.

Run from Blender's Text Editor (Alt-P) or the Python Console:

    exec(open(r"d:\\gamedev\\_blender_fork\\latest_pyareas\\docs_ui\\test_addon_editor_delegate.py").read())
"""

import addon_utils
import bpy

ADDON = "node_wrangler"
KEEP = {"CONSOLE", "TEXT_EDITOR"}


def ensure_material():
    """The Node Wrangler poll needs a node tree, which needs a material."""
    obj = bpy.context.view_layer.objects.active
    if obj is None:
        obj = next((o for o in bpy.context.view_layer.objects), None)
        if obj is None:
            print("[delegate] no object in the scene")
            return False
        bpy.context.view_layer.objects.active = obj

    if not obj.data or not hasattr(obj.data, "materials"):
        print(f"[delegate] {obj.name!r} cannot hold materials")
        return False

    if not obj.material_slots:
        mat = bpy.data.materials.new("AddonEditorTest")
        mat.use_nodes = True
        obj.data.materials.append(mat)
        print(f"[delegate] created material on {obj.name!r}")
    return True


def main():
    if not addon_utils.check(ADDON)[1]:
        addon_utils.enable(ADDON, default_set=True)
    print(f"[delegate] {ADDON} enabled: {addon_utils.check(ADDON)[1]}")

    if not ensure_material():
        return

    areas = [a for a in bpy.context.screen.areas if a.type not in KEEP]
    areas.sort(key=lambda a: a.width * a.height, reverse=True)
    if len(areas) < 2:
        print("[delegate] need at least two areas, split the window and rerun")
        return

    # Largest area hosts the add-on, second largest becomes the editor it borrows from.
    addon_area, node_area = areas[0], areas[1]

    node_area.type = "NODE_EDITOR"
    node_space = node_area.spaces.active
    node_space.tree_type = "ShaderNodeTree"
    node_space.shader_type = "OBJECT"
    print(f"[delegate] node editor ready, node_tree: {node_space.node_tree is not None}")

    addon_area.type = "ADDON"
    addon_area.spaces.active.addon_id = ADDON
    addon_area.tag_redraw()

    print(f"[delegate] hosting {ADDON!r} - its panel should now draw via the Node Editor")
    print("[delegate] close or change the Node Editor and the panel should disappear again")


main()
