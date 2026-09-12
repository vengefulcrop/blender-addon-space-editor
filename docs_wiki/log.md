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

  - Added `research/` with 15 files. The folder is no longer distributed.

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
    It said the fork does not touch `BLENDER_FILE_SUBVERSION`, and that developers
    must not bump the define. The code raises it to 12. The newer-version
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
    from the removed picker, as items 28 to 30. They stay marked, not deleted.
    The user states that no real file holds these values, so no versioning
    step is necessary.

  - Recorded that `UserDef::addon_editors` is **not** dead. An audit claimed
    zero writers. `rna_userdef.cc:1120` writes it, exposed to Python as
    `preferences.addon_editors.new()`. It stays as an older display name
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


- **2026-09-12** (Agent: Claude Opus 5):
  - Restored `reference/c_cpp_style.md`, `reference/c_cpp_best_practice.md`,
    and `reference/blend_file_compatibility.md` verbatim from their sources.
    A linguistic pass had rewritten them. They are copies of Blender
    documents written elsewhere, so the prose must match the source.
  - Added rule 1c to `CLAUDE.md` and `GEMINI.md`. It names the four verbatim
    files and states that the language rules do not apply to them.
  - Excluded `research/` from git. The folder holds legacy studies, unrelated
    research, and internal memos. The files stay on disk.
  - Added rule 1d. It forbids a link from any other section into `research/`,
    because such a link breaks for a reader who clones the repository.

- **2026-09-12** (Agent: Claude Opus 5):
  - Restored the style linter caveats as rule 9 of `CLAUDE.md`. The rule
    names the three things `ste-lint.py` over-counts, so no agent rewrites
    correct English to satisfy a false positive.
  - Added rule 10 and
    [tools/sync_agent_rules.py](./tools/sync_agent_rules.py). `CLAUDE.md` is
    now the source of truth. The script generates `GEMINI.md` from it, and
    `--check` reports drift.

- **2026-09-12** (Agent: Claude Opus 5, wiki and code agreement audit):
  - Rewrote three sections of
    [addon_space_type.md](./architecture/addon_space_type.md). They described
    the `SpaceType` subtype mechanism, which the sidebar tree replaced. The
    symbols `ADDON_SUBTYPE_PICK` and `addon_space_subtype_get` do not exist.
  - Marked the subtype half of
    [ADR-001](./decisions/adr_001_addon_space_type_vs_dynamic_registration.md)
    as superseded. The core decision holds.
  - Corrected 9 code citations that pointed at the wrong function.
  - Corrected two claims in [bugfix.md](./operations/bugfix.md): the flag bit
    location, and the subversion number, which now reads 12 after a later
    change.

- **2026-09-12** (Agent: Claude Opus 5 and two Claude Sonnet agents):
  - Added rule 1e to `CLAUDE.md` and `GEMINI.md`: deleting code means
    deleting its documentation, in the same commit.
  - Added [tools/check_references.py](./tools/check_references.py). It finds
    a symbol this fork owns that no longer exists, and a citation whose line
    is past the end of the file.
  - Fixed the 18 dead references the checker found, across 7 files. An
    architecture doc states the current mechanism. An ADR keeps its decision
    and gains a status line. A defect record keeps its history and marks the
    dead symbol.
  - Recorded in [testing.md](./operations/testing.md) that the three test
    scripts are not distributed.

- **2026-09-12** (Agent: Claude Opus 5):
  - Wired OpenFastTrace 4.9.0. Five architecture concepts now carry an ID,
    and five functions carry a matching tag. The trace passes on 10 items.
  - Added rule 1f to `CLAUDE.md` and `GEMINI.md`. It gives the routine: read
    the tagged concept before you edit, and raise the version when the
    meaning changes.
  - Added `tools/oft/`: `fetch_oft.sh`, `trace.sh`, and `strip_tags.py`. The
    jar is not committed.

- **2026-09-12** (Agent: Claude Opus 5):
  - Added a pre-commit hook that runs the trace, plus
    `tools/oft/install_hook.sh` to install it. A commit that touches
    `source/`, `scripts/`, or `docs_wiki/` and breaks the trace is refused.
  - Pinned a SHA-256 checksum in `tools/oft/fetch_oft.sh`. The jar stays out
    of the repository, because OpenFastTrace is GPL-3.0.
  - Extended rule 1f with the hook, and with one correction: never put
    `strip_tags.py --check` in a hook, because this fork carries the tags.

- **2026-09-12** (Agent: Claude Opus 5):
  - Added [reference/traceability.md](./reference/traceability.md). The
    traceability system was described only in the agent rule files, which are
    instructions, not reference. A reader navigating the wiki could not find
    it.
  - Added `tools/oft/bump.py`. It raises a concept version in two steps. The
    first raises the wiki and names every code site still claiming the old
    version. The second raises those tags, after review.

- **2026-09-12** (Agent: Claude Opus 5):
  - Rewrote [handoff.md](./handoffs/handoff.md) for a third-party reader. The
    old one cited files under `docs_ui/`, which a clone does not carry.
  - Archived the old one as
    [handoff_2026-09-12_pre-distribution.md](./handoffs/archive/handoff_2026-09-12_pre-distribution.md),
    and corrected its relative links for the deeper folder.
  - Fixed mojibake in `handoffs/index.md`, where two em-dashes were stored in
    the wrong encoding.

- **2026-09-12** (Agent: Claude Opus 5):
  - Extended rule 3 of `CLAUDE.md` with a rationed word list. "silent" is
    allowed once per document. "quiet", "hazard", "subtle", and
    "catastrophic" are not allowed at all. A heading carries no adjective.
  - Added [tools/check_register.py](./tools/check_register.py). The style
    linter scores sentence shape and says nothing about register, so a
    document can score well and still read as overwritten.
  - Cleared 16 instances across 7 files, and renamed two headings.

- **2026-09-12** (Agent: Claude Opus 5):
  - Corrected the distribution claim in
    [fork_mergeability.md](./architecture/fork_mergeability.md),
    [addon_space_type.md](./architecture/addon_space_type.md), and
    [building.md](./reference/building.md). They said the fork publishes a
    diff produced with `git format-patch`. The fork ships as a git branch,
    and a second developer fetches and rebases it.
- **2026-09-12** (Agent: Antigravity):
  - Sharpened rule 3 in `CLAUDE.md` and `GEMINI.md` to forbid narrative framing,
    code history storytelling, editorial headings, and conversational devices.
  - Rewrote headings and novelistic prose across `architecture/fork_mergeability.md`,
    `architecture/context_delegation.md`, `architecture/panel_hosting.md`,
    `architecture/multiwindow_context_search.md`, `architecture/sidebar_tree_view.md`,
    `operations/testing.md`, and `reference/upstream_base.md` into dry, factual STE.
  - Added [tools/register_wordlist.md](./tools/register_wordlist.md) cataloging
    banned register patterns, narrative devices, and colloquialisms.
  - Added "why", "how", "matters", and "yourself" to `HEADING_WORDS` in
    `tools/check_register.py`, and linked the wordlist in `index.md`.


- **2026-09-12** (Agent: Claude Opus 5):
  - Squashed the prose-only commits into five documentation milestones. The
    branch holds 49 commits on `027ef661892`, and the tree is unchanged.
  - Corrected the commit count in [handoff.md](./handoffs/handoff.md) from 97
    to 49.
  - Replaced the root `README.md` with the fork README, and deleted
    `.github/README.md`, which carried the upstream Blender text.
