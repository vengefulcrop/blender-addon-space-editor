# SPDX-License-Identifier: GPL-2.0-or-later
"""
Proves the Add-on editor actually draws panels, independently of whether any
real add-on's poll() happens to pass.

Registers a throwaway module named `addon_editor_demo` containing sidebar panels
with no poll(), then hosts it in an Add-on editor. Because panels are attributed
to an add-on by the module they were defined in, the panels register as belonging
to `addon_editor_demo` exactly as a real add-on's would.

Run from Blender's Text Editor (Alt-P) or the Python Console:

    exec(open(r"d:\\gamedev\\_blender_fork\\latest_pyareas\\docs_ui\\test_addon_editor_demo.py").read())
"""

import sys
import types

import bpy

MODULE = "addon_editor_demo"

SOURCE = '''
import bpy
from bpy.types import Panel


class DEMO_PT_main(Panel):
    bl_idname = "DEMO_PT_main"
    bl_label = "Demo Panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Demo"

    def draw(self, context):
        layout = self.layout
        layout.label(text="The Add-on editor is drawing this.")
        layout.operator("wm.splash", text="A Button")
        layout.prop(context.scene, "frame_current")


class DEMO_PT_second(Panel):
    bl_idname = "DEMO_PT_second"
    bl_label = "Second Panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Demo"

    def draw(self, context):
        # Screen-level context resolves in any editor, so this works unmodified.
        obj = context.object
        self.layout.label(text=f"Active object: {obj.name if obj else 'None'}")


class DEMO_PT_child(Panel):
    bl_idname = "DEMO_PT_child"
    bl_label = "Sub-panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Demo"
    bl_parent_id = "DEMO_PT_main"

    def draw(self, context):
        self.layout.label(text="Sub-panels resolve through the parent.")


classes = (DEMO_PT_main, DEMO_PT_second, DEMO_PT_child)
'''

KEEP = {"CONSOLE", "TEXT_EDITOR", "OUTLINER", "PROPERTIES"}


def unregister_existing():
    mod = sys.modules.get(MODULE)
    if mod is None:
        return
    for cls in reversed(getattr(mod, "classes", ())):
        try:
            bpy.utils.unregister_class(cls)
        except RuntimeError:
            pass


def main():
    unregister_existing()

    # Defining the classes inside a real module is what gives them their add-on identity.
    mod = types.ModuleType(MODULE)
    sys.modules[MODULE] = mod
    exec(compile(SOURCE, f"<{MODULE}>", "exec"), mod.__dict__)

    for cls in mod.classes:
        bpy.utils.register_class(cls)
    print(f"[demo] registered {len(mod.classes)} panels in module {MODULE!r}")

    screen = bpy.context.screen
    area = next((a for a in screen.areas if a.type == "ADDON"), None)
    if area is None:
        candidates = [a for a in screen.areas if a.type not in KEEP]
        if not candidates:
            print("[demo] no suitable area to convert, split one and rerun")
            return
        area = max(candidates, key=lambda a: a.width * a.height)
        area.type = "ADDON"

    area.spaces.active.addon_id = MODULE
    area.tag_redraw()
    print(f"[demo] area now hosting {MODULE!r} - expect 2 top-level panels and 1 sub-panel")


main()
