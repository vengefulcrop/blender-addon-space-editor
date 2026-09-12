---
type: reference
title: "Traceability Between the Wiki and the Code"
description: "How a documented concept links to the code that implements it, how to run the trace, and how to remove the tags before an upstream patch"
tags: [reference, traceability, openfasttrace, tooling]
last_updated: 2026-09-12
---

# Traceability Between the Wiki and the Code

A documented concept carries an ID. The code that implements it carries a tag
with the same ID. The link is per concept, not per file.

[OpenFastTrace](https://github.com/itsallcode/openfasttrace) 4.9.0 checks the
two sides against each other. It is one jar and needs a Java runtime.

## Setup, once per clone

```
tools/oft/fetch_oft.sh       download the jar, checked against a pinned SHA-256
tools/oft/install_hook.sh    install the pre-commit hook
```

The jar is not in the repository. OpenFastTrace is GPL-3.0, and shipping the
binary takes on source-availability duties this fork does not need. The tool
runs as a separate process and links into nothing, so no license reaches the
Blender code.

The hook lives in `.git/hooks/`, which git does not clone. Every clone runs
`install_hook.sh` for itself.

## The shape of a link

In the wiki, under the section heading:

```markdown
## Single delegate per area, not per panel

`arch~single-delegate-per-area~1`

Needs: impl
```

In the code, on its own line above the function:

```c
/* [impl->arch~single-delegate-per-area~1] */
static short addon_delegate_spacetype_find(const bContext *C, ...)
```

`arch` is the artifact type. The middle is the name. `~1` is the version.

## Running the trace

```
tools/oft/trace.sh           report failures, exit 1 when work remains
tools/oft/trace.sh all       report every item
tools/oft/trace.sh summary   one line
```

| Verdict | Meaning |
|---|---|
| covered | The wiki and the code agree |
| uncovered | A documented concept that no code claims |
| outdated | The code claims an older version than the wiki |
| orphaned | The code claims an ID that no wiki section defines |

A pre-commit hook runs the trace on any commit that touches `source/`,
`scripts/`, or `docs_wiki/`. A commit that breaks the trace is refused.

## Raising a version

The decision to raise a version is a judgment, not a mechanical step. Raise it
when the meaning of a concept changes, not when its wording changes.

```
tools/oft/bump.py <id>           raise the wiki concept, leave the code
tools/oft/bump.py <id> --accept  raise the code tags to match
```

The two steps are separate on purpose. `bump.py <id>` raises the wiki only.
The trace then reports every code site as `outdated`, and names each one.
Review each site, then run `--accept` to raise the tags.

A single command that raised both sides at once would defeat the check. The
point of the version is to force a look at every claiming site.

## Before an upstream patch

The tags are not idiomatic Blender. Remove them before sending a patch to
`projects.blender.org`:

```
python tools/oft/strip_tags.py --check    report, change nothing
python tools/oft/strip_tags.py            remove the tag lines
```

A tag sits on its own line, so removing the line leaves the comment untouched.

Never put `strip_tags.py --check` in a hook. This fork carries the tags, so
the check fails on every commit by design.

## What the trace cannot do

The trace does not know that a code change altered a documented meaning.
Nothing changes on the wiki side, so the trace still passes. Reading the
tagged concept and judging the change is the work of the person or agent
making the change. The tag gives the pointer, not the verdict.

## Related

- [Fork Mergeability](../architecture/fork_mergeability.md)
- [Upstream Base and Version](./upstream_base.md)
