# Agent Guidelines — Blender Fork (pyareas)

This fork adds the ability to host add-on panels in real editor areas.

## Knowledge Base (OKF)
- The documentation and technical knowledge base is in `docs_wiki/`.
- It conforms to the **Open Knowledge Format (OKF v0.2)**.
- **Reading Context:** Read `docs_wiki/index.md` before you make an
  architectural decision or implement a feature.
- **Protected Folders:** `docs_wiki/raw/` and `docs_wiki/design/notes/` are
  **STRICTLY READ-ONLY** for AI agents. Do not edit or create files there.
  `docs_wiki/raw/` holds the handwritten notes of the user.
- **Writing Knowledge:** Write new patterns, APIs, and architectural
  decisions to `docs_wiki/` under the OKF rules: `type:` in the frontmatter,
  update the parent `index.md`, and add a record to `log.md`.
- Full rules: `docs_wiki/CLAUDE.md`.

## Source Tree
- Blender C/C++ sources are in `source/`. Follow the upstream style in
  `.clang-format` and the handbooks in `docs_wiki/reference/`.
- Python tests for the add-on editor are in `docs_ui/test_addon_editor*.py`.
