# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

import bpy
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


def _addon_has_open_delegate(context, addon_id):
    """Whether an editor type addon_id's panels need is currently open somewhere.

    Not a guarantee any specific panel will actually draw - an individual poll() can
    still fail for unrelated reasons (no active object, and so on) - but it is the
    same signal that decides whether delegation can find anything at all, and is the
    best one available from Python without exposing new state from the C++ side.
    """
    open_types = {area.type for area in context.screen.areas}
    for space_type in _addon_top_level_panel_space_types(addon_id):
        # Matches the C side's delegation skip-list (BKE_screen.hh): these panels
        # need no delegate, so they are always considered satisfied.
        if space_type in {'EMPTY', 'ADDON'} or space_type in open_types:
            return True
    return False


class ADDON_OT_supported_editors_info(Operator):
    """Which editor types the hosted add-on's panels are written for"""
    bl_idname = "addon.supported_editors_info"
    bl_label = "Supported Editors"
    bl_options = {'INTERNAL'}

    @classmethod
    def description(cls, context, properties):
        addon_id = context.area.spaces.active.addon_id
        names = [name for _space_type, name, _icon in _addon_supported_spaces(addon_id)]
        if not names:
            return "No editor-type information available"
        return "Panels shown here need one of: " + ", ".join(names)

    @classmethod
    def poll(cls, context):
        return context.area is not None and context.area.type == 'ADDON'

    def execute(self, context):
        # Exists for its tooltip; nothing to do on click.
        return {'CANCELLED'}


def _preferred_delegate_spacetype_items(self, context):
    """Items for ADDON_OT_set_preferred_delegate_spacetype.spacetype.

    Labels are prefixed with the add-on's own display name (e.g. "Lumos: UV/Image
    Editor") - purely a Python-side presentation choice: the identifiers themselves
    are exactly #SpaceAddon.preferred_delegate_spacetype's own (the plain
    #rna_enum_space_type_items set, same one Area.type uses), so setting this property
    always writes a value the real, C-defined property already accepts. "No
    preference" is stored as 'EMPTY' (#SPACE_EMPTY) - that identifier's own meaning on
    this property - shown to the user as "Auto" rather than "Empty".
    """
    space = context.area.spaces.active
    display_name = _addon_display_name(context, space.addon_id)

    items = [(
        'EMPTY',
        display_name + ": Auto",
        "Borrow context from whichever supported editor is open, preferring the first "
        "one found",
        'NONE',
        0,
    )]
    for i, (space_type, name, icon) in enumerate(_addon_supported_spaces(space.addon_id), start=1):
        items.append((space_type, f"{display_name}: {name}", "", icon, i))
    return items


class ADDON_OT_set_preferred_delegate_spacetype(Operator):
    """Choose which of this add-on's editor types to borrow context from"""
    bl_idname = "addon.set_preferred_delegate_spacetype"
    bl_label = "Editor Type"
    bl_options = {'INTERNAL'}

    spacetype: EnumProperty(
        name="Editor Type",
        items=_preferred_delegate_spacetype_items,
    )

    def execute(self, context):
        context.area.spaces.active.preferred_delegate_spacetype = self.spacetype
        return {'FINISHED'}


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

        # A second dropdown, the same pattern as the 3D Viewport's interaction-mode
        # selector next to its own editor-type button: only meaningful once there is an
        # actual choice to make. With 0 or 1 declared editor types "Auto" and the one
        # explicit choice (if any) are equivalent, so the dropdown would offer nothing.
        #
        # Drawn through our own operator rather than a direct layout.prop() on the real
        # property: the property's own item list (the plain rna_enum_space_type_items
        # set, same as Area.type) carries Blender's plain editor names ("UV/Image
        # Editor"), and prefixing them with the add-on's name ("Lumos: UV/Image
        # Editor") is easier to read here in the header - a presentation choice, so it
        # lives here in Python rather than in the C-side item list.
        if space.addon_id and len(_addon_top_level_panel_space_types(space.addon_id)) > 1:
            display_name = _addon_display_name(context, space.addon_id)
            current = space.preferred_delegate_spacetype
            if current == 'EMPTY':
                text, icon = display_name + ": Auto", 'NONE'
            else:
                name, icon = _space_type_icon_name(current)
                text = f"{display_name}: {name}"
            layout.operator_menu_enum(
                "addon.set_preferred_delegate_spacetype", "spacetype", text=text, icon=icon)

        # Only when something is actually drawing: an empty region already explains
        # itself via ADDON_PT_empty_state, which covers the same information.
        if space.addon_id and _addon_has_open_delegate(context, space.addon_id):
            layout.operator(
                "addon.supported_editors_info", text="", icon='INFO', emboss=False)

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
        addon_id = context.area.spaces.active.addon_id

        if not addon_id:
            layout.label(text="Choose an add-on from the editor type menu", icon='INFO')
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


def _addon_is_bundled(mod):
    """Whether mod ships with Blender itself (scripts/addons_core), rather than being
    separately installed by the user (Extensions, or a legacy user add-ons directory).

    Path-based, not a poll()-outcome guess: bundled add-ons (Cycles, Pose Library, the
    format importers/exporters, ...) are real add-ons with real panels, but those
    panels are typically gated on scene state that has nothing to do with which editor
    is open (active render engine, pose mode, imported asset data, ...) - Cycles' own
    panels require context.scene.render.engine == 'CYCLES', for instance. Predicting
    whether such a poll() will pass is exactly the kind of thing this fork has
    deliberately stayed out of elsewhere (see the design notes on dynamic context
    routing); this sidesteps that by filtering on a simple, deterministic fact instead.
    """
    file = getattr(mod, "__file__", None)
    return file is not None and "addons_core" in file


def _installed_addon_items(self, context):
    import addon_utils

    show_bundled = context.preferences.show_addon_editor_bundled

    items = []
    for mod in addon_utils.modules():
        module_name = mod.__name__
        if module_name in _INTERNAL_MODULES:
            continue
        # Disabled add-ons register no panels, so picking one here would add a
        # curated entry that the editor-type dropdown then hides anyway (it only
        # shows entries with at least one currently-registered panel) - offering it
        # here would be a dead end with no feedback. Matches addon_utils.py's own
        # (loaded_default, loaded_state) naming; loaded_state is "is it enabled now".
        _loaded_default, loaded_state = addon_utils.check(module_name)
        if not loaded_state:
            continue
        if not show_bundled and _addon_is_bundled(mod):
            continue
        # Many bundled add-ons (importers/exporters, the extensions platform UI
        # itself) register only operators and menu entries, no panel at all - hosting
        # one would always land on the empty-state block, never anything real.
        # Reuses the exact filter addon_panel_types_collect (space_addon.cc) and the
        # empty-state message use, so "would this ever draw something" agrees
        # everywhere rather than being decided three different ways.
        if not _addon_top_level_panel_space_types(module_name):
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
            text="Add-ons listed here appear as editor types, via \"Add an Add-on...\" "
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

        layout.prop(context.preferences, "show_addon_editor_bundled")


classes = (
    ADDON_OT_supported_editors_info,
    ADDON_OT_set_preferred_delegate_spacetype,
    ADDON_HT_header,
    ADDON_PT_empty_state,
    ADDON_OT_pick_and_host,
    ADDON_OT_editor_remove,
    ADDON_UL_editors,
    USERPREF_PT_addon_editors,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
