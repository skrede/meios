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

### Changed

- The library is silent by default. Diagnostics travel through a caller-installed sink instead of a
  stream the library chose; a program that installs no sink emits nothing on its own.
- A consumer can collect diagnostics straight into its own record type. The diagnostic sink accepts
  a caller-defined type that receives each `(level, code, message)`, so records land in the
  consumer's representation without an intermediate format.
- Project status moved to public preview, with the README documenting the exported consumer target
  and the integration path a first-time adopter follows.

### Fixed

- The command-line tool's help text now describes what the `resolve` and `deps` commands actually
  accept — `resolve` reads a `package://<pkg>/<rel>` reference and fails loudly on anything else —
  and `deps` gained the `key:=value` property override and the evaluation flag its behavior always
  implied.
