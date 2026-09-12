---
okf_version: "0.2"
title: "pyareas Knowledge Base"
description: "Architecture, design specifications, operational backlogs, and reference studies for the pyareas Blender fork"
last_updated: 2026-09-12
---

# pyareas Knowledge Base

> **Provenance.** 
> AI agents generated most of this documentation from working notes and code changes, with iterative corrections and cross-referencing. Humans verified and corrected parts of it. 
> Treat a claim here as a good-faith lead, and check it against the source before you rely on it.
> The build itself is a different matter: it was compiled and tested manually across many sessions.


The technical knowledge base for the **pyareas** Blender fork, which lets a
user host add-on panels in real editor areas. It is organized under the
**Open Knowledge Format (OKF v0.2)**.

**Base: Blender 5.3.0 alpha, upstream commit `027ef661892`, dated
2026-07-31.** Upstream publishes daily builds, so it moves ahead of this
number. See [Upstream Base and Version](./reference/upstream_base.md) for
the full table, and for the commands that measure the gap.

## Core Navigation (Progressive Disclosure)

- [Architecture](./architecture/index.md) — The Add-on Editor space type, panel
  hosting, context delegation, and the multi-window model.
- [Design Specifications](./design/index.md) — The UX design of the Add-on
  Editor, the tree view, and the area layout.
- [Operations](./operations/index.md) — Active tasks, open defects, and the
  test suite.
- [Architectural Decisions](./decisions/index.md) — Decision records that
  capture the evaluated tradeoffs.
- `research/` — Audits of native Blender equivalents, the precision snapping
  dossier, and the sky rendering study. **Not distributed.** The folder holds
  legacy studies, unrelated research, and internal memos. It stays on disk
  and git ignores it, so a clone of this repository does not carry it.
- [Handoffs](./handoffs/index.md) — The active session handoff and the archive.
- [Raw Notes](./raw/README.md) — The handwritten notes of the user. Read
  only for AI agents, and outside the OKF format.
- [Reference](./reference/index.md) — The Blender C/C++ handbooks, the build
  guide, and the OKF specification.

## Governance and Tools
- [OKF Specification](./reference/okf_spec.md) — The canonical v0.2 format.
- [Validation Tool](./tools/validate_okf.py) — The conformance linter
  (`python docs_wiki/tools/validate_okf.py`).
- [Mutation Ledger](./log.md) — The audit record of knowledge base changes.
