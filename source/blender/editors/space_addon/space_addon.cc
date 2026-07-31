/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaddon
 *
 * Editor that hosts the panels of a single add-on or extension.
 *
 * The add-on is identified by the module name stored in #SpaceAddon::addon_id. This
 * file provides the space type skeleton only; panel collection and drawing of the
 * hosted add-on's panels are added in a later step.
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "addon_intern.hh" /* own include */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Space Callbacks
 * \{ */

static SpaceLink *addon_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceAddon *saddon = MEM_new<SpaceAddon>("initaddon");
  saddon->spacetype = SPACE_ADDON;

  saddon->runtime = MEM_new<SpaceAddon_Runtime>(__func__);

  /* Header. */
  ARegion *region = BKE_area_region_new();
  BLI_addtail(&saddon->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* Main region. */
  region = BKE_area_region_new();
  BLI_addtail(&saddon->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return reinterpret_cast<SpaceLink *>(saddon);
}

/* Doesn't free the space-link itself. */
static void addon_free(SpaceLink *sl)
{
  SpaceAddon *saddon = reinterpret_cast<SpaceAddon *>(sl);

  if (saddon->runtime) {
    BLI_freelistN(&saddon->runtime->paneltypes);
    MEM_delete(saddon->runtime);
    saddon->runtime = nullptr;
  }
}

static void addon_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *addon_duplicate(SpaceLink *sl)
{
  /* `addon_id` is intentionally carried over, so a duplicated area keeps its add-on. */
  SpaceAddon *saddon_new = MEM_dupalloc(reinterpret_cast<SpaceAddon *>(sl));

  /* Runtime data is rebuilt on the first draw, it must not be shared with the original. */
  saddon_new->runtime = MEM_new<SpaceAddon_Runtime>(__func__);

  return reinterpret_cast<SpaceLink *>(saddon_new);
}

static void addon_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceAddon *saddon = reinterpret_cast<SpaceAddon *>(sl);
  /* `addon_id` is kept verbatim, so re-enabling a missing add-on restores the editor. */
  saddon->runtime = MEM_new<SpaceAddon_Runtime>(__func__);
}

static void addon_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  writer->write_struct_cast<SpaceAddon>(sl);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Region
 * \{ */

static void addon_main_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);

  /* Match the Properties editor, which is the other editor whose main region is a
   * panel list: mark overflow so panels wider than the region are indicated. */
  region->flag |= RGN_FLAG_INDICATE_OVERFLOW;
}

/**
 * Collect the top-level panel types belonging to \a addon_id, from every space and
 * region type in Blender.
 *
 * The returned list holds *copies* of the registered #PanelType structs. They cannot be
 * linked directly, because a #PanelType is already a member of its own region type's
 * list through its `next`/`prev` fields, and re-linking would corrupt that list.
 * Copying is safe: panels are matched to their type by ID name rather than by pointer
 * (see #panel_find_by_type), and sub-panels are reached through `children`, which still
 * refers to the registered types.
 */
static void addon_panel_types_collect(const char *addon_id, ListBaseT<PanelType> *r_paneltypes)
{
  BLI_freelistN(r_paneltypes);

  if (addon_id[0] == '\0') {
    return;
  }

  for (const std::unique_ptr<SpaceType> &st : BKE_spacetypes_list()) {
    /* Skip our own space type, so a nested add-on editor cannot recurse. */
    if (st->spaceid == SPACE_ADDON) {
      continue;
    }
    for (ARegionType &art : st->regiontypes) {
      /* Only the side-bar hosts add-on panels, matching where they appear natively. */
      if (art.regionid != RGN_TYPE_UI) {
        continue;
      }
      for (PanelType &pt : art.paneltypes) {
        /* Sub-panels are drawn by their parent. */
        if (pt.parent != nullptr) {
          continue;
        }
        if (!STREQ(pt.addon_id, addon_id)) {
          continue;
        }
        PanelType *pt_copy = MEM_dupalloc(&pt);
        pt_copy->next = pt_copy->prev = nullptr;
        BLI_addtail(r_paneltypes, pt_copy);
      }
    }
  }
}

static void addon_main_region_layout(const bContext *C, ARegion *region)
{
  SpaceAddon *saddon = CTX_wm_space_addon(C);

  /* Rebuild when the add-on changed, or when panel types were registered or removed
   * (an add-on being enabled, disabled or reloaded). */
  const uint64_t paneltypes_state = BKE_paneltypes_state_get();
  if (!STREQ(saddon->runtime->cached_addon_id, saddon->addon_id) ||
      saddon->runtime->cached_paneltypes_state != paneltypes_state)
  {
    addon_panel_types_collect(saddon->addon_id, &saddon->runtime->paneltypes);
    STRNCPY(saddon->runtime->cached_addon_id, saddon->addon_id);
    saddon->runtime->cached_paneltypes_state = paneltypes_state;

    /* The existing panels reference the copies that were just freed. Drop them; the
     * layout below recreates them from the new panel types. */
    BKE_area_region_panels_free(&region->panels);
  }

  ED_region_panels_layout_ex(C,
                             region,
                             &saddon->runtime->paneltypes,
                             wm::OpCallContext::InvokeRegionWin,
                             nullptr,
                             nullptr);
}

/* Note: layout and drawing are deliberately separate callbacks. The region layout pass
 * runs before drawing and establishes the region size and View2D bounds, so doing the
 * layout from the draw callback would draw against stale metrics. */

static void addon_main_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Region
 * \{ */

static void addon_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void addon_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_spacetype_addon()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_ADDON;
  STRNCPY_UTF8(st->name, "Addon");

  st->create = addon_create;
  st->free = addon_free;
  st->init = addon_init;
  st->duplicate = addon_duplicate;
  st->blend_read_data = addon_blend_read_data;
  st->blend_write = addon_blend_write;

  /* Regions: main window. */
  art = MEM_new_zeroed<ARegionType>("spacetype addon region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;

  art->init = addon_main_region_init;
  art->layout = addon_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = addon_main_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* Regions: header. */
  art = MEM_new_zeroed<ARegionType>("spacetype addon region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = addon_header_region_init;
  art->draw = addon_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

/** \} */

}  // namespace blender
