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
