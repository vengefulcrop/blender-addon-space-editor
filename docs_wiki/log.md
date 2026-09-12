# Knowledge Base Mutation Ledger

This ledger records additions, reorganizations, and structural changes in the
pyareas OKF knowledge base.

- **2026-09-12** (Agent: Claude Opus 5):
  - Established the OKF v0.2 knowledge base in `docs_wiki/`.
  - Copied the [OKF specification](./reference/okf_spec.md) and the
    [validator](./tools/validate_okf.py) from the geodraft-doc bundle.
  - Added the agent rules in `CLAUDE.md` at the repository root and in
    `docs_wiki/CLAUDE.md`.

- **2026-09-12** (Agent: Claude Sonnet, reference section):
  - Added [reference/index.md](./reference/index.md) and four concept files
    from `docs_ui/building.md` and the three C and C++ handbooks.
  - Kept a truncated code fragment in the "Always Use Braces" section as it
    is in the source, and marked it.

- **2026-09-12** (Agent: Claude Sonnet, operations section):
  - Added [operations/](./operations/index.md) with `todo.md`, `bugfix.md`,
    and `testing.md`, from `docs_ui/punch_list.md`,
    `docs_ui/addon_space_editor_testing.md`, and
    `docs_ui/legacy/code_review.md`.
  - Added [handoffs/](./handoffs/index.md) with the current branch state.
  - Items 13 and 25 of `todo.md` stay unconfirmed. The agent matched them to
    commit messages, not to diffs.

- **2026-09-12** (Agent: Claude Sonnet, research section):
  - Added [research/](./research/index.md) with 15 files.
  - Merged each Claude audit and Gemini audit of the same subject into one
    file under `research/audits/`, and kept both verdicts where they differ.
  - Added `research/precision_snapping/` from the seven `docs_cad/` files,
    and `research/sky_glsl_rendering.md` from `docs_sky/`.

- **2026-09-12** (Agent: Claude Sonnet, architecture section):
  - Added [architecture/](./architecture/index.md) with 8 concept files from
    `docs_ui/addon_space_editor_plan.md`, the UX redesign, and the
    multi-window research.
  - Added [design/](./design/index.md) with 4 UX concept files.
  - Added [decisions/](./decisions/index.md) with ADR-001 to ADR-009.

- **2026-09-12** (Agent: Claude Opus 5):
  - Added [raw/](./raw/README.md) for the handwritten notes of the user.
  - Marked the folder as read-only for AI agents in `CLAUDE.md` at the
    repository root and in `docs_wiki/CLAUDE.md`, and registered it in
    [index.md](./index.md).
  - Added `raw` to `EXCLUDED_DIRS` in
    [validate_okf.py](./tools/validate_okf.py), so a raw note needs no
    frontmatter.

- **2026-09-12** (Agent: Claude Opus 5):
  - Added [architecture/upstream_fragility.md](./architecture/upstream_fragility.md).
    It names the two changes that break silently on a rebase: the
    `SPACE_ADDON = 25` enum slot, and the `BLENDER_FILE_SUBVERSION` bump from
    10 to 12.
  - Corrected two false claims in
    [persistence_and_compatibility.md](./architecture/persistence_and_compatibility.md).
    It said the fork does not touch `BLENDER_FILE_SUBVERSION`, and that the
    define must not be bumped. The code raises it to 12. The newer-version
    refusal reads `minversion` and `minsubversion`, which the fork does not
    touch, so a fork file still opens.
  - Rewrote the `DNA_space_enums.h` risk row in
    [fork_mergeability.md](./architecture/fork_mergeability.md), and added a
    row for the subversion bump.

- **2026-09-12** (Agent: Claude Sonnet, two code traces):
  - Settled item 13 of [todo.md](./operations/todo.md). Per-panel delegate
    resolution is **not** implemented. The two commits narrowed the scope of
    the context override, not its resolution. The ucupaint gap stays open.
  - Corrected [handoff.md](./handoffs/handoff.md), which guessed that the two
    commits implemented it.
  - Settled item 25 of [todo.md](./operations/todo.md).
    `SPACE_ADDON_ID_PICK_MARKER` is dead code. Nothing writes it and nothing
    reads it. Two references survive in `DNA_space_types.h`.

- **2026-09-12** (Agent: Claude Opus 5):
  - Replaced every stale commit hash in the wiki. The branch history rewrite
    changed all 39 hashes. Each citation now carries the new hash and the
    commit subject, which survives a later rewrite.
  - Corrected one hash that was wrong before the rewrite: `30304d7bec0` was a
    typo for `30304d7eb49`.

- **2026-09-12** (Agent: Claude Opus 5):
  - Deleted the dead `SPACE_ADDON_ID_PICK_MARKER` macro and rewrote the stale
    `SpaceAddon::addon_id` doc comment in `DNA_space_types.h`.
  - Marked item 25 of [todo.md](./operations/todo.md) as done.

- **2026-09-12** (Agent: Claude Opus 5, pre-alpha audit):
  - Recorded two fixed defects in [bugfix.md](./operations/bugfix.md) as items
    12 and 13: `U.addon_bookmarks` never freed and never swapped, and two
    stale comments.
  - Added section 9 to [todo.md](./operations/todo.md) with three dead symbols
    from the removed picker, as items 28 to 30. They are marked, not deleted.
    The user states that no real file holds these values, so no versioning
    step is needed.
  - Recorded that `UserDef::addon_editors` is **not** dead. An audit claimed
    zero writers. `rna_userdef.cc:1120` writes it, exposed to Python as
    `preferences.addon_editors.new()`. It stays as a legacy display name
    cache.

- **2026-09-12** (Agent: Claude Opus 5):
  - Added [reference/upstream_base.md](./reference/upstream_base.md). It
    records Blender 5.3.0 alpha, the base commit `027ef661892` of 2026-07-31,
    the file subversion 12, and five commands that measure the gap to current
    upstream.
  - Recorded that this clone has not fetched upstream since 2026-07-31. The
    local `origin/main` still points at the base commit.
  - Put the base version at the top of [index.md](./index.md), and linked the
    new file from [reference/index.md](./reference/index.md) and
    [upstream_fragility.md](./architecture/upstream_fragility.md).
