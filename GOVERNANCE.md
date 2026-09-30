# Governance

Cairn Linux is an independent open-source project. It is not a company,
foundation, nonprofit, or other incorporated organisation.

This file records how the project is run while it is small. It can change as
contributors and maintainers join, but changes to stewardship will be recorded
in this repository.

## Project lead

Mason Ball founded Cairn Linux and is its lead maintainer.

The project lead has final responsibility for:

- product direction and scope;
- release readiness and publication;
- safety, privacy, and security requirements;
- use of the Cairn Linux name and stacked-stones mark;
- appointing or removing maintainers; and
- changes to this governance document.

Specific decisions and repository access may be delegated to other
maintainers. Mason remains project lead unless a successor is recorded here.

## How decisions are made

Routine changes are discussed and reviewed through GitHub issues and pull
requests.

`docs/DESIGN.md` is authoritative for the intended experience and
architecture. A change that alters the design needs an architecture decision
record in `docs/decisions/` and the matching design edit in the same pull
request. `docs/ROADMAP.md` records the current plan and verified progress.

Decisions should be based on tests, research, and user needs rather than a
simple count of comments. Child safety, privacy, recovery, and
privilege-boundary changes need evidence appropriate to their risk. The
project lead makes the final call when there is no consensus.

## Maintainers

There is currently one maintainer: Mason Ball.

A future maintainer may be given review, merge, release, or repository
administration rights for a defined area. Appointments and departures will be
recorded here or in a linked public decision record.

## Contributions and copyright

Code contributions are accepted under the Apache License 2.0. Documentation
and brand assets, including the SVG artwork for the stacked-stones mark, are
accepted under CC BY-SA 4.0 unless a file states another approved licence.

Contributors keep the copyright they hold in their contributions. Accepted
work is distributed under the applicable project licence. The Developer
Certificate of Origin sign-off confirms that the contributor has the right to
submit the work under those terms; it is not a copyright assignment.

The copyright licences permit forks and reuse. They do not grant trademark
rights in the Cairn Linux name or stacked-stones mark. See
[`TRADEMARKS.md`](TRADEMARKS.md).

## AI-assisted work

Cairn uses AI agents extensively for implementation, tests, documentation, and
research assistance. Mason sets the requirements, approves scope and design
decisions, and is responsible for accepting the resulting work.

An AI agent cannot provide a Developer Certificate of Origin sign-off. The
human submitting a commit must check its provenance well enough to make the
DCO certification. An agent's commits therefore carry no `Signed-off-by`
line; the maintainer squash-merges the pull request and adds their sign-off
to the squash commit's message, so the commit that lands on `main` carries a
human certification. Separately, Cairn requires submitted work to be
reviewed, understood, and tested in proportion to its risk before it is
accepted.

## Project name, marks, and accounts

Mason Ball owns the Cairn Linux project name and stacked-stones mark and may
approve uses under [`TRADEMARKS.md`](TRADEMARKS.md). He also controls the
project domains, GitHub organisation, release credentials, and social
accounts.

Those assets are separate from contributor copyrights and the rights granted
by the open-source licences.

## Future stewardship

Cairn may later form or join a legal organisation if funding, contracts,
liability, or community governance justify it. That has not happened yet.

Mason may appoint another project lead. Any transfer of marks, domains, account
control, or other legally owned assets will be handled separately and recorded
in the repository.
