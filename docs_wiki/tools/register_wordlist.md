---
type: reference
title: "Banned Register Patterns and Wordlist"
description: "Patterns, phrases, and vocabulary used to detect and eliminate novelistic, dramatic, and conversational register violations"
tags: [tools, register, ste, style, linter]
last_updated: 2026-09-12
---

# Banned Register Patterns and Wordlist

This document records the patterns, phrases, and vocabulary checked when
enforcing rule 3 of `CLAUDE.md` and `GEMINI.md`. These criteria enforce a dry,
declarative technical register beyond statistical STE compliance alone.

Commit messages are subject to these same rules: they must use the imperative
mood, cut storytelling and dramatic framing, and state the factual change and
its reason directly.

## 1. Editorial and Conversational Headings

Headings must name the subject and stop. A heading must not pose questions,
frame value judgments, or offer tutorial hand-holding.

| Banned Heading Pattern | Problem | Required Phrasing |
|---|---|---|
| `## Why [X] mattered` | Evaluative / narrative | `## [X] refactor` or `## [X] architecture` |
| `## Why [X] mostly works` | Conversational / hedged | `## [X] context resolution` |
| `## What breaks, and the [X] fix` | Storytelling / drama | `## [X] failure conditions` |
| `## Does Blender's engine do [X]?` | Rhetorical question | `## Upstream [X] implementations` |
| `## What fixing [X] would cost` | Narrative framing | `## [X] implementation requirements` |
| `## How to measure [X] yourself` | 2nd person / tutorial | `## [X] measurement procedure` |
| `## The idea` | Vague / casual | `## [X] concept overview` |
| `## What was built (first pass)` | Journey / journal | `## Initial implementation` |
| `## Why this matters` | Hand-holding | `## [X] rationale` or `## Test coverage objectives` |
| `## Not built yet` | Informal / conversational | `## Deferred features` |

## 2. Retrospective Narrative and Storytelling

Do not narrate code evolution or dramatize author decisions. State the earlier
behavior, the change, and the current state.

| Banned Phrase / Device | Category | Required Treatment |
|---|---|---|
| `was the one file with real ongoing cost` | Retrospective drama | State the conflict or maintenance site factually. |
| `the design objection is gone` | Narrative resolution | State that code no longer names the entity. |
| `the fix made [X] a [Y]` | Storytelling | State `[X] was replaced with [Y]`. |
| `what remains is an ordinary...` | Narrative conclusion | State the current risk or rebase surface directly. |
| `stage proved insufficient before the next` | Process narration | State the evaluated options directly (`Three scopes were evaluated:`). |
| `turned out load-bearing, not cosmetic` | Retrospective surprise | State the requirement directly (`[X] is required for functionality:`). |
| `presents as "the sidebar is not there"` | Colloquial diagnosis | State the technical defect (`Build-order defects`). |
| `we decided` / `we found` / `we chose` | Self-reference | State the architectural fact without author attribution. |

## 3. Conversational Idioms and Colloquialisms

Conversational filler obscures technical precision and adds unneeded words.

| Banned / Flagged Expression | Replacement |
|---|---|
| `in practice` | State the exact condition under which the statement holds. |
| `as it turns out` / `turns out` | State the fact directly. |
| `under the hood` | Name the specific subsystem, data structure, or function. |
| `the trick is` / `the catch is` | Name the constraint, requirement, or limitation. |
| `boiled down to` / `boils down to` | State the exact mechanism. |
| `deal with` / `wrestle with` | `handle`, `process`, `resolve`. |
| `deep dive` / `take a look` | State what is analyzed or inspected. |
| `at first glance` / `on the surface` | State the apparent versus actual behavior directly. |

## 4. Dramatized and Evaluative Qualifiers

Avoid dramatizing engineering problems or claiming emotional impact.

| Term | Context to Avoid | Approved Replacement |
|---|---|---|
| `real crash` | Emphasizing defect severity | `crash` |
| `the real risk` | Elevating a caveat | Name the specific risk (`Window mismatch risk`). |
| `the real tradeoff` | Journalistic contrast | `tradeoff` |
| `worth defending` | Value judgment / advocacy | State the technical justification factually. |
| `silent reintroduction` | Adjective reuse | `regression` |
| `nightmare`, `messy`, `ugly` | Code evaluation | Describe the architectural coupling or formatting issue. |
| `magic`, `miracle`, `hero` | Technical mechanisms | Describe the algorithm or hook explicitly. |
| `silver bullet`, `secret sauce` | Idiomatic claims | State the specific design decision and its effects. |

## 5. Banned and Rationed Words (from CLAUDE.md / GEMINI.md Rule 3)

### Rationed (Maximum 1 Per Document)

- **`silent` / `silently`**: Use only for a failure that emits no error, warning,
  or conflict. Never use in a heading, a summary, or for a second mention of a
  fault already described.

### Banned Entirely

- `quiet`, `quietly` → Name the fact, or use `silent` once.
- `hazard`, `hazards`, `peril` → Write `caveat`, `risk`, or `pitfall`.
- `subtle`, `insidious` → State what happens.
- `catastrophic`, `disastrous` → State the loss.
- `seamless`, `robust`, `powerful`, `elegant` → State what the code does.

## 6. Linter Integration

The vocabulary in this file guides manual inspection and informs the checks in:
- `docs_wiki/tools/check_register.py`
