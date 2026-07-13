# Contributing to meios

Thanks for your interest in contributing. meios is a dependency-light C++20
library that reads, resolves, and flattens URDF/xacro across packages and pushes
the resolved robot model straight into a consumer's own types -- without an
intermediate serialization format. Contributions of all sizes -- documentation
fixes, bug reports, code improvements, new features -- are welcome.

## Quick Links

- [Filing an Issue](#filing-an-issue)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Building and Testing](#building-and-testing)
- [Coding Conventions](#coding-conventions)
- [Commit Message Format](#commit-message-format)
- [Branching Model](#branching-model)
- [License](#license)

## Filing an Issue

- **Bug reports.** Include a minimal reproducer: the smallest URDF/xacro
  document (plus any `package://` / `$(find ...)` files it references) that
  triggers the issue, the compiler + version (`g++ --version` or equivalent),
  the CMake version, and the platform. Report the observed resolved model or
  diagnostic against the expected one; for a bad or missing diagnostic, quote
  the `file:line` meios emitted (or should have emitted).
- **Feature requests.** Describe the use case first, the proposed API shape
  second. Concrete user stories make it easier to evaluate the proposal against
  the library's scope -- meios owns URDF/xacro *semantics*; the consumer owns
  rendering and kinematics.
- **Questions.** Open an issue tagged "question" -- there is no Discord or Slack
  to fragment discussion.

If you're not sure whether something is a bug or expected behavior, an issue is
the right place to ask.

## Submitting a Pull Request

1. **Fork** the repository and create a feature branch from `develop` (or from
   the active `milestone/<version>` branch if you're contributing to in-progress
   milestone work -- ask first if unsure).
2. **Build and test** locally. The full ctest suite should pass
   (`ctest --output-on-failure` in your build directory) before you submit. New
   features need new tests; new tests need to pass.
3. **Match the coding conventions.** See [Coding Conventions](#coding-conventions)
   below. PRs that diverge stylistically will be asked to converge before
   review; this saves everyone time.
4. **Write a clear PR description.** What problem does this solve? What approach
   did you take? What alternatives did you consider? Link relevant issues.
5. **One logical change per PR.** Don't bundle unrelated cleanups with feature
   work. If you spot an unrelated issue while working on something else, file it
   separately.

## Building and Testing

See the [README](README.md) for the install and build instructions for end
users. For contributors:

```bash
# Configure the dev preset (Debug + tests).
cmake --preset dev

# Build everything.
cmake --build build/dev -j$(nproc)

# Run the test suite.
ctest --test-dir build/dev --output-on-failure
```

Dependencies resolve via FetchContent with a `find_package` fallback. The
`MEIOS_CMAKE_FETCH_DEPS` toggle controls the acquisition policy: `ON` fetches
pinned sources, `OFF` requires the dependencies to be discoverable on the
system. Tests build under `-Werror` by default (`MEIOS_WERROR` is on for a
top-level build and off for FetchContent consumers); property tests are gated
behind `MEIOS_BUILD_PROPERTY_TESTS`.

## Coding Conventions

meios targets idiomatic, cross-platform C++20 (macOS, Linux, Windows).
`CONVENTIONS.md` is the authoritative, full style spec; `.clang-format` owns the
mechanical rules -- run it before submitting. The essentials below are the rules
a contribution must not get wrong.

### Language and style

- **C++20** -- use modern features where they improve clarity: `concepts`,
  `std::ranges`, `if constexpr`, designated initializers, `std::string_view`,
  `std::filesystem`. Reach for a feature where it makes the code clearer, not
  for novelty. Platform-specific code is isolated in backends, never in the core.
- **pugixml is the only core dependency.** Every enrichment is a separate CMake
  target carrying at most one extra dependency, and every declared dependency
  must be FetchContent-able. Python is the single nuance: a *found* environment
  resource (`find_package(Python3)`), never fetched.
- **Namespaces:** a flat `namespace meios`, with `meios::detail` for internals.
- **Naming:** lowercase types and functions (`model_sink`, `source_stack`,
  `pod_recorder`); template parameters in `CamelCase` (`Scalar`, `Sink`); a
  member holding an injected callback carries a `_cb` suffix and groups with the
  other callbacks.
- **Return types:** always spelled out -- never `auto` as a function's return
  type (the only exception is a genuinely unspellable lambda/closure). `auto`
  for a local is fine where the initializer makes the type obvious.
- **No `[[nodiscard]]`** and no decorative attributes the compiler does not
  require.
- **Construction:** initialize members in the constructor's init list, in
  member-declaration order -- never in-declaration.
- **File / function size:** functions 5-15 lines (25 hard ceiling), files ~100
  lines (200 hard ceiling), comments and blanks included. Over the ceiling means
  the "one thing" is too broad -- decompose it. The only sanctioned over-limit
  units are listed in `EXCEPTIONS.md`.

### Headers

- **Header guards** in the form `HPP_GUARD_MEIOS_<FOLDER>_<FILE>_H`. No
  `#pragma once`.
- **No matching closing-namespace comment** (`// namespace meios` is noise after
  the closing brace; just close the brace).
- **No matching `#endif` comment** for the include guard.

### Include ordering

Includes are grouped into three sections separated by a single blank line:

1. Internal project includes (`#include "..."`), the file's own subsystem first.
2. Third-party library includes (`#include <pugixml.hpp>`, `#include <catch2/...>`,
   etc.).
3. Standard library includes (`#include <vector>`, `#include <string_view>`).

Within each section, group by folder location with a blank line between folder
groups. Within each folder group, sort by file-name length first, then
alphabetically.

### Comments

The codebase is written for people to read. A comment must say something the
code itself cannot; the default is no comment. Write one only to cite a source
(paper, RFC, spec, URL), name a non-obvious algorithm so a reader can look it
up, or explain a genuinely non-obvious design choice, contract, or invariant in
a sentence or two. Delete comments that restate a name, signature, or obvious
control flow.

### Testing

- **Unit tests** via Catch2: cover every public API path.
- **Property-based tests** via RapidCheck: for the xacro expression evaluator
  and the URDF reader, property tests catch what handpicked unit tests miss.
- **Compile-fail tests** (`compile/fail/`): ensure misuse is rejected at compile
  time.
- **Fuzz harnesses** for the parsing surfaces (xacro expression parser, URDF
  reader) are encouraged for changes to those paths.

## Commit Message Format

```
{Prefix}: {summary sentence}.

- {what was done; one bullet per logical item}
- {another item if applicable}
```

Allowed prefixes:

| Prefix          | When to use |
|-----------------|-------------|
| `Feature:`      | New user-visible capability or API surface. |
| `Fix:`          | Bug fix or correctness-affecting change. |
| `Refactor:`     | Internal reorganization with no user-visible behavior change. |
| `Build:`        | Build system, packaging, dependency, or CI changes. |
| `Docs:`         | Documentation-only changes (README, docs, doc-comments). |
| `Examples:`     | Changes to `examples/` only. |
| `Optimization:` | Performance change with no API or correctness impact. |
| `Test:`         | Test-only changes (new or updated tests, test harness). |
| `WIP:`          | Work-in-progress commits whose code does not yet compile. Use sparingly. |

The summary line is brief and descriptive. The bullet list expands on the what;
single-item commits may omit it. Do not reference issue trackers, planning
artifacts, or task IDs in commit messages, and do not add `Co-Authored-By`
lines.

**One logical change per commit.** A tightly-scoped commit gets reviewed and
merged faster than one bundling multiple changes.

## Branching Model

```
master   <- develop   <- milestone/<version>
(releases)  (integration)  (active work)
```

- **master** holds releases.
- **develop** is the integration branch; milestone branches merge into develop
  when each milestone ships. `develop` is never deleted.
- **milestone/v0.X.0** branches host active work on a specific milestone.
  External contributors should normally branch from `develop`; if you're
  contributing to in-progress milestone work, ask first via an issue or PR
  comment.
- Merge direction is always milestone -> develop -> master. Reverse merges
  (master -> develop, etc.) do not happen.

## License

meios is released under the [Apache License 2.0](LICENSE). By submitting a
contribution you agree that your contribution is licensed under the same terms.
There is no separate Contributor License Agreement (CLA); the Apache 2.0 grant
in the standard "Submission of Contributions" clause (Section 5) applies.
