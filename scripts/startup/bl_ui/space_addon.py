# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Header


class ADDON_HT_header(Header):
    bl_space_type = 'ADDON'

    def draw(self, context):
        layout = self.layout

        # Always draw the editor type selector first, so an area hosting an add-on
        # can always be switched back to another editor, even when no add-on is set.
        layout.template_header()

        space = context.space_data
        layout.separator_spacer()
        if space.addon_id:
            layout.label(text=space.addon_id)
        else:
            layout.label(text="No Add-on Selected")


classes = (
    ADDON_HT_header,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
