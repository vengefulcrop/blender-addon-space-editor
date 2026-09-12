---
type: architecture
title: "Upstream Fragility"
description: "The two changes in this fork that break silently when upstream moves, and what to check on every rebase"
tags: [architecture, upstream, rebase, dna, versioning]
last_updated: 2026-09-12
---

# Upstream Fragility

This fork is a rebasable commit series. See
[Fork Mergeability](./fork_mergeability.md) for the full file-level risk
table.

Two changes differ from the rest. Every other touch fails loudly: the
compiler stops, or git reports a conflict. These two can pass a build and a
rebase, and stay wrong.

Check both on every rebase onto a newer `main`. See
[Upstream Base and Version](../reference/upstream_base.md) for the base
commit, the current numbers, and the commands that read the upstream ones.

## 1. `SPACE_TYPE_NUM` claims enum slot 25

**What the fork does.** `makesdna/DNA_space_enums.h` adds `SPACE_ADDON = 25`
and rebases `SPACE_TYPE_NUM` onto it:

```c
#define SPACE_TYPE_NUM (SPACE_ADDON + 1)   /* was (SPACE_PROJECT + 1) */
```

**Why the number matters.** `SPACE_TYPE_NUM` sizes fixed arrays in
`wm_dragdrop.cc` and `wm_toolsystem.cc`. The value also goes into a `.blend`
as `ScrArea::spacetype`.

**What goes wrong.** Upstream adds its own space type and also takes 25. Git reports no conflict, because the two sides edit different
lines of the same enum. The build succeeds. Then:

- A fork-saved `.blend` reports space type 25. A newer build reads 25 as the
  upstream space type. The area changes into the wrong editor.
- Both enumerators exist with the same value. A `switch` on the space type
  gets a duplicate case, or it selects the wrong branch with no warning.

**What to check on a rebase.** Read `DNA_space_enums.h` after the rebase.
Confirm that `SPACE_ADDON` holds a value no other enumerator holds. Renumber
`SPACE_ADDON` to the next free slot if upstream took 25. Confirm that
`SPACE_TYPE_NUM` is still the highest value plus one.

**After a renumber, an old `.blend` is stale.** A file saved with
`SPACE_ADDON = 25` still says 25. The generic fallback in
`area.cc` turns the unknown area into a 3D Viewport. See
[Persistence and Compatibility](./persistence_and_compatibility.md).

## 2. `BLENDER_FILE_SUBVERSION` is bumped to 12

**What the fork does.** `BKE_blender_version.h` raises
`BLENDER_FILE_SUBVERSION` from 10 to 12. The fork takes two subversion
numbers, 11 and 12. `versioning_530.cc` holds one block:

```c
if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 12)) { ... }
```

**A fork file still opens in stock Blender.** The "saved by a newer version"
refusal in `readfile.cc` tests `minversion` and `minsubversion`, not the
subversion. The fork does not touch `BLENDER_FILE_MIN_VERSION` (405) or
`BLENDER_FILE_MIN_SUBVERSION` (85). So the refusal never fires.

**What goes wrong.** Upstream raises its own subversion to 11 and to
12, for its own changes. Then two different meanings share one number:

- Git reports a conflict on the `#define` line. That one is easy.
- Git reports **no** conflict between the two `versioning_530.cc` blocks,
  because they sit at different places in the file. Both then run against
  the same subversion number.
- A `.blend` saved by this fork claims 503.12. A stock build at 503.12 skips
  its own 11 and 12 versioning, because the file already claims to hold it.
  The upstream data change never runs. Nothing reports an error.

**What to check on a rebase.** Compare `BLENDER_FILE_SUBVERSION` in the
upstream base against the fork value. Move the fork versioning block to a
subversion above the upstream number. Renumber the guard in
`versioning_530.cc` to match.

**The rule for a shared build.** Do not open a fork-saved `.blend` in a
stock Blender build of the same version, and do not resave it there. See
[Persistence and Compatibility](./persistence_and_compatibility.md) for the
loss on that round trip.

## Summary

| # | Change | File | Fails loudly | Check on rebase |
|:-:|:---|:---|:---|:---|
| 1 | `SPACE_ADDON = 25`, `SPACE_TYPE_NUM` rebased | `DNA_space_enums.h` | No | Is 25 still free |
| 2 | `BLENDER_FILE_SUBVERSION` 10 to 12 | `BKE_blender_version.h`, `versioning_530.cc` | Partly | Is the fork subversion above the upstream one |

## Related

- [Fork Mergeability](./fork_mergeability.md)
- [Persistence and Compatibility](./persistence_and_compatibility.md)
- [Add-on Space Type](./addon_space_type.md)
