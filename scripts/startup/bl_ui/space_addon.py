# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

from bpy.types import Header, Operator, Panel, UIList
from bpy.props import EnumProperty, IntProperty


def _addon_display_name(context, addon_id):
    """Curated display name for addon_id, falling back to the raw id.

    The fallback matters for an area hosting an add-on that was never added through
    the picker (e.g. addon_id set directly via Python), which has no curated
    bAddonEditor entry to read a name from.
    """
    for entry in context.preferences.addon_editors:
        if entry.module == addon_id:
            return entry.name or addon_id
    return addon_id


class ADDON_HT_header(Header):
    bl_space_type = 'ADDON'

    def draw(self, context):
        layout = self.layout

        # Always draw the editor type selector first, so an area hosting an add-on
        # can always be switched back to another editor, even when no add-on is set.
        layout.template_header()

        # Not context.space_data: while an add-on is hosted, that resolves to the editor
        # this area borrows context from, not to this SpaceAddon.
        space = context.area.spaces.active
        layout.separator_spacer()
        if space.addon_id:
            layout.label(text=_addon_display_name(context, space.addon_id))
        else:
            layout.label(text="No Add-on Selected")


# Blender's own internal script packages, not real add-ons - see scripts/startup/bl_*.
# Deliberately an exact set, not a "bl_" prefix check: extensions import as
# bl_ext.<repository>.<addon> (see addon_utils.py's _ext_base_pkg_idname), which shares
# the prefix but is real, installable add-on content that must not be excluded.
_INTERNAL_MODULES = {"bl_ui", "bl_operators", "bl_app_templates_system"}


def _addon_label(module_name):
    """Human-readable name for an add-on module, e.g. from bl_info["name"].

    Falls back to the raw module id. That id is a real name for a legacy add-on
    (e.g. "node_wrangler"), but for an extension it is the full import path
    (bl_ext.<repository>.<addon>) - a poor fallback, but bl_info should always be
    present for anything addon_utils can enumerate at all, so this only matters if
    lookup itself fails.
    """
    import addon_utils

    mod = sys.modules.get(module_name)
    info = addon_utils.module_bl_info(mod) if mod else None
    return info.get("name", module_name) if info else module_name


def _installed_addon_items(self, context):
    import addon_utils

    items = []
    for mod in addon_utils.modules():
        module_name = mod.__name__
        if module_name in _INTERNAL_MODULES:
            continue
        items.append((module_name, _addon_label(module_name), module_name))

    items.sort(key=lambda item: item[1].lower())
    return items


class ADDON_OT_pick_and_host(Operator):
    """Choose an installed add-on to add to the editor type menu"""
    bl_idname = "addon.pick_and_host"
    bl_label = "Add an Add-on"
    bl_options = {'REGISTER', 'UNDO'}
    # Names which property invoke_search_popup() searches over.
    bl_property = "addon_id"

    addon_id: EnumProperty(
        name="Add-on",
        items=_installed_addon_items,
    )

    def execute(self, context):
        editors = context.preferences.addon_editors
        if not any(entry.module == self.addon_id for entry in editors):
            entry = editors.new()
            entry.module = self.addon_id
            entry.name = _addon_label(self.addon_id)

        area = context.area
        if area is not None and area.type == 'ADDON':
            area.spaces.active.addon_id = self.addon_id

        return {'FINISHED'}

    def invoke(self, context, event):
        # Returns None, not a result set: call it, then report our own.
        context.window_manager.invoke_search_popup(self)
        return {'RUNNING_MODAL'}


class ADDON_OT_editor_remove(Operator):
    """Remove an add-on from the editor type menu"""
    bl_idname = "addon.editor_remove"
    bl_label = "Remove Add-on Editor"
    bl_options = {'REGISTER', 'UNDO'}

    index: IntProperty()

    def execute(self, context):
        editors = context.preferences.addon_editors
        if self.index < 0 or self.index >= len(editors):
            return {'CANCELLED'}
        editors.remove(editors[self.index])
        return {'FINISHED'}


class ADDON_UL_editors(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.label(text=item.name or item.module, icon='PLUGIN')


class USERPREF_PT_addon_editors(Panel):
    bl_label = "Add-on Editors"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "addons"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.label(
            text="Add-ons listed here appear as editor types, via Add an Add-on... "
                 "in any area's editor type menu.")

        row = layout.row()
        row.template_list(
            "ADDON_UL_editors", "", context.preferences, "addon_editors",
            context.preferences, "active_addon_editor_index", rows=3,
        )

        col = row.column(align=True)
        col.operator("addon.pick_and_host", text="", icon='ADD')
        props = col.operator("addon.editor_remove", text="", icon='REMOVE')
        props.index = context.preferences.active_addon_editor_index


classes = (
    ADDON_HT_header,
    ADDON_OT_pick_and_host,
    ADDON_OT_editor_remove,
    ADDON_UL_editors,
    USERPREF_PT_addon_editors,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
