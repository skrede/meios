# meios

![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Status](https://img.shields.io/badge/status-early%20development-red.svg)

A dependency-light C++20 library that reads, resolves, and flattens URDF/xacro across packages
and hands the resolved robot model straight to a consumer's own types — no intermediate
serialization format.

## Status

**Early development.** meios is being built from the ground up; the API is unstable and much of
the library is not yet implemented. This README describes the library's intent and scope rather
than a finished feature set. Expect breaking changes onwards to a stable release. Follow the
milestone branches for work in progress.

## Features

meios aims to be a first-class URDF/xacro engine that owns URDF/xacro *semantics* as a primary
concern while the consumer owns rendering and kinematics:

- **Resolve-and-push:** hand a consumer a correct, fully resolved robot model — xacro expanded,
  `package://` / `$(find)` resolved, a global material table, un-baked origins, resolved mesh
  paths — pushed straight into its own representation.
- **Loud diagnostics:** typed `file:line` diagnostics with never a silent failure.
- **Dependency-light core:** pugixml is the one core dependency; every enrichment is a separate,
  opt-in target carrying at most one extra dependency.
- **Cross-platform:** idiomatic C++20 targeting macOS, Linux, and Windows; platform-specific code
  stays isolated in backends, never the core.

## Scope

meios owns URDF/xacro reading, resolution, and flattening — and deliberately only that. It stays a
small, composable library rather than a robotics framework, so the following are **non-goals**,
each better served by a purpose-built tool meios composes with:

- **Kinematics / dynamics** — the resolved model feeds the consumer's own representation and solver.
- **Rendering / visualization** — the consumer owns its scene and renderer.
- **A serialization round-trip** — meios pushes the resolved model directly into consumer types
  rather than emitting an intermediate format.

The guiding principle is *library, not framework*: meios owns URDF/xacro semantics and stays out
of everything else.

## Requirements

- C++20 compiler: GCC 14+, Clang 18+, MSVC 19.38+
- CMake 3.28+
- pugixml (auto-fetched via FetchContent, or found on the system)

## Quick Install

### CMake FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
    meios
    GIT_REPOSITORY https://github.com/skrede/meios.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(meios)

target_link_libraries(my_app PRIVATE meios::core)
```

### find_package

```cmake
find_package(meios CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE meios::core)
```

## Documentation

Documentation is under construction and will be published as the library takes shape.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the contribution workflow, coding conventions, and
commit message format.

## License

Apache License 2.0 — see [LICENSE](LICENSE) for the full text.

Copyright 2026 Aleksander Skrede.

## Declaration of AI use

This library has been — and will be — developed with extensive use of Claude Code (Sonnet, Opus
and Fable). Development through Claude has been spec-driven using
[GSD](https://github.com/open-gsd/gsd-core).
