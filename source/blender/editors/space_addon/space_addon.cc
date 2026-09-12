/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaddon
 *
 * Editor that hosts the panels of a single add-on or extension.
 *
 * The add-on is identified by the module name stored in #SpaceAddon::addon_id. This
 * file provides the space type, the panel collection, the region layout, and the
 * context delegation to the editor each hosted panel was written for.
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include <string>

#include "BLI_listbase.hh"
#include "BLI_set.hh"
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

#include "WM_api.hh"

#include "BLO_read_write.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

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

  /* Sidebar (Bookmarks + Addons tree). */
  region = BKE_area_region_new();
  BLI_addtail(&saddon->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;

  /* Main region. Must be added last: #region_rect_recursive carves the area up in
   * region-list order, and this region (alignment #RGN_ALIGN_NONE) takes whatever is
   * left over - anything appended after it would be left with no space at all. */
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
  if (area == nullptr || area->context_delegate_spacetype == SPACE_EMPTY) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  SpaceType *st = BKE_spacetype_from_id(area->context_delegate_spacetype);
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
 * Panels whose `poll()` raised while hosted here, by ID name.
 *
 * Hosting panels outside the editor they were written for means a raising `poll()` is an
 * expected condition, not an anomaly - the panel is reading something its own editor
 * would have provided. Blender prints the traceback and carries on, which is fine once
 * and catastrophic at redraw rate: on Windows the console applies backpressure and the
 * main thread blocks in `WriteConsoleW`, hanging Blender with no CPU use and no crash
 * log. So a panel that raises is recorded here and not polled again.
 *
 * Global rather than per-area because "this panel cannot survive being polled out of
 * context" is a property of the panel, not of the area showing it. Cleared whenever the
 * registered panel types change (see #BKE_paneltypes_state_get), so re-enabling or
 * reloading an add-on gives its panels a clean slate.
 */
static Set<std::string> &addon_poll_failed_get()
{
  static Set<std::string> failed;
  return failed;
}

/**
 * Call a Python panel's `poll()`, treating an exception as "do not show".
 *
 * Mirrors `panel_poll` in `rna_ui.cc`, with the one difference that matters here: the
 * return code of #StructRNA::ext `call` is checked instead of discarded, which is the
 * only way to tell that the callback raised rather than returned false.
 */
static bool addon_panel_poll_guarded(const bContext *C, PanelType *pt)
{
  if (addon_poll_failed_get().contains(pt->idname)) {
    return false;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, pt->rna_ext.srna, nullptr);
  FunctionRNA *func = RNA_struct_find_function(ptr.type, "poll");
  if (func == nullptr) {
    return true;
  }

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  const int err = pt->rna_ext.call(const_cast<bContext *>(C), &ptr, func, &list);

  bool visible = false;
  if (err == 0) {
    void *ret;
    RNA_parameter_get_lookup(&list, "visible", &ret);
    visible = *static_cast<bool *>(ret);
  }
  else {
    /* Already reported once, by the call above. Remember it so the next redraw does not
     * report it again. */
    addon_poll_failed_get().add(pt->idname);
  }

  RNA_parameter_list_free(&list);
  return visible;
}

/**
 * Detach any panel in \a panels bound to \a pt, so that freeing \a pt cannot leave a
 * dangling #Panel::type.
 *
 * A null type is not a broken state: it is exactly what a panel whose type went away
 * is left in, and what every panel starts as when read from a file (see
 * #direct_link_panel_list). The panel code checks for it throughout, and
 * #panel_begin re-binds the panel as soon as a type of the same ID name shows up again.
 *
 * Only top-level panels are considered, because only their types are copies owned by
 * this editor. Sub-panel #Panel::type points at the registered child type reached
 * through #PanelType::children, which this editor never owns or frees.
 */
static void addon_panels_type_detach(ListBaseT<Panel> *panels, const PanelType *pt)
{
  for (Panel &panel : *panels) {
    if (panel.type == pt) {
      panel.type = nullptr;
    }
  }
}

/** Unlink and return the entry of \a lb with the given ID name, or null. */
static PanelType *addon_paneltype_pop(ListBaseT<PanelType> *lb, const char *idname)
{
  for (PanelType &pt : *lb) {
    if (STREQ(pt.idname, idname)) {
      BLI_remlink(lb, &pt);
      return &pt;
    }
  }
  return nullptr;
}

/**
 * The add-on (top-level Python module) that registered \a pt, or an empty string for a
 * panel defined in C or one whose owning class cannot be determined.
 *
 * Deliberately not a field on #PanelType itself: computing it here, on demand, keeps
 * every other panel registration in Blender free of this editor's own bookkeeping - the
 * attribution only ever needs to be known while this editor is collecting panels.
 */
static void addon_panel_owner_get(const PanelType &pt, char *r_addon_id, size_t r_addon_id_maxncpy)
{
  r_addon_id[0] = '\0';
#ifdef WITH_PYTHON
  BPY_class_module_name_get(pt.rna_ext.data, r_addon_id, r_addon_id_maxncpy);
#else
  UNUSED_VARS(pt, r_addon_id_maxncpy);
#endif
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
 * A copy that is still wanted is *reused* rather than freed and reallocated, with its
 * contents refreshed from the registered type. This keeps #Panel::type valid across a
 * re-collection, which is what preserves the panels in \a region_panels - their
 * collapsed state, drag order and `layout.panel()` sub-section states. Rebuilding the
 * list from scratch instead would strand every panel on a freed type, and those panels
 * carry the layout the user arranged and the file restored.
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
                                      const short delegate_spacetype,
                                      ListBaseT<PanelType> *r_paneltypes,
                                      ListBaseT<Panel> *region_panels)
{
  /* Set aside rather than freed: entries still wanted are moved back across below, and
   * whatever is left over at the end is what genuinely went away. */
  ListBaseT<PanelType> previous = *r_paneltypes;
  *r_paneltypes = {nullptr, nullptr};

  /** Reuse the previous copy of \a pt if there is one, else make a new one. */
  auto paneltype_copy_get = [&](const PanelType &pt) {
    PanelType *pt_copy = addon_paneltype_pop(&previous, pt.idname);
    if (pt_copy == nullptr) {
      pt_copy = MEM_dupalloc(&pt);
    }
    else {
      /* Refresh: re-enabling or reloading an add-on registers a whole new #PanelType, so
       * a reused copy's callbacks and `children` list would otherwise be stale. */
      *pt_copy = pt;
    }
    pt_copy->next = pt_copy->prev = nullptr;
    /* Re-installed after every refresh, since the assignment above restores the
     * registered callback. Only for Python panels: a C panel's poll cannot raise. */
    if (pt_copy->poll != nullptr && pt_copy->rna_ext.call != nullptr) {
      pt_copy->poll = addon_panel_poll_guarded;
    }
    BLI_addtail(r_paneltypes, pt_copy);
  };

  if (addon_id[0] != '\0') {
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
          char pt_addon_id[128];
          addon_panel_owner_get(pt, pt_addon_id, sizeof(pt_addon_id));
          if (!STREQ(pt_addon_id, addon_id)) {
            continue;
          }
          /* Only panels the chosen delegate can actually satisfy.
           *
           * A panel's poll() typically assumes context members that only its own
           * editor's #SpaceType.context callback provides (context.material,
           * context.light, ...), and raises rather than failing quietly when they are
           * absent - it was not written expecting to run elsewhere. Since the whole area
           * borrows from a single editor (#ScrArea::context_delegate_spacetype), a panel
           * written for any other editor would be polled against the wrong space data.
           *
           * That is not hypothetical: ucupaint registers for both the Node Editor and
           * the 3D Viewport, and with both open its NODE_PT_YPaintUI was polled against
           * a SpaceView3D and raised on every redraw.
           *
           * So panels for other editors are dropped rather than shown and broken. When
           * the user can choose which of an add-on's editor slots an area hosts, this
           * filter becomes that choice instead of an automatic one. */
          if (!ELEM(pt.space_type, SPACE_EMPTY, SPACE_ADDON) &&
              pt.space_type != delegate_spacetype)
          {
            continue;
          }
          paneltype_copy_get(pt);
        }
      }
    }
  }

  if (BLI_listbase_is_empty(r_paneltypes)) {
    if (const PanelType *fallback = addon_empty_state_paneltype_find()) {
      paneltype_copy_get(*fallback);
    }
  }

  /* Left over: types that are no longer wanted, because the add-on changed, was disabled
   * or reloaded, or the editor its panels need was closed. Detach their panels before
   * freeing, so nothing is left pointing at freed memory. The panels themselves are kept
   * - the type may well come back, and #panel_begin re-binds them by ID name when it
   * does, restoring the layout rather than starting over. */
  for (const PanelType &pt : previous) {
    addon_panels_type_detach(region_panels, &pt);
  }
  BLI_freelistN(&previous);
}

/**
 * The editor type this area will borrow context from, or #SPACE_EMPTY for none.
 *
 * Most of what an add-on panel reads (`object`, `scene`, `mode`, selection, and so on)
 * is resolved at the screen level and works in any editor. What does not is
 * `space_data` and `region_data`: those come straight from the area
 * (see #CTX_wm_space_data), so a panel polling for `space.type == 'NODE_EDITOR'`, or
 * reading `space_data.overlay`, would fail here. Rather than let those panels vanish,
 * the area and region are temporarily swapped for a real editor of the type the panel
 * was written for. This is the same approach as operator context overrides.
 *
 * Resolved *before* collection rather than from its result, because the whole area can
 * only borrow from one editor: collection then keeps just the panels this delegate can
 * satisfy, instead of gathering panels for several editors and letting whichever lost
 * be polled against the wrong space data (see #addon_panel_types_collect).
 *
 * \a preferred_spacetype (#SpaceAddon::preferred_delegate_spacetype), when set, is
 * honored strictly: if an editor of that type is open it wins, and if not, this
 * returns #SPACE_EMPTY rather than substituting a different declared type - showing
 * the add-on's panels for an editor the user did not choose would be a silent,
 * surprising override of an explicit choice. The empty result still reaches the user:
 * collection ends up with nothing to show, and #ADDON_PT_empty_state explains
 * specifically which editor the current choice needs (see space_addon.py).
 *
 * Only when there is no explicit choice (#SPACE_EMPTY) does this fall back to the
 * first declared type with an editor open, by the order
 * #BKE_paneltypes_addon_space_types_get returns them - kept from before this parameter
 * existed, so an add-on with only one declared type behaves exactly as it always has.
 */
static short addon_delegate_spacetype_find(const bContext *C,
                                           const char *addon_id,
                                           const short preferred_spacetype)
{
  const bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return SPACE_EMPTY;
  }

  if (preferred_spacetype != SPACE_EMPTY) {
    return (BKE_screen_find_big_area(screen, preferred_spacetype, 0) != nullptr) ?
               preferred_spacetype :
               SPACE_EMPTY;
  }

  for (const short space_type : BKE_paneltypes_addon_space_types_get(addon_id)) {
    if (BKE_screen_find_big_area(screen, space_type, 0) != nullptr) {
      return space_type;
    }
  }

  return SPACE_EMPTY;
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
  ScrArea *area_orig = CTX_wm_area(C);
  SpaceAddon *saddon = static_cast<SpaceAddon *>(area_orig->spacedata.first);

  /* Resolved every layout - cheap, one scan over the add-on's own (usually few) declared
   * types - rather than only on cache miss below, so that changing
   * #SpaceAddon::preferred_delegate_spacetype (nothing else invalidates the cache for
   * that) is picked up on the very next redraw it triggers. */
  const short delegate_spacetype = addon_delegate_spacetype_find(
      C, saddon->addon_id, saddon->preferred_delegate_spacetype);

  /* Rebuild collection when the add-on changed, when panel types were registered or
   * removed (an add-on being enabled, disabled or reloaded), when the set of open editor
   * types changed (collection depends on which editors are available to delegate to, see
   * #addon_panel_types_collect), or when the resolved delegate itself changed (the user
   * picked a different preference, or the editor it needed just opened or closed). */
  const uint64_t paneltypes_state = BKE_paneltypes_state_get();
  const uint64_t screen_signature = addon_screen_signature_get(CTX_wm_screen(C));
  if (!STREQ(saddon->runtime->cached_addon_id, saddon->addon_id) ||
      saddon->runtime->cached_paneltypes_state != paneltypes_state ||
      saddon->runtime->cached_screen_signature != screen_signature ||
      area_orig->context_delegate_spacetype != delegate_spacetype)
  {
    if (saddon->runtime->cached_paneltypes_state != paneltypes_state) {
      /* An add-on was enabled, disabled or reloaded: give panels that previously raised
       * another chance, since the code behind them may well have changed. */
      addon_poll_failed_get().clear();
    }

    /* Recorded on the area itself - a generic per-area override, see
     * #ScrArea::context_delegate_spacetype - rather than on this space, so that
     * #blenkernel's context resolution needs no knowledge of this editor. */
    area_orig->context_delegate_spacetype = delegate_spacetype;

    addon_panel_types_collect(C,
                              saddon->addon_id,
                              area_orig->context_delegate_spacetype,
                              &saddon->runtime->paneltypes,
                              &region->panels);
    STRNCPY(saddon->runtime->cached_addon_id, saddon->addon_id);
    saddon->runtime->cached_paneltypes_state = paneltypes_state;
    saddon->runtime->cached_screen_signature = screen_signature;

    /* Note: `region->panels` is deliberately left alone. Collection above reuses the
     * panel type copies, so panels stay bound to live types and keep the state the user
     * arranged or the file restored. Panels whose type really did go away were detached
     * there, and are re-bound by ID name if it comes back. */
  }

  /* Borrow context from a real editor of the delegated type, so that panels polling on
   * the editor type or reading space data still draw.
   *
   * Handed to #ED_region_panels_layout_ex rather than swapped around the call, so that it
   * applies to the panels' own `poll`/`draw`/header callbacks and to nothing else. The
   * layout pass also aligns panels, updates region size and registers panel animation
   * handlers, and all of that has to act on *this* region: done under a borrowed one, a
   * panel whose height changed would start its re-align animation on the borrowed region
   * and never move here, leaving the panels below it overlapping until something else
   * forced a rebuild. Sub-panels are covered too, which a swap installed on this editor's
   * own #PanelType copies could not do - a copy's `children` are the registered child
   * types, shared with the editor the panels came from and not ours to modify.
   *
   * This editor's own #ADDON_PT_empty_state fallback needs no such care for the same
   * reason: it reads `context.area` expecting the real #SpaceAddon, and now nothing has
   * replaced it by the time any callback runs except the borrowing panels themselves.
   *
   * The delegate type is also recorded on the area (above), so that context lookups made
   * outside this layout pass - menus opened from a panel, operator polls when a button
   * is pressed - resolve the same way. Without that, a panel would draw but its buttons
   * would silently do nothing. */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area_delegate = (screen != nullptr &&
                            area_orig->context_delegate_spacetype != SPACE_EMPTY) ?
                               BKE_screen_find_big_area(
                                   screen, area_orig->context_delegate_spacetype, 0) :
                               nullptr;

  PanelDrawContextOverride ctx_override;
  if (area_delegate != nullptr) {
    ctx_override.area = area_delegate;
    ctx_override.region = BKE_area_find_region_type(area_delegate, RGN_TYPE_WINDOW);
  }

  ED_region_panels_layout_ex(C,
                             region,
                             &saddon->runtime->paneltypes,
                             wm::OpCallContext::InvokeRegionWin,
                             nullptr,
                             nullptr,
                             area_delegate ? &ctx_override : nullptr);

  /* Recorded here rather than asked again at draw time, because it is only answerable
   * right after a layout pass. See #addon_main_region_draw. */
  saddon->runtime->drew_nothing = ui::region_panels_drew_nothing(region);
}

/* Note: layout and drawing are deliberately separate callbacks. The region layout pass
 * runs before drawing and establishes the region size and View2D bounds, so doing the
 * layout from the draw callback would draw against stale metrics. */

/**
 * Draw the panels, and say so when they came to nothing.
 *
 * The #ADDON_PT_empty_state fallback only covers the case where no panel type was
 * collected at all. A hosted add-on can just as easily register panels that are all
 * filtered out by their own `poll()`, or that draw no content in the current state - a
 * UV tool with nothing selected, or one whose panel is registered for the Properties
 * editor while that editor sits on a different tab (see the note in the implementation
 * log). All three leave the region blank, and only the first explains itself.
 *
 * Drawn over the region rather than injected as another panel on purpose: a panel would
 * make the region non-empty, which would remove the panel again, which would make it
 * empty - a notice that flickers rather than one that shows.
 */
static void addon_main_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels_draw(C, region);

  const ScrArea *area = CTX_wm_area(C);
  const SpaceAddon *saddon = static_cast<const SpaceAddon *>(area->spacedata.first);
  if (!saddon->runtime->drew_nothing || saddon->addon_id[0] == '\0') {
    return;
  }

  /* Deliberately does not try to say *what* is missing. Which of the three causes above
   * applies is only answerable by interpreting an add-on's own `poll()`, which this fork
   * has declined to attempt everywhere else it has come up.
   *
   * #ED_region_info_draw is the same overlay other editors use to explain themselves
   * (the 3D viewport's "Clipped" / render-border notices), so this reads as a normal
   * Blender message rather than something this editor invented. */
  const char *lines[] = {
      IFACE_("This add-on's panels are drawing nothing right now"),
      IFACE_("They may need a specific selection, mode, or editor state"),
      IFACE_("- see the add-on's own documentation"),
      nullptr,
  };
  const float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};
  ED_region_info_draw_multiline(const_cast<ARegion *>(region), lines, fill_color, true);
}

/**
 * Redraw when anything a hosted panel might be reading has changed.
 *
 * Every other editor's listener is a tight allow-list, because an editor knows what it
 * displays. This one does not: it shows whatever panels an arbitrary add-on registered,
 * reading arbitrary data. Without this the region only ever redrew when something else
 * forced it - adding a light to the scene left a light-listing panel showing the old
 * list until the user collapsed and re-expanded it.
 *
 * Forwarding to the delegate editor's own listener was the obvious alternative and is
 * not safe: several listeners cast `params->area->spacedata` to their own space type
 * (see `space_clip.cc`, `space_action.cc`, `space_buttons.cc`), and this area holds a
 * #SpaceAddon. Substituting the delegate's area would fix the cast but silently give
 * those listeners a different area than the region they are tagging. Redrawing a little
 * too often is the cheaper mistake.
 *
 * Excluded are the categories that describe the interface rather than the data behind
 * it (#NC_WINDOW, #NC_SCREEN, #NC_WORKSPACE, #NC_WM). Panel layout state arrives through
 * #NC_SPACE, which is kept.
 */
static void addon_main_region_listener(const wmRegionListenerParams *params)
{
  switch (params->notifier->category) {
    case NC_WINDOW:
    case NC_SCREEN:
    case NC_WORKSPACE:
    case NC_WM:
      break;
    default:
      ED_region_tag_redraw(params->region);
      break;
  }
}

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
  st->context = addon_context;
  st->blend_read_data = addon_blend_read_data;
  st->blend_write = addon_blend_write;

  /* No #SpaceType::space_subtype_item_extend: this editor appears in the editor-type menu
   * as a single "Add-on" entry, like every other editor, and which add-on it hosts is
   * chosen from the sidebar's Add-ons tree instead. It used to add one sub-type entry per
   * curated add-on plus an "Add an Add-on..." picker; note that defining that callback is
   * also what suppresses a space type's own plain entry in #rna_Area_ui_type_itemf, so
   * leaving it unset is what puts "Add-on" in the menu normally. */

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

  /* Regions: sidebar (Bookmarks + Addons tree). */
  art = MEM_new_zeroed<ARegionType>("spacetype addon region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 240;
  art->keymapflag = ED_KEYMAP_UI;

  art->init = ED_region_panels_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;

  BLI_addhead(&st->regiontypes, art);
  addon_tools_region_panels_register(art);

  BKE_spacetype_register(std::move(st));
}

/** \} */

}  // namespace blender
