# Audits

Independent audits of the Add-on Space Editor branch
(`pyareas/addon-space-editor` versus `main`), cross-checked against the actual
source. Each file below merges an internal audit with an external audit of the
same subject, and states both verdicts where they disagree.

**Attribution.** An audit names the agent that produced it. This section is the
one place in the knowledge base where an AI name stays in the text. An audit is
only as good as its author, so the reader must know who wrote each claim. The
`sources` block in each file carries the same fact in the OKF `author` field.

| Author | Role | Source files |
|---|---|---|
| Claude Sonnet agent | Internal, read the source and the diff directly | `audit_native_equivalents.md`, `audit_pruning_candidates.md` |
| Gemini | External reviewer | `Gemini_native_blender_equivalents.md`, `Gemini_pruning_and_cleanup_candidates.md`, `Gemini_verified_native_apis.md` |

The source files are in `docs_ui/audits/`, which is no longer tracked by git.

- [Native Blender Equivalents](./native_equivalents.md) — hand-rolled code in
  the Add-on Space Editor, checked against existing native Blender primitives.
- [Pruning Candidates](./pruning_candidates.md) — dead code, stale comments,
  and duplication found in the Add-on Space Editor.
- [Verified Native Blender APIs](./verified_native_apis.md) — code-level
  verification of every native API cited as a proposed replacement, with exact
  signatures and file locations.
- [Audit Reconciliation Summary](./reconciliation_summary.md) — the short-form
  cross-check of all four source audits against each other and the source
  tree, including which external claims did not survive verification.
