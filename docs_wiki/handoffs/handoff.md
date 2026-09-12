---
type: operation
title: "Add-on Space Editor — Handoff"
description: "State of the pyareas/addon-space-editor branch: what works, what is open, and what a second developer does first"
tags: [addon-space-editor, handoff]
last_updated: 2026-09-12
---

# Add-on Space Editor — Handoff

Branch `pyareas/addon-space-editor`, on upstream `main` at
`027ef661892` (2026-07-31). The diff touches 43 files under `source/` and
`scripts/`: 2346 insertions, 43 deletions.

Blender 5.3.0 alpha. See
[Upstream Base and Version](../reference/upstream_base.md) for the full
table and the commands that measure the gap to current upstream.

## What the feature does

`SPACE_ADDON` is a new editor type. It hosts the panels of one add-on in a
real editor area. The panels poll and draw correctly, because the area
borrows context from an open editor of the type each panel was written for.

The editor type menu shows one plain "Add-on" entry. A sidebar tree lists
every enabled add-on that registers panels, and activating a row hosts it.

Read [Add-on Space Type](../architecture/addon_space_type.md) first, then
[Panel Hosting](../architecture/panel_hosting.md) and
[Context Delegation](../architecture/context_delegation.md).

## First steps in a fresh clone

1. Read [Upstream Base and Version](../reference/upstream_base.md) and note
   the base commit. Every line number in this knowledge base is measured
   against it.
2. Run `tools/oft/fetch_oft.sh`, then `tools/oft/install_hook.sh`. See
   [Traceability](../reference/traceability.md).
3. Build. See [Building This Fork](../reference/building.md).
4. Before any rebase, read
   [Upstream Fragility](../architecture/upstream_fragility.md).

## Caveats

Neither reports an error. Both pass a build and a rebase.

| Change | What goes wrong |
|---|---|
| `SPACE_ADDON = 25`, `SPACE_TYPE_NUM` rebased onto it | Upstream takes slot 25. Git reports no conflict. A saved file opens as the wrong editor |
| `BLENDER_FILE_SUBVERSION` raised from 10 to 12 | Upstream reuses 11 and 12. A stock build skips its own versioning on a file this fork saved |

[Upstream Fragility](../architecture/upstream_fragility.md) gives the check
for each.

## What is open

29 tasks in [todo.md](../operations/todo.md) and 2 defects in
[bugfix.md](../operations/bugfix.md).

Three of them first:

- **Per-panel context delegation is not built.** The delegate is one value
  for the whole area. An add-on that registers panels for two editor types
  gets one delegate. The panel that does not match reads context from the
  wrong editor. A code trace settled this. `todo.md` item 13 holds the
  evidence and the three parts a fix needs.
- **Three dead symbols stay in the tree**: `active_addon_editor_index`,
  `addon_editor_max_visible`, and `CTX_wm_space_addon()`. Items 28 to 30.
  Delete them in one commit after the next rebase. That keeps the conflict
  surface small.
- **The subversion 11 to 12 versioning block is untested.** No file saved
  before the sidebar existed has gone through it.

## What holds the documentation to the code

Three checks. Each exits 1 when work remains.

| Command | Checks |
|---|---|
| `python docs_wiki/tools/validate_okf.py` | Every concept file carries OKF frontmatter |
| `python docs_wiki/tools/check_references.py` | No document names a symbol or a line that does not exist |
| `tools/oft/trace.sh` | Every documented concept matches the code that claims it |

A pre-commit hook runs the trace. `check_facts.py` compares a document
against its committed version, so a rewrite does not lose a fact.

## What is not in the repository

`.gitignore` excludes these. They stay on disk here and do not reach a
clone:

- `docs_wiki/research/` holds legacy studies and internal memos.
- `docs_ui/`, `docs_cad/`, `docs_sky/`, and
  `docs_experimentalfeatures_new/` hold the working notes this knowledge
  base was built from. The three manual test scripts named in
  [testing.md](../operations/testing.md) are among them.

## Before an upstream patch

Remove the traceability tags. They are not idiomatic Blender:

```
python tools/oft/strip_tags.py
```

## Related

- [Fork Mergeability](../architecture/fork_mergeability.md)
- [Traceability](../reference/traceability.md)
- [Archive](./archive/index.md)
