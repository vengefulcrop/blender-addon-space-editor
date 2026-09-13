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
- **Use the `claude-canvas` plugin for canvas work.** It is installed at
  `~/.claude/skills/claude-canvas` and loads as `claude-canvas@skills-dir`.
  It brings the `/canvas` command, its skills, and the scripts under
  `scripts/`: `canvas_validate.py`, `canvas_layout.py`, `canvas_template.py`.
- **Validate every canvas** with the plugin's `canvas_validate.py`. Keep
  every coordinate on the 20 pixel grid. Put group nodes first in the array.
- `.excalidraw` files are hand-made. Treat them as read-only.

## 1c. Verbatim External Copies (DO NOT REWRITE)
These files are unmodified copies of documents written elsewhere. The
ASD-STE100 rules, the register rules, and the audience rules do NOT apply to
them. Do not rewrite the prose, the headings, or the wording. A file carries
`verbatim: true` in its frontmatter when this applies.

- `reference/c_cpp_style.md`
- `reference/c_cpp_best_practice.md`
- `reference/blend_file_compatibility.md`
- `reference/okf_spec.md`

Only two edits are allowed: update the YAML frontmatter, and refresh the copy
from its upstream source. To record a local deviation, write a separate file
and link to it.

## 1d. The `research/` Folder Is Not Distributed
`research/` holds legacy studies, unrelated research, and internal memos.
`.gitignore` excludes it, so a clone of this repository does not carry it.
The files stay on disk for local reading.

- Do not link to `research/` from any other section. Such a link breaks for
  every reader who clones the repository.
- Do not move a file out of `research/` to make it distributable. Write the
  fact into the right section instead, and leave the research file alone.
- Rule 1b still applies inside the folder: an audit names the agent that
  produced it.

## 1e. Deleting Code Means Deleting Its Documentation

Every wrong claim found in this knowledge base traces to one cause. A
feature was deleted, and its documentation stayed. The reader then hunts
for machinery that is not there.

**When you delete or rename a symbol, a function, a struct field, an
operator, or a whole feature, the same commit updates every place that
names it.** This is part of the deletion, not a follow-up task.

Do this in order:

1. Grep the whole repository for the name, before you delete it:
   ```
   grep -rn "<symbol>" source/ scripts/ docs_wiki/
   ```
2. Delete the code.
3. Fix every hit the grep found. For each one, choose:
   - The claim is now false. Rewrite it to state what the code does.
   - The claim records history (a log entry, an ADR, a defect record).
     Keep it, and mark it: say the symbol no longer exists, or mark the
     section superseded.
4. Run the checker:
   ```
   python docs_wiki/tools/check_references.py
   ```
   It reports a symbol this fork owns that no longer exists, and a
   `file.cc:123` citation whose line is past the end of the file. A line
   that says a symbol is gone is not flagged. Exit code 1 means work
   remains.
5. Run `python docs_wiki/tools/check_facts.py <file>` on every document
   you edited, so a rewrite does not lose a fact.

An ADR is never rewritten to match new code. Add a status line at the top
that says which part is superseded, and leave the recorded decision.

A line number moves on every rebase. Cite a symbol name first, and a line
number second.

## 1f. Traceability Tags (OpenFastTrace)

A documented concept carries an ID. The code that implements it carries a
tag with the same ID. The link is per concept, not per file.

**In the wiki**, under the section heading:

```markdown
## Single delegate per area, not per panel

`arch~single-delegate-per-area~1`

Needs: impl
```

**In the code**, on its own line above the function:

```c
/* [impl->arch~single-delegate-per-area~1] */
static short addon_delegate_spacetype_find(const bContext *C, ...)
```

### The routine

**Before you edit a tagged function**, read the concept it names. That tag
is the direct pointer to the part of the wiki your change affects. Do not
search for it.

**After you change what the code does**, decide one thing: did the change
alter the documented meaning?
- No. Leave the tag as it is.
- Yes. Update the wiki section, then raise the version in two steps:

```
python tools/oft/bump.py <id>           raise the wiki, leave the code
python tools/oft/bump.py <id> --accept  raise the code tags to match
python tools/oft/bump.py --list         show every concept and its sites
```

`bump.py <id>` raises the wiki only, then names every code site that still
claims the old version. Read each site and confirm it still matches the
concept. Only then run `--accept`. Never skip the review. The version
exists to force that look.

The reader-facing account is
[Traceability](./reference/traceability.md).

**When you delete tagged code**, the concept becomes uncovered and the
trace fails. Either delete the concept from the wiki, or mark the section
superseded and remove its `Needs: impl` line. See rule 1e.

**When you add a concept to the wiki**, give it an ID and `Needs: impl`
only when code implements it. A design note with no implementation needs
no ID.

### Running it

```
tools/oft/fetch_oft.sh       once per clone, downloads and verifies the jar
tools/oft/install_hook.sh    once per clone, installs the pre-commit hook
tools/oft/trace.sh           report failures, exit 1 when work remains
tools/oft/trace.sh all       report every item
```

**A pre-commit hook runs the trace.** It fires only on a commit that touches
`source/`, `scripts/`, or `docs_wiki/`. A commit that breaks the trace is
refused. Do not reach for `git commit --no-verify` to get past it. Fix the
item the report names. Use `--no-verify` only when the user tells you to
leave the mismatch, and say in the commit message that you did.

If the jar is absent the hook prints a note and allows the commit. Run
`tools/oft/fetch_oft.sh` rather than working without the check.

Four verdicts matter:

| Verdict | Meaning |
|---|---|
| covered | The doc and the code agree |
| uncovered | A documented concept that no code claims |
| outdated | The code claims an older version than the doc |
| orphaned | The code claims an ID that no doc defines |

### Before an upstream patch

The tags are not idiomatic Blender. Remove them before you send a patch to
`projects.blender.org`:

```
python tools/oft/strip_tags.py --check    report, change nothing
python tools/oft/strip_tags.py            remove the tag lines
```

A tag always sits on its own line, so removing the line leaves the
surrounding comment untouched.

Never put `strip_tags.py --check` in a hook. This fork is meant to carry the
tags, so the check fails on every commit by design. It is a one-off step
before an upstream patch.

### What the trace cannot do

The trace does not know that a code change altered a documented meaning.
Nothing changed on the wiki side, so the trace still passes. Reading the
tagged concept and judging the change is your work. The tag gives you the
pointer, not the verdict.

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

   **Register, on top of STE.** Cut ornate and narrative wording to a minimum.
   - **No narrative or novelistic framing.** Do not tell an evolutionary story
     or dramatize code history. Ban story phrasing and narrative resolution
     ("was the one file with real ongoing cost", "the fix made", "the objection
     is gone", "what remains is"). State what earlier code did, what changed,
     and what the current code does.
   - **Dry technical register over metric-gaming.** Passing statistical STE
     checks (sentence length, forbidden word lists) is not sufficient on its own.
     The prose must read as a dry technical reference, not an essay, magazine
     article, or developer journal.
   - **Structural simplicity.** Do not link narrative thoughts with colons,
     em-dashes, or compound subordinate clauses. Use short, single-idea
     declarative sentences in active voice.
   - **Headings carry no editorial framing or adjectives.** Do not use "Why...",
     "How...", or questions in headings ("Why the generic-context refactor
     mattered"). Write "Generic context refactor". A heading names the subject
     and stops.
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

   **Words to ration.** Each one is correct in the right place and becomes
   noise when repeated. Use each at most once per document, and only where
   it carries a fact the sentence would otherwise lose:

   | Word | Use it for | Do not use it for |
   |---|---|---|
   | silent, silently | A failure that emits no error, warning, or conflict | A heading, a summary, or a second mention of a fault already described |
   | quiet | Nothing. Write "silent" once, or name the fact | Any use |
   | hazard, peril | Nothing. Write "caveat", "risk", or "pitfall" | Any use |
   | subtle, insidious | Nothing. State what happens | Any use |
   | catastrophic, disastrous | Nothing. State the loss | Any use |

   Prefer the fact over the adjective. "Git reports no conflict, and the
   build succeeds" beats "fails silently", because it says what a reader
   would see.

   **Headings carry no adjectives.** Write "Caveats", not "The two caveats
   that fail silently". Write "What is open", not "What is worth knowing
   before you start". A heading names the subject and stops.

   Run the register check before you commit a document:

   ```
   python docs_wiki/tools/check_register.py
   ```

   The check skips `CLAUDE.md`, `GEMINI.md`, `log.md`, and
   `tools/register_wordlist.md`. These files name the banned words, so a
   scan of them reports each word as a violation. Add a file to the `skip`
   set in `check_register.py` only for that reason.

   **Commit messages are subject to these rules.** A commit message must
   adhere to the same register, STE, and concision standards. Use the
   imperative mood ("docs: clarify...", not "docs: clarified..."). Cut
   narrative storytelling, dramatic framing, and conversational commentary
   from both the summary line and the commit body. State what was changed
   and the factual reason for the change.

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

## 9. The Style Linter Over-Counts Three Things

Run the linter on every file you write or edit:

```
python docs_wiki/tools/ste_lint.py --json <file>
```

Read `per100w`. The target is 2.5 or lower.

**The raw score is not the measure.** Subtract these three before you judge a
file. Never rewrite correct text to satisfy a false positive:

1. **Possessives.** The linter counts `panel's` and `add-on's` as
   contractions. ASD-STE100 bans a contraction such as "don't". A possessive
   is correct English and stays. One file scored 37 of these.
2. **YAML frontmatter.** The linter reads the `tags:` line as prose, so every
   file gets a noun-train hit from its own frontmatter.
3. **Markdown tables.** The linter joins each table row into one sentence, so
   a file built around a table reports long sentences that do not exist.

To get the true score, strip the frontmatter, the fenced code blocks, and
every line that starts with `|`. Then subtract the possessive count from the
contraction count.

These violations are always real: `semicolon`, `passive_voice`,
`complex_tense`, `long_paragraph(>6s)`, `phrasal_verb`,
`marketing_adjective`, `banned_word`, and a `long_sentence(>20w)` outside a
table.

A file marked `verbatim: true` is exempt. See rule 1c.

## 10. Keep the Two Rule Files in Step

`CLAUDE.md` is the source of truth. `GEMINI.md` is generated from it, and the
two differ in section 1b only, which names the canvas tooling.

After you edit `CLAUDE.md`, run:

```
python docs_wiki/tools/sync_agent_rules.py
```

Never edit `GEMINI.md` by hand. The script overwrites it.
