/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaddon
 */

#pragma once

#include "BKE_screen.hh"

namespace blender {

struct SpaceAddon_Runtime {
  /**
   * Copies of the hosted add-on's top-level panel types, in the order they will be drawn.
   *
   * These are copies rather than the registered types themselves, because a #PanelType is
   * already linked into its own region type's list and cannot be a member of two lists.
   */
  ListBaseT<PanelType> paneltypes = {nullptr, nullptr};

  /** Add-on the cached #paneltypes were collected for, to detect a change of add-on. */
  char cached_addon_id[128] = {};
  /** Value of #BKE_paneltypes_state_get when #paneltypes was built. */
  uint64_t cached_paneltypes_state = 0;
  /**
   * Value of #addon_screen_signature_get when #paneltypes was built.
   *
   * Collection skips panels whose editor type is not open anywhere in the screen (see
   * #addon_panel_types_collect), so the result also depends on the screen's layout, not
   * only on the add-on and the set of registered panel types. Opening or closing an
   * editor elsewhere in the screen must therefore also invalidate the cache.
   */
  uint64_t cached_screen_signature = 0;

  /**
   * Whether the last layout pass produced nothing visible, so the draw pass can say so.
   * Runtime only: it describes one pass, not anything worth storing in a file.
   */
  bool drew_nothing = false;
};

/** Registers the "Addons" hierarchical tree panel onto the sidebar region type
 * (`addon_tree_view.cc`). */
void addon_tools_region_panels_register(ARegionType *art);

}  // namespace blender
