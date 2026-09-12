---
type: reference
title: "Upstream Base and Version"
description: "The Blender version this fork targets, the upstream commit it sits on, and how to check the gap"
tags: [reference, upstream, version, rebase]
last_updated: 2026-09-12
---

# Upstream Base and Version

## The numbers

| Field | Value | Source |
|---|---|---|
| Blender version | 5.3.0 alpha | `BKE_blender_version.h`, `BLENDER_VERSION 503` |
| Version cycle | `alpha` | `BLENDER_VERSION_CYCLE` |
| Version suffix | empty | `BLENDER_VERSION_SUFFIX` |
| File subversion | 12 | `BLENDER_FILE_SUBVERSION`, raised from 10 by this fork |
| Minimum readable file | 4.5, subversion 85 | `BLENDER_FILE_MIN_VERSION`, `BLENDER_FILE_MIN_SUBVERSION`, untouched |

## The upstream base

| Field | Value |
|---|---|
| Base commit | `027ef661892c1234de0eb8d44bf5bb189eb39d81` |
| Base date | 2026-07-31 |
| Base subject | "Fix (unreported) readfile missing to create missing IDs placeholders embedded IDs in the correct library." |
| Branch | `pyareas/addon-space-editor` |
| Branch commits | 43, on top of the base |
| Last branch commit | 2026-09-12 |

The branch never commits to `main`. Every change sits on top of the base.

## The gap, as of 2026-09-12

**This clone has not fetched upstream since 2026-07-31.** The local
`origin/main` points at the same commit as the base. Upstream Blender
publishes daily builds, so about six weeks of upstream work is not in this
clone.

The branch is therefore untested against current upstream. The two
caveats in [Upstream Fragility](../architecture/upstream_fragility.md)
get worse as the gap grows.

## How to measure the gap yourself

Run these in order. They report the current state, not the state written
above.

1. Fetch upstream:
   ```
   git fetch origin
   ```
2. Count the upstream commits this branch does not hold:
   ```
   git log --oneline 027ef661892..origin/main | wc -l
   ```
3. Read the upstream version, to see whether it moved past 5.3:
   ```
   git show origin/main:source/blender/blenkernel/BKE_blender_version.h | grep "^#define BLENDER_VERSION "
   ```
4. Read the upstream file subversion, to check the collision named in
   [Upstream Fragility](../architecture/upstream_fragility.md):
   ```
   git show origin/main:source/blender/blenkernel/BKE_blender_version.h | grep BLENDER_FILE_SUBVERSION
   ```
   A value of 12 or higher means the fork number collides. Renumber the
   fork block above it.
5. Read the upstream space type enum, to check the other hazard:
   ```
   git show origin/main:source/blender/makesdna/DNA_space_enums.h | grep -E "SPACE_|SPACE_TYPE_NUM"
   ```
   A value of 25 taken by an upstream space type means `SPACE_ADDON`
   needs a new slot.

## Line numbers

Every file path and line number in this knowledge base is measured against
the base commit above. A rebase onto a newer upstream moves them.

## Related

- [Upstream Fragility](../architecture/upstream_fragility.md)
- [Fork Mergeability](../architecture/fork_mergeability.md)
- [Building](./building.md)
