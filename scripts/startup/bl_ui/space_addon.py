# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

import bpy
from bpy.types import Header, Operator, Panel
from bpy.props import StringProperty


def _addon_display_name(context, addon_id):
    """Display name for addon_id: the curated name if it has one, else its bl_info name.

    Never falls back to addon_id while anything better can be resolved. For an
    extension that id is the full bl_ext.<repository>.<addon> import path rather than
    a name, so showing it directly is always a regression - which is exactly what
    happened once the sidebar tree made it possible to host an add-on that was never
    added through the picker, and so has no curated bAddonEditor entry to read from.
    """
    for entry in context.preferences.addon_editors:
        if entry.module == addon_id:
            if entry.name:
                return entry.name
            break
    return _addon_label(addon_id)


def _registered_panel_classes():
    """Every registered Panel subclass, at any inheritance depth.

    Recursive because __subclasses__() returns direct subclasses only. An add-on that
    defines its own base panel and derives from it (class MY_PT_x(MyAddonPanel)) would
    otherwise be invisible here, while the C++ side - which scans registered PanelTypes
    and does not care how they were declared - hosts it perfectly well.

    Filtered on is_registered because the subclass tree also holds the add-on's *own*
    unregistered base classes, and those routinely carry a bl_space_type for their
    children to inherit. Counting them reports editors the add-on has no registered
    panel for. Observed: ucupaint's unregistered Y_PT_UDIM_Atlas_menu base makes the
    add-on look like it needs an Image Editor, so the empty-state panel would list one
    as required while one is already open and still nothing draws.
    """
    stack = list(bpy.types.Panel.__subclasses__())
    while stack:
        cls = stack.pop()
        stack.extend(cls.__subclasses__())
        if getattr(cls, "is_registered", False):
            yield cls


def _addon_top_level_panel_space_types(addon_id):
    """Distinct bl_space_type values declared by addon_id's top-level panels.

    Covers the same panels the C++ side collects (top-level, RGN_TYPE_UI or WINDOW).
    Shared by _addon_supported_spaces() and _addon_has_open_delegate() so the
    filtering rule exists once, not twice with the risk of drifting apart.

    Membership matches by module prefix rather than an exact match, since a panel
    defined in a submodule (foo.ui) still belongs to add-on "foo": mirrors the
    prefix-based attribution BPY_class_module_name_get uses on the C side.
    """
    seen = set()
    for cls in _registered_panel_classes():
        module = cls.__module__
        if module != addon_id and not module.startswith(addon_id + "."):
            continue
        if getattr(cls, "bl_parent_id", ""):
            continue  # Sub-panels are drawn by their parent; skip like the C side does.
        if getattr(cls, "bl_region_type", "") not in {'UI', 'WINDOW'}:
            continue
        space_type = getattr(cls, "bl_space_type", "")
        if space_type:
            seen.add(space_type)
    return seen


def _space_type_icon_name(space_type):
    """(name, icon) for an Area.type enum identifier, e.g. ('NODE_EDITOR') ->
    ("Node Editor", 'NODE_EDITOR'). Falls back to the raw identifier if it is not a
    real, user-facing editor type (shouldn't normally happen for a panel's own
    bl_space_type, but this is display code - fail soft, not with a KeyError).

    The single place both the header's delegate icon and the supported-editors list
    resolve an editor type to what the user actually sees, so the two cannot drift
    apart on which icon or name represents a given type.
    """
    item = bpy.types.Area.bl_rna.properties["type"].enum_items.get(space_type)
    if item is None:
        return space_type, 'NONE'
    return item.name, item.icon


def _addon_supported_spaces(addon_id):
    """(space_type, name, icon) triples for every editor type addon_id's top-level
    panels declare - space_type is the raw Area.type identifier (e.g. 'NODE_EDITOR'),
    the same one #SpaceAddon.preferred_delegate_spacetype's items use.

    Computed independently here in Python because every consumer of this list - the
    header's info button, the editor-type picker, and the empty-state panel below - is
    Python-drawn, and none of them need anything the C++ side doesn't already expose
    more simply than a new RNA collection would.
    """
    triples = [(st, *_space_type_icon_name(st)) for st in _addon_top_level_panel_space_types(addon_id)]
    return sorted(triples, key=lambda triple: triple[1].lower())


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

        # Which add-on is hosted, and which of its editor types, are both chosen from the
        # sidebar's Add-ons tree now. The header used to carry a second drop-down for the
        # editor type and an info button listing the supported ones; the tree shows every
        # add-on with its editor types as expandable rows, so both said what was already
        # on screen.
        layout.separator_spacer()
        if space.addon_id:
            layout.label(text=_addon_display_name(context, space.addon_id))
        else:
            layout.label(text="No Add-on Selected")


class ADDON_PT_empty_state(Panel):
    """Explains why the hosted area has nothing to show.

    Injected by addon_panel_types_collect() (space_addon.cc) whenever the real
    collected panel list is empty - never reached through normal panel drawing, since
    this editor's window region always lays out SpaceAddon_Runtime::paneltypes, not
    this region type's own native panel list.
    """
    bl_idname = "ADDON_PT_empty_state"
    bl_space_type = 'ADDON'
    bl_region_type = 'WINDOW'
    bl_label = "Add-on Info"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        space = context.area.spaces.active
        addon_id = space.addon_id

        if not addon_id:
            layout.label(text="Choose an add-on from the editor type menu", icon='INFO')
            return

        # An explicit, unsatisfied choice (addon_delegate_spacetype_find in space_addon.cc
        # honors it strictly rather than substituting a different editor type) gets its
        # own message naming that one editor specifically, rather than the generic list
        # below - the user picked one editor, so "here's what you need" should say which.
        preferred = space.preferred_delegate_spacetype
        if preferred != 'EMPTY' and preferred not in {area.type for area in context.screen.areas}:
            name, icon = _space_type_icon_name(preferred)
            col = layout.column(align=True)
            col.label(text="This panel requires the following to be open", icon='INFO')
            col.label(text="in the workspace:")
            col.separator()
            col.label(text=name, icon=icon)
            return

        triples = _addon_supported_spaces(addon_id)
        if not triples:
            layout.label(
                text=_addon_display_name(context, addon_id) + " has no panels to show here",
                icon='INFO')
            return

        col = layout.column(align=True)
        col.label(text="This add-on's panels require one of the following", icon='INFO')
        col.label(text="editor types to be present in the workspace:")
        col.separator()
        for _space_type, name, icon in triples:
            col.label(text=name, icon=icon)


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


class ADDON_OT_bookmark_toggle(Operator):
    """Pin or unpin an add-on panel-set in the Bookmarks sidebar panel"""
    bl_idname = "addon.bookmark_toggle"
    bl_label = "Toggle Bookmark"
    bl_options = {'INTERNAL'}

    module: StringProperty(name="Module", options={'HIDDEN'})
    spacetype: StringProperty(name="Editor Type", options={'HIDDEN'})

    def execute(self, context):
        bookmarks = context.preferences.addon_bookmarks
        for entry in bookmarks:
            if entry.module == self.module and entry.spacetype == self.spacetype:
                bookmarks.remove(entry)
                return {'FINISHED'}
        entry = bookmarks.new()
        entry.module = self.module
        entry.spacetype = self.spacetype
        return {'FINISHED'}


class ADDON_OT_bookmark_activate(Operator):
    """Host this bookmarked add-on panel-set in the current Add-on Editor"""
    bl_idname = "addon.bookmark_activate"
    bl_label = "Open Bookmark"
    bl_options = {'INTERNAL'}

    module: StringProperty(name="Module", options={'HIDDEN'})
    spacetype: StringProperty(name="Editor Type", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return context.area is not None and context.area.type == 'ADDON'

    def execute(self, context):
        space = context.area.spaces.active
        space.addon_id = self.module
        space.preferred_delegate_spacetype = self.spacetype
        return {'FINISHED'}


class ADDON_PT_bookmarks(Panel):
    """Pinned add-on panel-sets, so a specific editor type of a specific add-on can be
    reopened without re-picking it from the header each time"""
    bl_label = "Bookmarks"
    bl_idname = "ADDON_PT_bookmarks"
    bl_space_type = 'ADDON'
    bl_region_type = 'TOOLS'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        bookmarks = context.preferences.addon_bookmarks

        header = layout.row()
        header.label(text="Bookmarks", icon='BOOKMARKS')

        # A pin toggle for whatever the current area is actually showing right now -
        # not space.preferred_delegate_spacetype directly, since that can be 'EMPTY'
        # ("Auto"). context_delegate_spacetype is the one resolved delegate that
        # produced, the same distinction the header's own accessor comment (below)
        # exists to avoid getting backwards.
        area = context.area
        space = area.spaces.active if area is not None else None
        if space is not None and getattr(space, "addon_id", None) and area.context_delegate_spacetype != 'EMPTY':
            is_bookmarked = any(
                entry.module == space.addon_id and entry.spacetype == area.context_delegate_spacetype
                for entry in bookmarks
            )
            props = header.operator(
                "addon.bookmark_toggle", text="",
                icon='SOLO_ON' if is_bookmarked else 'SOLO_OFF', emboss=False,
            )
            props.module = space.addon_id
            props.spacetype = area.context_delegate_spacetype

        if not bookmarks:
            layout.label(text="No bookmarks yet", icon='INFO')
            return

        for entry in bookmarks:
            name, icon = _space_type_icon_name(entry.spacetype)
            display_name = _addon_display_name(context, entry.module)

            row = layout.row(align=True)
            props = row.operator(
                "addon.bookmark_activate", text=f"{display_name}: {name}", icon=icon)
            props.module = entry.module
            props.spacetype = entry.spacetype

            props = row.operator("addon.bookmark_toggle", text="", icon='X', emboss=False)
            props.module = entry.module
            props.spacetype = entry.spacetype


classes = (
    ADDON_HT_header,
    ADDON_PT_empty_state,
    ADDON_OT_bookmark_toggle,
    ADDON_OT_bookmark_activate,
    ADDON_PT_bookmarks,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
