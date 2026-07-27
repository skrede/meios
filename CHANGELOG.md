# Changelog

All notable, user-facing changes to meios are recorded here. The format follows the conventions of
[Keep a Changelog](https://keepachangelog.com/). meios is in public preview: expect breaking
changes onwards to a stable `v1.0.0` release.

## Unreleased

### Added

- `load()` now hands back the resolved robot model through an error channel: a consumer receives
  either a fully resolved `model<double>` or a typed `file:line` diagnostic, never a half-built
  model to guess about.
- The resolved model carries its topology. Link parentage and a root-first traversal order are
  exposed to the consumer rather than being discarded during resolution.
- A documented consumer surface. The exported, installed `meios::urdf` target — the one a newcomer
  links to reach the public API — is now spelled out for both the FetchContent and `find_package`
  integration paths.
- An unrestricted Python evaluator, for a consumer who wants the wider expression surface
  deliberately. It is a separate class reachable only from code: the command-line tool never
  constructs it and carries no flag for it.

### Changed

- The library is silent by default. Diagnostics travel through a caller-installed sink instead of a
  stream the library chose; a program that installs no sink emits nothing on its own.
- A consumer can collect diagnostics straight into its own record type. The diagnostic sink accepts
  a caller-defined type that receives each `(level, code, message)`, so records land in the
  consumer's representation without an intermediate format.
- Project status moved to public preview, with the README documenting the exported consumer target
  and the integration path a first-time adopter follows.
- The Python evaluation enrichment now restricts what a description's expressions can reach. An
  expression that reaches past the supported subset — the import machinery, the filesystem, the
  process — is refused with a located diagnostic naming the rule that refused it, instead of being
  executed. `docs/evaluation.md` states the subset, the refusal rules, and where they diverge from
  canonical xacro.
- An auxiliary configuration file loaded from an expression now resolves through the same source
  stack as every other asset. A `package://` or `$(find)` spec resolves the way a mesh does, a
  relative spec resolves against the document it is written in, a source layer that serves bytes
  rather than a path works, and a spec that resolves outside every containment root is refused.

### Fixed

- The command-line tool's help text now describes what the `resolve` and `deps` commands actually
  accept — `resolve` reads a `package://<pkg>/<rel>` reference and fails loudly on anything else —
  and `deps` gained the `key:=value` property override and the evaluation flag its behavior always
  implied.
- A refused expression is no longer softened by a lenient evaluation policy, and no longer leaves the
  unevaluated expression behind in the flattened output.
