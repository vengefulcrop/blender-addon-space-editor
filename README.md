pyareas — Blender fork with add-on editor areas
===============================================

This is a fork of [Blender](https://www.blender.org). It adds a new Space type -- `Space: Addon`, which allows any addon to host its panels in a real editor area, selected from a standard editor-type menu.

`SPACE_ADDON` is the new editor type. It hosts the panels of one add-on. The panels poll and draw without modification, because the area borrows context from an open editor of the type each panel was written for. A sidebar tree lists every enabled add-on that registers panels. Activating a row hosts that add-on.

Everything else in this tree is upstream Blender, under the GNU General Public License, Version 3.

> [!IMPORTANT]
> Cloning from this GitHub fork may cause Git LFS errors. To avoid this, use `GIT_LFS_SKIP_SMUDGE=1` when doing your initial clone.  
> See [the documentation](https://developer.blender.org/docs/handbook/contributing/using_git/#github-mirror) for full instructions.

State
-----

Branch `pyareas/addon-space-editor`, on upstream `main` at `027ef661892`, dated 2026-07-31. Base version: Blender 5.3.0 alpha. The diff touches 43 files under `source/` and `scripts/`: 2346 insertions, 43 deletions.

The feature is a work in progress. 29 tasks and 2 defects are open. Per-panel context delegation is not built. The delegate is one value for the whole area, so an add-on that registers panels for two editor types gets one delegate.

Two changes to upstream constants break without an error and without a rebase conflict. Read [Upstream Fragility](./docs_wiki/architecture/upstream_fragility.md) before you rebase.

Documentation
-------------

`docs_wiki/` is the knowledge base. It follows the Open Knowledge Format (OKF v0.2). AI agents generated most of it and humans verified parts of it. Check a claim against the source before you rely on it.

- [Knowledge base index](./docs_wiki/index.md) — start here.
- [Handoff](./docs_wiki/handoffs/handoff.md) — the state of the branch, the caveats, and what is open.
- [Architecture](./docs_wiki/architecture/index.md) — the space type, panel hosting, and context delegation.
- [Building this fork (Windows)](./docs_wiki/reference/building.md)
- [Traceability](./docs_wiki/reference/traceability.md) — how a documented concept links to the code that implements it.
- [Upstream base and version](./docs_wiki/reference/upstream_base.md) — every line number in the knowledge base is measured against this commit.

First steps in a fresh clone
----------------------------

1. Read [Upstream base and version](./docs_wiki/reference/upstream_base.md).
2. Run `tools/oft/fetch_oft.sh`, then `tools/oft/install_hook.sh`.
3. Build. See [Building this fork](./docs_wiki/reference/building.md).
4. Read [Upstream fragility](./docs_wiki/architecture/upstream_fragility.md) before any rebase.

Checks
------

Each command exits 1 when work remains. A pre-commit hook runs the trace.

| Command | Checks |
|---|---|
| `python docs_wiki/tools/validate_okf.py` | Every concept file carries OKF frontmatter |
| `python docs_wiki/tools/check_references.py` | No document names a symbol or a line that does not exist |
| `python docs_wiki/tools/check_register.py` | Prose register and wording |
| `tools/oft/trace.sh` | Documented concepts match the code that claims them |

Before an upstream patch
------------------------

The traceability tags are not idiomatic Blender. Remove them with `python tools/oft/strip_tags.py`.

Upstream Blender
----------------

- [Main website](https://www.blender.org)
- [Reference manual](https://docs.blender.org/manual/en/latest/index.html)
- [Upstream build instructions](https://developer.blender.org/docs/handbook/building_blender/)
- [Upstream code review and bug tracker](https://projects.blender.org)
- [Developer documentation](https://developer.blender.org/docs/)

Report a defect in this fork here, not on `projects.blender.org`.

License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3. Individual files may have a different but compatible license. See [blender.org/about/license](https://www.blender.org/about/license).
