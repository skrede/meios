# meios documentation

Start here. This page routes you to the right tier before you read a line of the guides — pick how
you consume the library, then follow the one guide that matches.

## Which API tier is yours?

meios has two tiers. You are on exactly one of them; read only its guide.

**Use the consumer tier if you want the resolved robot and nothing else.** You call `load()`, get an
`expected<load_result, load_error>` back, and read the flattened links, joints, materials, and
topology out of the result's `robot`. This is the tier for a tool that inspects a description, a converter, or
any consumer that is happy to hold the model in meios's own `model` type. Read the
[consumer guide](consumer-guide.md).

**Use the engine tier if you want the resolved robot pushed straight into your own types.** You
declare a struct that satisfies the `model_sink` concept and meios drives it — one call per robot,
material, link, and joint — so the description lands directly in your scene graph, your solver's
bodies, or your renderer's nodes with no intermediate `model` to copy out of. Injecting a `log_sink`
alongside it captures typed diagnostics into your own record type. Read the
[engine guide](engine-guide.md).

If you are unsure: start on the consumer tier. It is the smaller surface, and every engine-tier
program can fall back to reading a `model` first. The engine tier pays off once copying out of
`model` into your own representation is the thing you were going to do anyway.

## Where the build wiring lives

Both tiers link the same target, and how you acquire it — built inside your own configure, or found
as an installed package — is one page: [CMake integration](cmake-integration.md). It owns which
target to link and which not to reach for, both acquisition recipes, every option and what it
defaults to, and what an installed package carries. This page does not repeat any of that. A second
copy would be a second thing to keep true, and the two would drift.

If you have not built against meios before, [Getting started](getting-started.md) is the shorter
road. It goes from an empty directory to a running program without asking you to know what a flag
does; come to the reference when you need one.

If something has already gone wrong — a diagnostic code in hand, or a configure that stopped —
[Troubleshooting](troubleshooting.md) is keyed on what you are looking at rather than on what you
were trying to do.

## Guides

- [Getting started](getting-started.md) — from an empty directory to a program that loads a
  description and prints what is in it, assuming no prior knowledge of the library.
- [Consumer guide](consumer-guide.md) — `load()` a description and read the resolved `model`.
- [Engine guide](engine-guide.md) — receive the resolved robot into your own types via `model_sink`.
- [CMake integration](cmake-integration.md) — the build reference: the target you link, both
  acquisition paths, every option and its default, and what an installed package carries and what it
  withholds.
- [Resource guide](resources-guide.md) — acquire a description package in CMake, deploy it where
  your program looks for it, and flatten one description to a file when something downstream needs
  a plain URDF, instead of vendoring the tree into your repository.
- [Evaluation](evaluation.md) — what a description's expressions may run, the rules that refuse the
  rest, and how the resource helper resolves a file.
- [Asset resolution](asset-resolution.md) — what a mesh or texture URI may name: the accepted forms,
  what a relative path is measured against, the containment rule, how long a resolved path is valid,
  what happens when an asset is absent, and what a written document carries.
- [URDF profile](urdf-profile.md) — the expanded URDF document meios reads: every rule, the
  diagnostic code each refusal carries, and the frame and unit conventions the numbers follow. It
  starts where expansion ends, so what a `${…}` may evaluate to is not here — that is
  [Evaluation](evaluation.md).
- [Troubleshooting](troubleshooting.md) — keyed on the diagnostic code you were handed or the line
  your configure stopped on: what each one means, what to do about it, and which guide owns the rule
  behind it.
- [Known limitations](known-limitations.md) — every defect and caveat live at this point in the
  library's life, described by its user-facing effect.
- [Description corpus survey](corpus-survey.md) — the record of which public third-party robot
  descriptions were examined for the evaluator constructs no pinned description exercises: what each
  candidate contributes, at which revision and under which license, what was rejected and why, which
  constructs no maintained carrier was found for, and what each candidate did when this project's own
  loader was pointed at it.
