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
#include "BLI_string_utf8.hh"

#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Space Callbacks
 * \{ */

static SpaceLink *addon_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceAddon *saddon = MEM_new<SpaceAddon>("initaddon");
  saddon->spacetype = SPACE_ADDON;

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
static void addon_free(SpaceLink * /*sl*/) {}

static void addon_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *addon_duplicate(SpaceLink *sl)
{
  /* `addon_id` is intentionally carried over, so a duplicated area keeps its add-on. */
  SpaceAddon *saddon_new = MEM_dupalloc(reinterpret_cast<SpaceAddon *>(sl));
  return reinterpret_cast<SpaceLink *>(saddon_new);
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
}

static void addon_main_region_draw(const bContext *C, ARegion *region)
{
  /* Placeholder: draws an empty panel region. The hosted add-on's panels are
   * collected and passed to #ED_region_panels_layout_ex in a later step. */
  ED_region_panels(C, region);
}

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
  st->blend_write = addon_blend_write;

  /* Regions: main window. */
  art = MEM_new_zeroed<ARegionType>("spacetype addon region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  art->init = addon_main_region_init;
  art->draw = addon_main_region_draw;
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
