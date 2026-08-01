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

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "BLF_api.hh"

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "UI_interface_c.hh"
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

/**
 * Forward context member lookups to the borrowed editor.
 *
 * Editors supply their own context members through this callback: the Node Editor
 * provides `selected_nodes`, the Sequencer provides `selected_strips`, and so on. A
 * hosted panel or its operators may poll on those, so the query is passed to the
 * delegated editor's callback.
 *
 * That callback resolves its own space through #CTX_wm_space_node and friends, which
 * already delegate, so it operates on the borrowed editor's data.
 */
static int /*eContextResult*/ addon_context(const bContext *C,
                                            const char *member,
                                            bContextDataResult *result)
{
  ScrArea *area = CTX_wm_area(C);
  const SpaceAddon *saddon = area ? static_cast<const SpaceAddon *>(area->spacedata.first) :
                                    nullptr;
  if (saddon == nullptr || saddon->delegate_spacetype == SPACE_EMPTY) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  SpaceType *st = BKE_spacetype_from_id(saddon->delegate_spacetype);
  if (st == nullptr || st->context == nullptr) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  return st->context(C, member, result);
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
/**
 * \param r_missing_spacetype: Set to a space type that at least one of this add-on's
 * panels required but was not open anywhere, when the collected list ends up empty for
 * that reason. Left at #SPACE_EMPTY otherwise. Used to explain an empty editor instead
 * of just leaving it blank.
 */
static void addon_panel_types_collect(const bContext *C,
                                      const char *addon_id,
                                      ListBaseT<PanelType> *r_paneltypes,
                                      short *r_missing_spacetype)
{
  BLI_freelistN(r_paneltypes);
  *r_missing_spacetype = SPACE_EMPTY;

  if (addon_id[0] == '\0') {
    return;
  }

  const bScreen *screen = CTX_wm_screen(C);

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
        /* Skip panels written for an editor that is not open anywhere. Their poll()
         * typically assumes context members that only that editor's own #SpaceType.context
         * callback provides (context.material, context.light, ...), and calling poll()
         * without them raises rather than failing quietly - it is not written expecting
         * to run outside its own editor. Checking here means never calling into a context
         * we already know cannot satisfy it. */
        if (!ELEM(pt.space_type, SPACE_EMPTY, SPACE_ADDON) && screen != nullptr &&
            BKE_screen_find_big_area(screen, pt.space_type, 0) == nullptr)
        {
          *r_missing_spacetype = pt.space_type;
          continue;
        }
        PanelType *pt_copy = MEM_dupalloc(&pt);
        pt_copy->next = pt_copy->prev = nullptr;
        BLI_addtail(r_paneltypes, pt_copy);
        /* A panel was included after all, so the list is not empty because of a
         * missing editor. */
        *r_missing_spacetype = SPACE_EMPTY;
      }
    }
  }
}

/**
 * Find an editor to borrow context from while the hosted panels are laid out.
 *
 * Most of what an add-on panel reads (`object`, `scene`, `mode`, selection, and so on)
 * is resolved at the screen level and works in any editor. What does not is
 * `space_data` and `region_data`: those come straight from the area
 * (see #CTX_wm_space_data), so a panel polling for `space.type == 'NODE_EDITOR'`, or
 * reading `space_data.overlay`, would fail here.
 *
 * Rather than let those panels silently vanish, the area and region are temporarily
 * swapped for a real editor of the type the panel was written for, for the duration of
 * the layout. This is the same approach as operator context overrides.
 *
 * Returns null when no suitable editor is open, in which case such panels still poll
 * `false` and are skipped, exactly as before.
 */
static ScrArea *addon_context_delegate_find(const bContext *C, const ListBaseT<PanelType> &pts)
{
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return nullptr;
  }

  /* Panels declare the editor they were written for, so borrow that one. */
  for (const PanelType &pt : pts) {
    if (ELEM(pt.space_type, SPACE_EMPTY, SPACE_ADDON)) {
      continue;
    }
    if (ScrArea *area = BKE_screen_find_big_area(screen, pt.space_type, 0)) {
      return area;
    }
  }

  return nullptr;
}

/**
 * Cheap signature of the screen's editor layout: which space types are currently open.
 *
 * Not a precise change counter, just good enough to catch "an editor of a type that
 * matters was opened or closed somewhere" without an expensive per-panel diff.
 */
static uint64_t addon_screen_signature_get(const bScreen *screen)
{
  uint64_t signature = 0;
  if (screen == nullptr) {
    return signature;
  }
  for (const ScrArea &area : screen->areabase) {
    signature |= uint64_t(1) << (area.spacetype % 64);
  }
  return signature;
}

static void addon_main_region_layout(const bContext *C, ARegion *region)
{
  SpaceAddon *saddon = CTX_wm_space_addon(C);

  /* Rebuild when the add-on changed, when panel types were registered or removed (an
   * add-on being enabled, disabled or reloaded), or when the set of open editor types
   * changed (collection depends on which editors are available to delegate to, see
   * #addon_panel_types_collect). */
  const uint64_t paneltypes_state = BKE_paneltypes_state_get();
  const uint64_t screen_signature = addon_screen_signature_get(CTX_wm_screen(C));
  if (!STREQ(saddon->runtime->cached_addon_id, saddon->addon_id) ||
      saddon->runtime->cached_paneltypes_state != paneltypes_state ||
      saddon->runtime->cached_screen_signature != screen_signature)
  {
    addon_panel_types_collect(
        C, saddon->addon_id, &saddon->runtime->paneltypes, &saddon->runtime->missing_spacetype);
    STRNCPY(saddon->runtime->cached_addon_id, saddon->addon_id);
    saddon->runtime->cached_paneltypes_state = paneltypes_state;
    saddon->runtime->cached_screen_signature = screen_signature;

    /* The existing panels reference the copies that were just freed. Drop them; the
     * layout below recreates them from the new panel types. */
    BKE_area_region_panels_free(&region->panels);
  }

  /* Borrow context from a real editor of the type these panels expect, so that panels
   * polling on the editor type or reading space data still draw. Restored below.
   *
   * The delegated type is also recorded on the space, so that context lookups made
   * outside this layout pass - menus opened from a panel, operator polls when a button
   * is pressed - resolve the same way. Without that, a panel would draw but its buttons
   * would silently do nothing. */
  bContext *C_mutable = const_cast<bContext *>(C);
  ScrArea *area_orig = CTX_wm_area(C);
  ARegion *region_orig = CTX_wm_region(C);
  ScrArea *area_delegate = addon_context_delegate_find(C, saddon->runtime->paneltypes);

  saddon->delegate_spacetype = area_delegate ? area_delegate->spacetype : SPACE_EMPTY;

  if (area_delegate != nullptr) {
    CTX_wm_area_set(C_mutable, area_delegate);
    CTX_wm_region_set(C_mutable, BKE_area_find_region_type(area_delegate, RGN_TYPE_WINDOW));
  }

  ED_region_panels_layout_ex(C,
                             region,
                             &saddon->runtime->paneltypes,
                             wm::OpCallContext::InvokeRegionWin,
                             nullptr,
                             nullptr);

  if (area_delegate != nullptr) {
    CTX_wm_area_set(C_mutable, area_orig);
    CTX_wm_region_set(C_mutable, region_orig);
  }
}

/* Note: layout and drawing are deliberately separate callbacks. The region layout pass
 * runs before drawing and establishes the region size and View2D bounds, so doing the
 * layout from the draw callback would draw against stale metrics. */

static void addon_main_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels_draw(C, region);

  const SpaceAddon *saddon = CTX_wm_space_addon(C);
  if (saddon == nullptr || !BLI_listbase_is_empty(&saddon->runtime->paneltypes)) {
    return;
  }

  /* Explain why the editor is empty, rather than leaving it blank with no indication of
   * whether that is expected. The three cases a user can actually act on: no add-on
   * chosen yet, the add-on has no sidebar panels at all, or its panels need an editor
   * that is not currently open. */
  std::string message;
  if (saddon->addon_id[0] == '\0') {
    message = TIP_("Choose an add-on from the editor type menu");
  }
  else if (saddon->runtime->missing_spacetype != SPACE_EMPTY) {
    const SpaceType *needed = BKE_spacetype_from_id(saddon->runtime->missing_spacetype);
    message = fmt::format(fmt::runtime(TIP_("Open a {} to see this add-on's panels")),
                          needed ? needed->name : TIP_("compatible editor"));
  }
  else {
    message = fmt::format(fmt::runtime(TIP_("{} has no panels to show here")), saddon->addon_id);
  }

  /* Left-aligned rather than centered: measuring text width to center it requires
   * matching the font size #fontstyle_draw_simple sets internally, which is more
   * coupling than this message is worth. */
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  uchar text_color[4];
  ui::theme::get_color_4ubv(TH_TEXT, text_color);
  const float margin = UI_UNIT_X * 0.5f;
  ui::fontstyle_draw_simple(
      fstyle, margin, region->winy - UI_UNIT_Y, message.c_str(), text_color);
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
/** \name Editor Type Drop-down
 * \{ */

/**
 * Reserved sub-type index for the "Add an Add-on..." entry, never a real list index.
 *
 * Not -1: #ScrArea::butspacetype_subtype already reserves -1 to mean "not yet
 * determined, call space_subtype_get()" (see area.cc), so a -1 sub-type value would
 * never reach #addon_space_subtype_set at all. The value must also survive being
 * packed as `(space_type << 16) | subtype` and later unpacked via `value >> 16` /
 * `value & 0xffff` (see #rna_Area_ui_type_set) - the maximum value a `short` sub-type
 * field can hold does that safely.
 */
#define ADDON_SUBTYPE_PICK 0x7FFF

/**
 * The add-ons the user has curated as editors, from #UserDef::addon_editors, sorted by
 * name.
 *
 * This is deliberately a curated, persistent list rather than "every add-on that
 * currently has panels": that would make the menu grow and shrink as unrelated add-ons
 * are enabled or disabled, and would offer no way to remove an entry the user does not
 * want there. Sorting keeps the order stable for a given set of entries, which matters
 * because the sub-type value is an index into this list.
 */
static Vector<StringRefNull> addon_ids_get()
{
  Vector<StringRefNull> ids;
  for (const bAddonEditor &entry : U.addon_editors) {
    ids.append(entry.module);
  }
  std::sort(ids.begin(), ids.end(), [](StringRefNull a, StringRefNull b) {
    return BLI_strcasecmp(a.c_str(), b.c_str()) < 0;
  });
  return ids;
}

static int addon_space_subtype_get(ScrArea *area)
{
  const SpaceAddon *saddon = static_cast<const SpaceAddon *>(area->spacedata.first);
  const Vector<StringRefNull> addon_ids = addon_ids_get();

  for (const int i : addon_ids.index_range()) {
    if (addon_ids[i] == saddon->addon_id) {
      return i;
    }
  }
  return 0;
}

static void addon_space_subtype_set(ScrArea *area, int value)
{
  SpaceAddon *saddon = static_cast<SpaceAddon *>(area->spacedata.first);

  if (value == ADDON_SUBTYPE_PICK) {
    /* Only leave the marker; #rna_Area_ui_type_update reads it and invokes the picker
     * operator, since it has the #bContext this `set` callback does not. */
    saddon->addon_id[0] = SPACE_ADDON_ID_PICK_MARKER;
    saddon->addon_id[1] = '\0';
    return;
  }

  const Vector<StringRefNull> addon_ids = addon_ids_get();
  if (addon_ids.index_range().contains(value)) {
    STRNCPY(saddon->addon_id, addon_ids[value].c_str());
  }
  else {
    saddon->addon_id[0] = '\0';
  }
}

static void addon_space_subtype_item_extend(bContext * /*C*/,
                                            EnumPropertyItem **item,
                                            int *totitem)
{
  const EnumPropertyItem heading = RNA_ENUM_ITEM_HEADING(N_("Add-ons"), nullptr);
  RNA_enum_item_add(item, totitem, &heading);

  const EnumPropertyItem pick = {ADDON_SUBTYPE_PICK,
                                 "ADDON_PICK",
                                 ICON_ADD,
                                 N_("Add an Add-on..."),
                                 N_("Choose an installed add-on to add to this menu")};
  RNA_enum_item_add(item, totitem, &pick);

  const Vector<StringRefNull> addon_ids = addon_ids_get();
  for (const int i : addon_ids.index_range()) {
    const EnumPropertyItem entry = {
        i, addon_ids[i].c_str(), ICON_PLUGIN, addon_ids[i].c_str(), ""};
    RNA_enum_item_add(item, totitem, &entry);
  }
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
  st->context = addon_context;
  st->blend_read_data = addon_blend_read_data;
  st->blend_write = addon_blend_write;

  /* One entry per add-on in the editor type drop-down, as the Node Editor does for its
   * tree types, rather than a space type per add-on. */
  st->space_subtype_get = addon_space_subtype_get;
  st->space_subtype_set = addon_space_subtype_set;
  st->space_subtype_item_extend = addon_space_subtype_item_extend;

  /* Regions: main window. */
  art = MEM_new_zeroed<ARegionType>("spacetype addon region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;

  art->init = addon_main_region_init;
  art->layout = addon_main_region_layout;
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
