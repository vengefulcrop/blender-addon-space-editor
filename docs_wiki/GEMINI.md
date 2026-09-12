# Agent Instructions — pyareas OKF Knowledge Base

This folder is organized under the **Open Knowledge Format (OKF v0.2)**
standard.

## 1. Protected Folders (CRITICAL)
- **`raw/` is STRICTLY READ-ONLY FOR ALL AI AGENTS.**
  - It holds the handwritten notes of the user. No agent processes them.
  - AI agents may read every file in `raw/` to understand user intent.
  - AI agents **must NEVER create, edit, delete, move, or reformat a file
    inside `raw/`**. This includes a fix to the spelling, the format, or the
    frontmatter.
  - The notes carry no OKF frontmatter, and the validator skips the folder.
  - To act on a note, write the result to another section, and link back to
    the note with a relative link.
- **`design/notes/` is STRICTLY READ-ONLY FOR ALL AI AGENTS.**
  - AI agents may read files in `design/notes/` to understand user intent,
    brainstorming, and requirements.
  - AI agents **must NEVER create, edit, or delete any file inside
    `design/notes/`**.
- **`design/` is Human-Primary:**
  - Files directly under `design/` are human-authored specifications. Do not
    alter or rewrite them unless the user asks for it.

## 1b. Canvas and Excalidraw Files
- A `.canvas` file is a schematic of the UI, in JSON Canvas format.
- **Never write a `.canvas` by hand.** Hand-placed nodes drift off the 20
  pixel grid and cannot be nudged without jumping.
- Keep every coordinate on the 20 pixel grid. Put group nodes first in the
  array.
- The canvas tooling lives in the `claude-canvas` plugin at
  `~/.claude/skills/canvas`, with the scripts `canvas_validate.py`,
  `canvas_layout.py`, and `canvas_template.py`. Run `canvas_validate.py` on
  every canvas you write. If you cannot run it, do not write the canvas.
- `.excalidraw` files are hand-made. Treat them as read-only.

## 2. Navigation Protocol (Progressive Disclosure)
- Always start with the root `index.md`.
- Read the section `index.md` files (for example `architecture/index.md`) to
  find a concept before you search or scan many files.

## 3. Authoring and Contribution Protocol
When you add or update knowledge:
1. **YAML Frontmatter:** Every concept file (except the reserved files
   `index.md`, `log.md`, `README.md`, `CLAUDE.md`, `GEMINI.md`) MUST have
   YAML frontmatter:
   ```yaml
   ---
   type: <architecture | spec | reference | decision | operation | research>
   title: "Descriptive Title"
   description: "One-line summary for progressive disclosure"
   tags: [tag1, tag2]
   last_updated: YYYY-MM-DD
   ---
   ```
1b. **Attribution of an AI author.** Do not name an AI agent in a document,
   with one exception. In `research/audits/`, name the agent that produced each
   audit, in the text and in the `author` field of the `sources` block. An audit
   is only as good as its author, so the reader must be able to track it.
2. **Audience.** The wiki is a reference for a third-party developer, not a
   channel for internal messages. Assume the reader is a competent developer
   who wants to understand the design decisions and the structure of the
   project. Write for that reader.
   - Never write about the reader in the third person. A section titled
     "Why the base matters to a second developer" is an internal note. State
     the fact instead: the base commit is what every line number is measured
     against.
   - Cut internal asides, progress notes, and self-reference. "We decided",
     "an agent found", "as noted earlier in this session" do not belong here.
   - No guidance language and no hand-holding. Do not explain why the reader
     should care. State what is true and let the reader judge.
   - Keep the technical jargon. The reader knows what DNA, RNA, a space type,
     and a rebase are. Do not define them.

3. **Language and Style (STE-100):** Always invoke and apply the
   `asd-ste100` skill when you write or edit documentation. Apply Simplified
   Technical English to keep the prose short and exact.

   **Register, on top of STE.** Cut ornate wording to a minimum.
   - No dramatized nouns. Write "caveats" or "pitfalls", not "silent
     hazards". Write "risk", not "quiet data fault".
   - Prefer the plain word. Use "check", not "interrogate". Use "old", not
     "legacy", where "old" is accurate.
   - Prefer concision. Cut a clause that carries no fact. (Note: STE still
     bans grammatical contractions such as "do not" to "don't". Concision
     here means fewer words, not shortened words. Say so if the intent
     was the opposite.)
   - Ornate is not the same as technical. Keep a precise technical term even
     when it is long. `PanelDrawContextOverride` stays as it is.

4. **Provenance.** State plainly that this documentation is largely generated
   by an AI agent, then verified in part by a human. The disclaimer lives at
   the top of the root `index.md`. Do not remove it.
5. **Link Relationships:** Use relative markdown links (`[Title](./path.md)`)
   to connect related concepts.
6. **Register New Concepts:** Add a link to every new document in the parent
   directory `index.md`.
7. **Log Updates:** Append a short record to `log.md` with the agent name,
   the date, and the changes.
8. **Validation:** Run `python docs_wiki/tools/validate_okf.py` to confirm
   compliance.

## 9. The linter, and what it over-counts

Run the linter on every file you write or edit:

```
python docs_wiki/tools/ste_lint.py --json <file>
```

Read `per100w`. The target is 2.5 or lower.

**The raw score is not the measure.** The linter over-counts three things.
Subtract them before you judge a file. Never rewrite correct text to satisfy
a false positive:

1. **Possessives.** It counts `panel's` and `add-on's` as contractions. STE
   bans a contraction such as "don't". A possessive is correct English and
   stays. One file scored 37 of these.
2. **YAML frontmatter.** It reads the `tags:` line as prose, so every file
   gets a phantom noun-train hit from its own frontmatter.
3. **Markdown tables.** It joins each table row into one long sentence, so a
   file built around a table reports many long sentences that do not exist.

To get the true score, strip the frontmatter, the fenced code blocks, and
every line that starts with `|`. Then subtract the possessive count from the
contraction count.

These violations are always real: `semicolon`, `passive_voice`,
`complex_tense`, `long_paragraph(>6s)`, `phrasal_verb`,
`marketing_adjective`, `banned_word`, and a `long_sentence(>20w)` outside a
table.

## 10. Files not yet processed

These 37 files still need the audience pass (rule 2), the register pass
(rule 3), and the STE pass. A file not on this list is done.

- `architecture/addon_space_type.md`
- `architecture/index.md`
- `architecture/multiwindow_context_search.md`
- `architecture/persistence_and_compatibility.md`
- `decisions/adr_001_addon_space_type_vs_dynamic_registration.md`
- `decisions/adr_002_panel_rehosting_via_region_layout_hook.md`
- `decisions/index.md`
- `design/index.md`
- `design/ux_addon_picker.md`
- `design/ux_empty_state_and_header.md`
- `design/ux_sidebar_bookmarks.md`
- `handoffs/archive/index.md`
- `handoffs/handoff.md`
- `handoffs/index.md`
- `log.md`
- `operations/bugfix.md`
- `operations/index.md`
- `operations/testing.md`
- `operations/todo.md`
- `reference/blend_file_compatibility.md`
- `reference/building.md`
- `reference/c_cpp_best_practice.md`
- `reference/c_cpp_style.md`
- `reference/index.md`
- `research/audits/index.md`
- `research/audits/native_equivalents.md`
- `research/audits/pruning_candidates.md`
- `research/audits/reconciliation_summary.md`
- `research/audits/verified_native_apis.md`
- `research/index.md`
- `research/precision_snapping/construction_lines_addon_teardown.md`
- `research/precision_snapping/edit_mode_snapping.md`
- `research/precision_snapping/extension_point_map.md`
- `research/precision_snapping/external_prior_art.md`
- `research/precision_snapping/index.md`
- `research/precision_snapping/object_mode_snapping.md`
- `research/precision_snapping/slct_addon_teardown.md`

Work one file at a time. After each file, run the linter, subtract the three
over-counts above, and confirm the true score is 2.5 or lower. Then confirm
that no relative link and no heading anchor broke. Other files link to these
headings by anchor, so keep the heading text when a link points at it.

Record the work in `log.md` under rule 7.
