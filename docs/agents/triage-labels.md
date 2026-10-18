# Triage Labels

The skills speak in terms of canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

## Canonical role mapping

| Canonical role | Label in our tracker | Meaning |
| --- | --- | --- |
| `bug` | `type:bug` | Something is broken |
| `enhancement` | `type:feature`, `type:performance`, `type:refactor`, `type:docs`, `type:test`, `type:chore`, `type:security` | New feature or improvement (any non-bug type) |
| `needs-triage` | `status:needs-triage` | Maintainer needs to evaluate this issue |
| `needs-info` | `status:needs-info` | Waiting on reporter for more information |
| `ready-for-agent` | `status:ready-for-agent` | Fully specified, ready for an AFK agent |
| `ready-for-human` | `status:ready-for-human` | Requires human implementation |
| `wontfix` | `status:wontfix` | Will not be actioned |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

## Full vocabulary

### State — exactly one per triaged issue

| Label | Meaning |
| --- | --- |
| `status:needs-triage` | Never evaluated, or returned for re-evaluation |
| `status:needs-info` | Blocked on the reporter |
| `status:ready-for-agent` | Agent brief attached; delegate it |
| `status:ready-for-human` | Ready for a human to implement or merge |
| `status:wontfix` | Rejected or already implemented; issue is closed |

### Type — exactly one per triaged issue

| Label | Meaning |
| --- | --- |
| `type:bug` | Defect in existing behavior |
| `type:feature` | New capability |
| `type:performance` | Faster or leaner without behavior change |
| `type:refactor` | Structure or maintainability change, same behavior |
| `type:docs` | Documentation only |
| `type:test` | Test coverage or test infrastructure |
| `type:chore` | Build, tooling, dependency, or housekeeping |
| `type:security` | Vulnerability or hardening work |

### Priority — exactly one per triaged issue

| Label | Meaning |
| --- | --- |
| `priority:high` | Breaks core use or blocks others; act next |
| `priority:medium` | Should be scheduled |
| `priority:low` | Nice to have; no urgency |

### Size — optional, at most one

| Label | Meaning |
| --- | --- |
| `size:small` | Single focused change |
| `size:medium` | Touches a few files or one subsystem |
| `size:large` | Cross-cutting; split before delegating |

### Modifiers — zero or more

| Label | Meaning |
| --- | --- |
| `needs-repro` | Claim unverified; reproduce before triage proceeds |
| `needs-design` | Shape is undecided; grill or design first |
| `blocked` | Waiting on another issue or an external dependency |
| `good-first-issue` | Low-context entry point for a new contributor |

## Invariants

- A triaged issue carries exactly one `status:*`.
- A triaged issue carries exactly one `type:*`.
- A triaged issue carries exactly one `priority:*` (unless it is `status:needs-triage`, where priority may be unset).
- At most one `size:*`.
- Modifiers are independent; `blocked` should name its blocker in the issue body.
- `type:bug` is the `bug` category; every other `type:*` is the `enhancement` category.
