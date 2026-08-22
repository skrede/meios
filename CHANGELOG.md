# Changelog

All notable, user-facing changes to meios are recorded here. The format follows the conventions of
[Keep a Changelog](https://keepachangelog.com/). meios is in public preview: expect breaking
changes onwards to a stable `v1.0.0` release.

## v0.2.2

### Added

- `load()` now hands back the resolved robot model through an error channel: a consumer receives
  either a fully resolved `model<double>` or a typed `file:line` diagnostic, never a half-built
  model to guess about. Both arms carry every diagnostic the document raised, in source order, so a
  failed load reports the whole document rather than the first problem in it.
- A result states what it is claiming. Three independent claims travel with it: that the document
  was parsed whole, that its topology reconstructed, and that every asset it references resolved.
  They are facts about different things rather than a ranking, and a load may hold any combination —
  a description can be parsed whole and still describe a topology meios refuses. Branch on the
  claims instead of on the presence of a value.
- `load_into` drives your own `model_sink` straight from a path on disk. The sink is reached only
  after the load has succeeded, so a failed load leaves it untouched and there is no rollback
  contract to honor. It is the engine tier without the two-step.
- A published URDF profile. `docs/urdf-profile.md` states every rule the reader enforces, the
  diagnostic code each refusal carries, the frame and unit conventions the numbers follow, and every
  place the profile decides something the specification is silent about or departs from something it
  states. A rotation reference file ships alongside it with worked values a consumer can check its
  own implementation against.
- The resolved model carries its topology. Link parentage and a root-first traversal order are
  exposed to the consumer rather than being discarded during resolution.
- A documented consumer surface. The exported, installed `meios::urdf` target — the one a newcomer
  links to reach the public API — is now spelled out for both the FetchContent and `find_package`
  integration paths.
- An unrestricted Python evaluator, for a consumer who wants the wider expression surface
  deliberately. It is a separate class reachable only from code: the command-line tool never
  constructs it and carries no flag for it.

### Changed

- **Breaking.** Expanding or substituting a xacro document directly now hands back either the
  finished result or a typed diagnostic, never a success flag beside a half-built document. On the
  way out you get the expanded document — or the substituted text — and nothing else, or a
  diagnostic naming the `file:line` that failed, the code for what failed there, and the underlying
  system cause where the failure came from one. That diagnostic is reported once through the sink
  you supplied and handed to you as the failure arm, so there is nothing to log twice and nothing to
  reconstruct from message text; warnings and every other non-fatal diagnostic keep travelling the
  sink as before. An expression left alone by a permissive evaluation policy is still a success with
  the span untouched. A failed `<xacro:include>` read also now names what went wrong — a missing
  file, a directory where a file was expected, a read that failed part-way — and the operating
  system's own reason for it, instead of reporting only that something could not be read. Callers
  branch on the result; `docs/evaluation.md` states the contract in full.
- **Breaking.** A description that says something meios cannot read is now refused instead of
  quietly completed. A two- or four-component offset, an unparseable or non-finite number, a missing
  mass or inertia component, a misspelled or absent joint type, an unrecognized geometry shape, a
  bounded joint with no limit, a blank or duplicated name, a joint naming a link that was never
  declared, a mimic naming a joint that does not exist, a joint told to turn about a zero axis, and
  an inertia tensor that no rigid body could have: each of these used to load clean and produce a
  robot the document never described. Each is now a refusal with a code and a `file:line`. If a
  description of yours stops loading, `docs/urdf-profile.md` names the rule and why it exists.
- **Breaking.** The document-validity policy is now `fail`, `warn` and `skip`, matching its three
  siblings, and it defaults to `fail`. It also governs a wider class than before: rules about what a
  document may say answer to it, while identity and reference integrity are refused at every setting
  and cannot be softened. The graph policy is correspondingly narrower — it governs root count,
  cycles, reachability and multiple parents, and no longer reopens a dangling link reference.
- **Breaking.** An unresolved `package://` or `$(find)` asset now fails the load by default. Lower
  `on_missing` to `warn` and the load succeeds, withholds its deployment claim, and names every
  asset that failed, one diagnostic each.
- The library is silent by default. Diagnostics travel through a caller-installed sink instead of a
  stream the library chose; a program that installs no sink emits nothing on its own, and reads the
  same diagnostics off the result afterwards.
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
- A refused expression is no longer softened by a permissive evaluation policy, and no longer leaves
  the unevaluated expression behind in the flattened output.
