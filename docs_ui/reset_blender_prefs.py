# SPDX-License-Identifier: GPL-2.0-or-later
"""
Resets this fork's Blender preferences after a UserDef/DNA-shape change.

Why this is needed: iterating on UserDef's shape (adding a field, reusing a bit,
etc.) mid-session means the machine's real userpref.blend may have been saved by a
build with a different struct layout than the one currently running. Reading that
old-shaped data back can crash on startup (observed repeatedly as an access
violation inside addon_ids_get(), reading garbage from a stale bAddonEditor entry).
See docs_ui/addon_space_editor_plan.md, "Startup crash from a stale userpref.blend
mid-development", for the full writeup.

The fix is always the same: get Blender to fall back to freshly-built defaults
instead of reading the stale file. This script does that by moving the current
userpref.blend aside (never deleting it - your keymaps/theme/etc are still in
there if you ever want to recover something from it by hand).

Usage:
    python reset_blender_prefs.py            # back up and remove the active file
    python reset_blender_prefs.py --dry-run   # just show what it would do
"""

import argparse
import datetime
import shutil
import sys
from pathlib import Path


# Must match BLENDER_VERSION in source/blender/blenkernel/BKE_blender_version.h
# (503 -> "5.3"). Deliberately hardcoded, not a scan of every version folder under
# %APPDATA%: this machine also has real, unrelated Blender installs (4.0, 4.2, 4.4,
# 5.0, 5.1, 5.2, ...) whose preferences must never be touched by this script. Update
# this if the fork's version number changes (e.g. after a rebase onto a newer
# upstream release).
FORK_BLENDER_VERSION = "5.3"


def find_config_dirs():
    """This fork's own "<version>/config" directory, and only that one."""
    if sys.platform != "win32":
        print("This script currently only knows the Windows config path layout.")
        sys.exit(1)

    import os

    config_dir = (Path(os.environ["APPDATA"]) / "Blender Foundation" / "Blender" /
                  FORK_BLENDER_VERSION / "config")
    return [config_dir] if config_dir.is_dir() else []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Show what would happen without moving anything.")
    args = parser.parse_args()

    config_dirs = find_config_dirs()
    if not config_dirs:
        print("No Blender config directory found under %APPDATA%\\Blender Foundation\\Blender.")
        return

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    moved_any = False

    for config_dir in config_dirs:
        userpref = config_dir / "userpref.blend"
        if not userpref.is_file():
            continue

        backup = config_dir / f"userpref.blend.bak_{timestamp}"
        print(f"{'[dry-run] would move' if args.dry_run else 'Moving'}: {userpref}")
        print(f"                   -> {backup}")
        if not args.dry_run:
            shutil.move(str(userpref), str(backup))
        moved_any = True

    if not moved_any:
        print("No userpref.blend found - nothing to reset. "
              "Blender will already start with fresh defaults.")
    elif not args.dry_run:
        print("\nDone. Launch Blender normally; it will regenerate defaults from "
              "the current build. Old preferences are preserved as .bak_* files, "
              "not deleted, if you want to recover anything from them by hand.")


if __name__ == "__main__":
    main()
