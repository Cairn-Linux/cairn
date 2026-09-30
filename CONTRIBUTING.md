# Contributing

Cairn Linux is in Phase 0. The launcher, session, greeter, Footpath terminal
integration, provisioning tools, and test fixtures are under active
development, but there is no release for everyday family use. Read
[`docs/DESIGN.md`](docs/DESIGN.md) first and [`docs/ROADMAP.md`](docs/ROADMAP.md)
second. The design document is authoritative; propose changes to it as an
ADR in `docs/decisions/` plus the edit, in one pull request.

[`GOVERNANCE.md`](GOVERNANCE.md) records how decisions are made and who is
responsible for accepting work.

## Where help is most useful right now

- **Reading the design as a parent, teacher, accessibility specialist, or
  security reviewer** and opening an issue where it is unclear, over-promises,
  or hides a caveat.
- **Running a named roadmap check** on supported x86_64 hardware and recording
  enough detail for someone else to repeat it.
- **Working an open issue** whose acceptance criteria are already written.
  Comment before starting so two people do not solve the same problem.
- **Testing the current child-facing flow** without widening Phase 0 into an
  installer or general-purpose distribution.

## Rules

- **Licence.** Code is Apache-2.0; docs and brand assets, including the mark
  artwork, are CC BY-SA 4.0. By contributing you agree your contribution is
  under the same terms. Those copyright licences do not grant trademark rights
  in the project's name or mark (see [`TRADEMARKS.md`](TRADEMARKS.md)).
- **Sign your commits.** Every commit carries a Developer Certificate of
  Origin sign-off, which `git commit -s` adds:

  ```
  Signed-off-by: Your Name <you@example.com>
  ```

  This certifies the text at https://developercertificate.org: you have the
  right to submit the contribution under the project's licence. It does not
  assign your copyright to the project lead.
- **Source files start with** `// SPDX-License-Identifier: Apache-2.0`.
- **Conventions** for C++, QML, scripts and docs are in [`CLAUDE.md`](CLAUDE.md).
  They apply to humans too.
- **Voice.** Anything a child or parent will read follows the brand guide's
  voice: calm, real words, no baby talk, limitations stated plainly.
- **AI-assisted work still needs a human signer.** An AI agent cannot make the
  DCO certification. The person signing the commit must verify that they have
  the right to submit it. Cairn separately requires a human to understand and
  review the change, its tests, and its risks before it can be accepted.

## Nothing phones home

No contribution may add telemetry, analytics, accounts, or network calls the
user did not ask for. This is not negotiable and is not a matter of defaults.
