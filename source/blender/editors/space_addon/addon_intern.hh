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
};

}  // namespace blender
