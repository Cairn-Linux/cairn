# Architecture decision records

One file per decision, numbered, never deleted. A superseded ADR gets a
"Superseded by" line at the top and stays put.

`DESIGN.md` records the decisions made before this repository existed. ADRs
record everything after, including the D-numbers in `ROADMAP.md` §1 when they
close.

## Template

```markdown
# ADR-NNNN: Title

**Status:** proposed | accepted | amended by ADR-MMMM | superseded by ADR-MMMM
**Date:** YYYY-MM-DD
**Closes:** ROADMAP D-n (if applicable)

## Context
What forced the decision. Link the DESIGN.md sections it touches.

## Decision
One paragraph. What we will do.

## Consequences
What gets easier, what gets harder, what we gave up.
```
