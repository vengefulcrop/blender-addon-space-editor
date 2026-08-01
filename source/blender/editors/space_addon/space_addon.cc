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
 * Find this editor's own built-in fallback panel, registered normally from Python
 * (`ADDON_PT_empty_state` in `bl_ui/space_addon.py`, `bl_space_type = 'ADDON'`,
 * `bl_region_type = 'WINDOW'`). Nothing draws it natively - this editor's window region
 * always lays out #SpaceAddon_Runtime::paneltypes, never the region type's own native
 * list - so it is only ever reached via the explicit lookup in
 * #addon_panel_types_collect below.
 */
static const PanelType *addon_empty_state_paneltype_find()
{
  const SpaceType *st = BKE_spacetype_from_id(SPACE_ADDON);
  if (st == nullptr) {
    return nullptr;
  }
  for (const ARegionType &art : st->regiontypes) {
    if (art.regionid != RGN_TYPE_WINDOW) {
      continue;
    }
    for (const PanelType &pt : art.paneltypes) {
      if (STREQ(pt.idname, "ADDON_PT_empty_state")) {
        return &pt;
      }
    }
  }
  return nullptr;
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
 *
 * When nothing qualifies - no add-on chosen, the add-on has no matching panels, or none
 * of its panels' editors are open anywhere - the list is not left empty. Instead it gets
 * exactly one entry: this editor's own `ADDON_PT_empty_state` fallback panel, which
 * explains why in whichever of those ways is actually true. Determining *which* message
 * applies needs the same "what does this add-on's panel set need" question either way,
 * so that logic lives once, in Python, read by both the fallback panel's own `draw()`
 * and the header's info button - see `_addon_supported_spaces()` in `space_addon.py`.
 */
static void addon_panel_types_collect(const bContext *C,
                                      const char *addon_id,
                                      ListBaseT<PanelType> *r_paneltypes)
{
  BLI_freelistN(r_paneltypes);

  if (addon_id[0] != '\0') {
    const bScreen *screen = CTX_wm_screen(C);

    for (const std::unique_ptr<SpaceType> &st : BKE_spacetypes_list()) {
      /* Skip our own space type, so a nested add-on editor cannot recurse. */
      if (st->spaceid == SPACE_ADDON) {
        continue;
      }
      for (ARegionType &art : st->regiontypes) {
        /* N-panels live in RGN_TYPE_UI; Properties-tab-style panels (Texture Manager,
         * Cycles' render/material/light settings) live in RGN_TYPE_WINDOW. Both are
         * ordinary panel lists as far as ED_region_panels_layout_ex is concerned - it
         * does not care what region type a PanelType was originally registered under,
         * only that it is a top-level, non-instanced panel. Other region types (header,
         * tools, ...) are not panel lists and are excluded. */
        if (!ELEM(art.regionid, RGN_TYPE_UI, RGN_TYPE_WINDOW)) {
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
           * typically assumes context members that only that editor's own
           * #SpaceType.context callback provides (context.material, context.light,
           * ...), and calling poll() without them raises rather than failing quietly -
           * it is not written expecting to run outside its own editor. Checking here
           * means never calling into a context we already know cannot satisfy it. */
          if (!ELEM(pt.space_type, SPACE_EMPTY, SPACE_ADDON) && screen != nullptr &&
              BKE_screen_find_big_area(screen, pt.space_type, 0) == nullptr)
          {
            continue;
          }
          PanelType *pt_copy = MEM_dupalloc(&pt);
          pt_copy->next = pt_copy->prev = nullptr;
          BLI_addtail(r_paneltypes, pt_copy);
        }
      }
    }
  }

  if (BLI_listbase_is_empty(r_paneltypes)) {
    if (const PanelType *fallback = addon_empty_state_paneltype_find()) {
      PanelType *pt_copy = MEM_dupalloc(fallback);
      pt_copy->next = pt_copy->prev = nullptr;
      BLI_addtail(r_paneltypes, pt_copy);
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
    addon_panel_types_collect(C, saddon->addon_id, &saddon->runtime->paneltypes);
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

/** One curated dropdown entry: the module id used for matching, and the label to show. */
struct AddonEditorEntry {
  StringRefNull id;
  StringRefNull label;
};

/**
 * The add-ons the user has curated as editors, from #UserDef::addon_editors, sorted by
 * display name.
 *
 * This is deliberately a curated, persistent list rather than "every add-on that
 * currently has panels": that would make the menu grow and shrink as unrelated add-ons
 * are enabled or disabled, and would offer no way to remove an entry the user does not
 * want there. Sorting keeps the order stable for a given set of entries, which matters
 * because the sub-type value is an index into this list.
 *
 * #AddonEditorEntry::label is #bAddonEditor::name - the human-readable name captured
 * when the entry was added - falling back to the module id for entries added before
 * that field existed. Not #bAddonEditor::module directly: for extensions that id is the
 * full `bl_ext.<repository>.<addon>` import path, not something to show a user.
 *
 * Entries whose add-on is currently disabled are omitted entirely, rather than shown
 * disabled or greyed out: disabling an add-on unregisters its classes, so it would be
 * dropdown noise pointing at something that cannot draw anything right now anyway. This
 * does not depend on this editor's own polling/delegation logic at all - "is this
 * add-on enabled" and "does at least one currently-registered #PanelType have this
 * addon_id" are the same question, answered by the same scan
 * #addon_panel_types_collect already performs for the active add-on.
 */
static bool addon_has_registered_panels(const char *addon_id)
{
  for (const std::unique_ptr<SpaceType> &st : BKE_spacetypes_list()) {
    if (st->spaceid == SPACE_ADDON) {
      continue;
    }
    for (const ARegionType &art : st->regiontypes) {
      if (!ELEM(art.regionid, RGN_TYPE_UI, RGN_TYPE_WINDOW)) {
        continue;
      }
      for (const PanelType &pt : art.paneltypes) {
        if (pt.parent == nullptr && STREQ(pt.addon_id, addon_id)) {
          return true;
        }
      }
    }
  }
  return false;
}

static Vector<AddonEditorEntry> addon_ids_get()
{
  Vector<AddonEditorEntry> entries;
  for (const bAddonEditor &entry : U.addon_editors) {
    if (!addon_has_registered_panels(entry.module)) {
      continue;
    }
    entries.append({entry.module, entry.name[0] ? entry.name : entry.module});
  }
  std::sort(entries.begin(), entries.end(), [](const AddonEditorEntry &a, const AddonEditorEntry &b) {
    return BLI_strcasecmp(a.label.c_str(), b.label.c_str()) < 0;
  });
  return entries;
}

static int addon_space_subtype_get(ScrArea *area)
{
  const SpaceAddon *saddon = static_cast<const SpaceAddon *>(area->spacedata.first);
  const Vector<AddonEditorEntry> entries = addon_ids_get();

  for (const int i : entries.index_range()) {
    if (entries[i].id == saddon->addon_id) {
      return i;
    }
  }
  /* No match - either nothing has been chosen yet, or the hosted add-on's curated
   * entry was filtered out (disabled; see addon_ids_get()). Falling back to a real
   * index (0) here would be wrong twice over: it collides with the packed value of
   * the "Add-ons" heading item (RNA_ENUM_ITEM_HEADING sets value = 0), which has no
   * icon, so the area-type button would show blank; and if an add-on genuinely is
   * hosted but merely hidden from the list, index 0 would falsely present a
   * different, unrelated add-on as the current selection. The "Add an Add-on..."
   * entry has neither problem: it is reserved (ADDON_SUBTYPE_PICK, never a real
   * index) and has its own icon. */
  return ADDON_SUBTYPE_PICK;
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

  const Vector<AddonEditorEntry> entries = addon_ids_get();
  if (entries.index_range().contains(value)) {
    STRNCPY(saddon->addon_id, entries[value].id.c_str());
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

  const Vector<AddonEditorEntry> entries = addon_ids_get();
  for (const int i : entries.index_range()) {
    /* The identifier string is the module id (needed by rna_Area_ui_type_itemf to
     * round-trip through set/get); the label is the human-readable name. */
    const EnumPropertyItem entry = {
        i, entries[i].id.c_str(), ICON_PLUGIN, entries[i].label.c_str(), ""};
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
