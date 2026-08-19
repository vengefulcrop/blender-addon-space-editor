/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaddon
 *
 * Hierarchical "Addons" tree for the Add-on Editor's left sidebar: one row per enabled
 * add-on that registers any panels at all, expanding to one child row per distinct
 * editor type (#eSpace_Type) its panels target. Selecting a child row hosts that
 * add-on/space-type combination in the same area, the same net effect as picking it
 * from the header's editor-type drop-down.
 *
 * Modeled directly on `space_file/asset_catalog_tree_view.cc`'s
 * `AssetCatalogTreeView`, trimmed to what this read-only tree needs: no drag/drop,
 * rename, or context menu, since - unlike asset catalogs - add-on/panel-set rows
 * aren't user-editable. See docs_ui/addon_space_editor_ux_redesign.md for why this is
 * a contained clone rather than a generic Python-facing tree API.
 */

#include <memory>
#include <string>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.hh"
#include "BLI_string_utf8.hh"
#include "BLI_utildefines.hh"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_enum_types.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "addon_intern.hh"

namespace blender {

namespace {

/** Name + icon for a plain #eSpace_Type value, straight from the same enum the header's
 * editor-type drop-down and #SpaceAddon::preferred_delegate_spacetype both use. */
void space_type_name_and_icon(short spacetype, const char **r_name, int *r_icon)
{
  for (const EnumPropertyItem *item = rna_enum_space_type_items; item->identifier; item++) {
    if (item->value == spacetype) {
      *r_name = item->name;
      *r_icon = item->icon;
      return;
    }
  }
  *r_name = "";
  *r_icon = ICON_NONE;
}

/** Human-readable name for an add-on module, and whether it is bundled with Blender.
 *
 * The module id is never shown directly when anything better is available: for an
 * extension it is the full `bl_ext.<repository>.<addon>` import path, which is not a
 * name (the same reason #AddonEditorEntry::label exists rather than using
 * #bAddonEditor::module - see `space_addon.cc`).
 *
 * Prefers the curated #bAddonEditor::name when the add-on has an entry - that is the
 * label the user already sees for it in the editor-type drop-down, so the two agree -
 * and otherwise asks Python for `bl_info["name"]`, exactly as the "Add an Add-on"
 * picker does via `_addon_label()` (`space_addon.py`). */
std::string addon_display_name(const char *module, bool *r_is_bundled)
{
  if (r_is_bundled != nullptr) {
    *r_is_bundled = false;
  }

  char label[128] = "";
#ifdef WITH_PYTHON
  BPY_addon_module_info_get(module, label, sizeof(label), r_is_bundled);
#endif

  for (const bAddonEditor &entry : U.addon_editors) {
    if (STREQ(entry.module, module) && entry.name[0] != '\0') {
      return entry.name;
    }
  }

  return (label[0] != '\0') ? label : module;
}

/** Applies the same area/space mutation as
 * `ADDON_OT_set_preferred_delegate_spacetype.execute()` (`space_addon.py`), from C++
 * since a tree row activation has no Python operator to call through. */
void addon_tree_activate(bContext &C, const char *module, short spacetype)
{
  ScrArea *area = CTX_wm_area(&C);
  if (area == nullptr || area->spacetype != SPACE_ADDON) {
    return;
  }
  SpaceAddon *saddon = static_cast<SpaceAddon *>(area->spacedata.first);
  STRNCPY_UTF8(saddon->addon_id, module);
  saddon->preferred_delegate_spacetype = spacetype;

  ED_area_tag_redraw(area);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_CHANGED, nullptr);
}

class AddonTreeView : public ui::AbstractTreeView {
 public:
  void build_tree() override;
};

/** One add-on worth listing, resolved once so the two ordering passes below need no
 * repeated Python lookups. */
struct AddonTreeEntry {
  std::string module;
  std::string display_name;
  Vector<short> space_types;
  bool is_bundled = false;
};

void AddonTreeView::build_tree()
{
  const bool show_bundled = (U.uiflag2 & USER_ADDON_EDITOR_SHOW_BUNDLED) != 0;

  Vector<AddonTreeEntry> entries;
  for (const bAddon &addon : U.addons) {
    AddonTreeEntry entry;
    entry.space_types = BKE_paneltypes_addon_space_types_get(addon.module);
    if (entry.space_types.is_empty()) {
      /* Enabled, but registers no panels at all - nothing to host, skip it. */
      continue;
    }

    entry.display_name = addon_display_name(addon.module, &entry.is_bundled);

    /* Same opt-in the picker used to apply: bundled add-ons have real panels, but ones
     * typically gated on scene state rather than on which editor is open, so they look
     * pickable and then draw nothing. */
    if (entry.is_bundled && !show_bundled) {
      continue;
    }

    entry.module = addon.module;
    entries.append(std::move(entry));
  }

  /* Bundled add-ons first, and marked with Blender's own icon rather than the generic
   * plug-in one - they are a different kind of thing from what the user installed, and
   * only appear here at all when the preference above is on, so grouping them keeps them
   * from being scattered through a list of the user's own add-ons. Order within each
   * group is left as-is (#UserDef::addons order); the view's own A-Z toggle sorts. */
  for (const bool bundled_pass : {true, false}) {
    for (const AddonTreeEntry &entry : entries) {
      if (entry.is_bundled != bundled_pass) {
        continue;
      }

      ui::BasicTreeViewItem &addon_item = add_tree_item<ui::BasicTreeViewItem>(
          entry.display_name, entry.is_bundled ? ICON_BLENDER : ICON_PLUGIN);

      /* Clicking the add-on itself hosts its first editor type, rather than only
       * expanding to reveal children that all have to be clicked separately. With one
       * declared editor type - the common case - that makes the parent row the whole
       * interaction. */
      const std::string module_copy = entry.module;
      const short first_spacetype = entry.space_types.first();
      addon_item.set_on_activate_fn(
          [module_copy, first_spacetype](bContext &C, ui::BasicTreeViewItem & /*item*/) {
            addon_tree_activate(C, module_copy.c_str(), first_spacetype);
          });

      for (const short spacetype : entry.space_types) {
        const char *name;
        int icon;
        space_type_name_and_icon(spacetype, &name, &icon);

        ui::BasicTreeViewItem &space_type_item = addon_item.add_tree_item<ui::BasicTreeViewItem>(
            name, icon);

        space_type_item.set_on_activate_fn(
            [module_copy, spacetype](bContext &C, ui::BasicTreeViewItem & /*item*/) {
              addon_tree_activate(C, module_copy.c_str(), spacetype);
            });
      }
    }
  }
}

static void addon_panel_tree_draw(const bContext *C, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  ui::Block *block = layout.block();

  ui::block_layout_set_current(block, &layout);

  ui::AbstractTreeView *tree_view = ui::block_add_view(
      *block, "addon panel tree view", std::make_unique<AddonTreeView>());

  /* Not cosmetic: this is what gives the view a bounded height at all. Everything the
   * scrollable/searchable list treatment consists of - the scroll bar, the drag-to-resize
   * grip, the search field, and the alphabetical sort toggle - is drawn only when the view
   * has a custom height, and this is the only public way to establish one (see
   * `tree_view.cc`, `if (tree_view.custom_height_)`). Without it the tree draws every row
   * of every installed add-on at full length, with no way to filter it. */
  tree_view->set_default_rows(8);

  ui::TreeViewBuilder::build_tree_view(*C, *tree_view, layout);
}

}  // namespace

void addon_tools_region_panels_register(ARegionType *art)
{
  PanelType *pt = MEM_new_zeroed<PanelType>("spacetype addon panels buttons");
  STRNCPY_UTF8(pt->idname, "ADDON_PT_addons_tree");
  STRNCPY_UTF8(pt->label, N_("Add-ons"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->draw = addon_panel_tree_draw;
  BLI_addtail(&art->paneltypes, pt);
}

}  // namespace blender
