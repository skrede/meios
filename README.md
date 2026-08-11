# meios
[![Linux](https://github.com/skrede/meios/actions/workflows/linux.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/linux.yml)
[![macOS](https://github.com/skrede/meios/actions/workflows/macos.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/macos.yml)
[![Windows](https://github.com/skrede/meios/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/windows.yml)
[![codecov](https://codecov.io/gh/skrede/meios/branch/master/graph/badge.svg)](https://codecov.io/gh/skrede/meios)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Status](https://img.shields.io/badge/status-public%20preview-orange.svg)](#status)

**meios** is a dependency-light C++20 library that reads, resolves, and flattens URDF/xacro across
packages and hands the resolved robot model to your own code — no intermediate serialization format.
xacro is expanded in process, `package://` and `$(find)` are resolved through a layered stack of
package sources, and every refusal comes back as a typed `file:line` diagnostic.

There are two ways to take delivery. Call `load()` and read the flattened links, joints, materials,
and topology out of meios's own `model`. Or declare a type satisfying the `model_sink` concept and
meios drives the description straight into your scene graph, your solver's bodies, or your renderer's
nodes — one call per robot, material, link, and joint, with no intermediate to copy out of. That
second path is atomic: the sink is driven only after the model is built, so a failed load leaves your
types exactly as they were, neither partially filled nor filled and rolled back.

## Status

**Public preview.** Expect breaking changes onwards to a stable `v1.0.0` release. Follow the
milestone branches for work in progress. Every defect and caveat live right now is written down in
[known limitations](docs/known-limitations.md), described by the effect you will see rather than by
the work that would close it.

## Features

- **Two consumption tiers:** `load()` returns an `expected<load_result, load_error>` carrying the
  resolved `model`; `load_into()` pushes the same description into a type of your own that satisfies
  `model_sink`. Both link one target and both hand back the same diagnostics.
- **xacro expanded in process:** properties, arguments, macros, includes, and `xacro:if`/`xacro:unless`
  are resolved by a built-in evaluator that needs nothing outside the C++ standard library and cannot
  reach the filesystem, the network, or the process at all. Comprehensions, f-strings, `math`, and
  `load_yaml` come from an opt-in enrichment driving a *found* (never fetched) interpreter,
  restricted by default to a [documented subset](docs/evaluation.md) whose refusals name the rule
  that refused them.
- **Layered package resolution:** `package://` and `$(find)` resolve through an ordered stack of
  package sources — a directory, a ROS package layout, an in-memory tree, a bundle. A
  lower-precedence layer that could also have answered is reported as a shadow diagnostic instead of
  being silently hidden.
- **Typed diagnostics, never a silent failure:** every diagnostic carries a `file:line:column`
  location and a `diagnostic_code`, so a caller branches on a code rather than matching message
  strings. Both arms of the result carry the full ordered list.
- **Completeness claims:** a successful load also reports what it established — whether the document
  parsed, whether the topology holds, whether every referenced asset resolved. Branch on `has()`
  rather than counting warnings.
- **Flatten and bundle:** write one expanded description back out as a plain URDF, or collect a
  description together with every asset it references into a self-contained folder — or a `.zip`,
  with entry names confined to the archive root so extraction cannot escape it.
- **Asset scanners:** Wavefront `.obj` (every `mtllib` and `map_*` reference), COLLADA, glTF and GLB,
  and STL — each a separate opt-in target, each returning references verbatim so the bundle closure
  re-anchors them against the referring package.
- **Descriptions as fetched data:** `meios_declare_resource` pins a description package by hash and
  `meios_target_deploy_resources` deploys it beside the executable that loads it, instead of
  vendoring the tree into your repository.
- **Dependency-light:** pugixml is the one dependency in the core, linked privately so it never
  reaches your include path. Every enrichment is a separate target carrying at most one extra
  dependency, and each is FetchContent-able.
- **Cross-platform C++20:** Linux, macOS, and Windows, each with its own CI leg, plus sanitizer,
  coverage, fuzzing, and clang-tidy jobs.

## Modules

Link `meios::urdf`. It carries the public API and pulls in everything below it.

| Target | Carries | Built |
|--------|---------|-------|
| `meios::urdf` | **The public API** — `load`, `load_into`, the URDF reader, the strictness policies | always |
| `meios::core` | The compiled nucleus and the single private pugixml edge. It carries **no reader**: linking it reaching for the API is the wrong target | always |
| `meios::model` | The records — links, joints, materials, topology — plus diagnostic codes, source locations, and completeness claims | always |
| `meios::io` | Package sources, the ordered source stack, resolved assets | always |
| `meios::xacro` | Structural expansion, substitution, and the evaluator seam | always |
| `meios::bundle` | Flatten one description to a URDF; write a description and its assets to a folder | always |
| `meios::completion` | The command table the CLI's parser and its shell completions are both generated from | always |

The enrichments are separate targets behind separate options. One is on by default; the rest are
**off**, so a build that has not turned them on will not link them:

| Target | Carries | Default | Option |
|--------|---------|---------|--------|
| `meios::ros-package` | `package://` resolution through ROS package manifests | **ON** | `MEIOS_ROS_PACKAGE_SUPPORT` |
| `meios::scan-obj` | Wavefront `.obj` reference scanning | off | `MEIOS_SCAN_OBJ_SUPPORT` |
| `meios::scan-stl` | STL reference scanning (a typed no-op: STL references nothing) | off | `MEIOS_SCAN_STL_SUPPORT` |
| `meios::scan-collada` | COLLADA reference scanning | off | `MEIOS_SCAN_COLLADA_SUPPORT` |
| `meios::scan-gltf` | glTF and GLB reference scanning | off | `MEIOS_SCAN_GLTF_SUPPORT` |
| `meios::archive-zip` | Bundling into a single `.zip` archive | off | `MEIOS_ARCHIVE_ZIP_SUPPORT` |
| `meios::eval-python` | Python-backed expression evaluation against a found interpreter | off | `MEIOS_EVAL_PYTHON_SUPPORT` |

`MEIOS_BUILD_TOOLS` additionally builds the `meios` command-line tool, which inspects, flattens, and
bundles a description without writing a program.

## Scope

meios owns URDF/xacro reading, resolution, and flattening — and deliberately only that. It stays a
small, composable library rather than a robotics framework, so the following are **non-goals**, each
better served by a purpose-built tool meios composes with:

- **Kinematics and Lie groups** — see [cartan](https://github.com/skrede/cartan); the resolved model
  feeds its `SE3`/`SO3` types directly.
- **Dynamics** (RNEA, mass matrix, gravity/Coriolis) — use
  [Pinocchio](https://github.com/stack-of-tasks/pinocchio).
- **Control and estimation** — see [ctrlpp](https://github.com/skrede/ctrlpp).
- **Optimization and inverse-kinematics solving** — the solvers are
  [cartan](https://github.com/skrede/cartan)'s, over
  [argmin](https://github.com/skrede/argmin) underneath.
- **Rendering and visualization** — use [threepp](https://github.com/markaren/threepp) or your own
  renderer; meios hands you the model, you own the scene.
- **A serialization round-trip** — meios pushes the resolved model into consumer types rather than
  emitting an intermediate format to read back. It writes a plain URDF and a bundle; it does not
  define a format of its own.
- **Mesh and geometry decoding** — a scanner reads which files a mesh references, never its
  triangles.

The guiding principle is *library, not framework*: meios owns URDF/xacro semantics and stays out of
everything else.

## Requirements

- C++20 compiler: GCC 14+, Clang 18+, MSVC 19.38+
- CMake 3.28+
- pugixml (found on the system, or auto-fetched via FetchContent)

## Quick Install

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    meios
    GIT_REPOSITORY https://github.com/skrede/meios.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(meios)

target_link_libraries(my_app PRIVATE meios::urdf)
```

### find_package

```cmake
find_package(meios CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE meios::urdf)
```

Everything past these two blocks — every option and what it defaults to, how dependencies are
acquired, and what an installed package carries — is in
[CMake integration](docs/cmake-integration.md).

### Acquiring a robot description

Either integration also brings two CMake functions for treating a description package as fetched
data rather than vendored files — pin it by hash, then deploy it beside the executable that loads
it:

```cmake
meios_declare_resource(NAME kuka GITHUB ros-industrial/kuka_experimental REF melodic-devel)

meios_target_deploy_resources(my_app RESOURCES kuka SUBDIR urdf)
```

Configure once and meios reports the archive's `SHA256` so you can pin it; pinned fetches are
reproducible and cached.

See the [resource guide](docs/resources-guide.md) for the acquisition modes, offline configures, and
how the deployed directory maps onto `package://` resolution.

## Quick Start

Load a description and read the resolved model. On failure the error carries a location, a
diagnostic code, and the full diagnostic list — not a message on `stderr`.

<!-- meios:snippet name=quick-start tu -->
```cpp
#include <meios/urdf.h>
#include <meios/model.h>

#include <cstddef>
#include <iostream>

int main()
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load("examples/robot.urdf.xacro");
    if(!loaded)
    {
        const meios::load_error &failure = loaded.error();
        std::cout << to_string(failure.loc) << ": " << to_string(failure.code) << ": "
                  << failure.message << '\n';
        return 1;
    }

    const meios::model<double> &robot = loaded->robot;
    std::cout << robot.name << ": " << robot.links.size() << " links, "
              << robot.joints.size() << " joints\n";

    std::cout << "root-first:";
    for(const int index : robot.topo.order)
        std::cout << ' ' << robot.links[static_cast<std::size_t>(index)].name;
    std::cout << '\n';

    const int edge = robot.joint_index.at("base_to_upper");
    std::cout << "base_to_upper origin z = "
              << robot.joints[static_cast<std::size_t>(edge)].origin.translation.z << '\n';

    return 0;
}
```

Run against [`examples/robot.urdf.xacro`](examples/robot.urdf.xacro), that prints:

```text
tabletop_arm: 3 links, 2 joints
root-first: base_link upper_link gripper_link
base_to_upper origin z = 0.3
```

The `0.3` was written in the description as `${upper_length}`, and the root-first order is topology
meios computed while reading rather than something you reconstruct afterwards.

Eighteen more programs, each one runnable and each listed with what it shows, are indexed in
[`examples/`](examples/README.md).

## Continuous integration

[![Sanitizers](https://github.com/skrede/meios/actions/workflows/sanitizers.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/sanitizers.yml)
[![Clang-Tidy](https://github.com/skrede/meios/actions/workflows/clang-tidy.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/clang-tidy.yml)
[![Canary](https://github.com/skrede/meios/actions/workflows/canary.yml/badge.svg?branch=master)](https://github.com/skrede/meios/actions/workflows/canary.yml)

Every push runs the three platform workflows in the header. Each one is a fan-out of jobs rather than
a single build, and the integration-shaped ones are the point: meios is *installed* and then consumed
from that install tree, on all three platforms, by a project that lives outside its build.

| What is proven | Job | Linux | macOS | Windows |
|---|---|:--:|:--:|:--:|
| Compiles and tests | `build` — GCC 14 and Clang 18 / AppleClang / MSVC | ● | ● | ● |
| **Installs, then a separate project consumes it** — `find_package` against the install prefix, built and run | `install-test` | ● | ● | ● |
| **An installed program finds its deployed description** — the resource consumer is installed and the installed binary is run | `install-test` | ● | ● | ● |
| Enrichment targets build and pass together | `enrichments` | ● | ● | ● |
| The example programs compile | `examples` | ● | ● | ● |
| **Every C++ block in this README and in `docs/` is extracted and compiled** | `docs` | ● | ● | ● |
| The CLI builds and its unit tests pass | `tools` | ● | ● | ● |
| Generative properties of the xacro evaluator | `property-tests` (RapidCheck) | ● | ● | ● |
| An optional dependency is absent as well as present | `eval-python` — Python found *and* absent | ● | | |
| A pinned known-good and known-faulty document pair | `corpus` | ● | | |
| Header-guard uniqueness and format | `header-guards` | ● | | |
| No header over the size ceiling unless registered | `file-size` | ● | | |
| Line coverage, uploaded to Codecov | `build` → gcovr | ● | | |

Consuming from a *subproject* or `FetchContent` acquisition is covered too, by the CMake integration
suite that runs inside the platform workflows: install opt-in versus decline, export-set legality
when a dependency was fetched rather than found, the installed module manifest, resource deployment
and pruning, and the configure-time refusals — each an end-to-end sub-configure of a real consumer
tree.

Four workflows are deliberately **advisory** — they report without blocking a merge, and promotion to
a required gate would be a visible decision rather than a silent one:

| Workflow | What it watches | Why advisory |
|---|---|---|
| Sanitizers | asan/ubsan and tsan, with a step asserting the library object really carries the instrumentation; plus four fuzz harnesses, existence checked, run 60s each | findings are triaged, not merge-blocking |
| Clang-Tidy | first-party `lib/` translation units | the check itself says so, in its own job output |
| Canary | newest-toolchain drift, `gcc:latest`, warnings-as-errors forced off | a benign new-compiler diagnostic must not red-light the pipeline |
| Nightly | corpus breadth over expression-valued documents from a third-party host | fetches an upstream every run, so it can fail for reasons that are not meios |

## Documentation

Start at the [documentation hub](docs/README.md), which routes you to a tier — `load()` a description
and read the resolved model, or receive the robot into your own types via a `model_sink` — before you
read either guide. Every C++ example in the guides is compiled by CI, so a renamed symbol breaks the
build instead of rotting on the page.

- [Consumer guide](docs/consumer-guide.md) — `load()` a description and read the resolved `model`.
- [Engine guide](docs/engine-guide.md) — receive the robot into your own types via `model_sink`.
- [Resource guide](docs/resources-guide.md) — acquire a description package in CMake, deploy it where
  your program looks for it, and flatten one description to a plain URDF.
- [Evaluation](docs/evaluation.md) — what a description's expressions may run, and the rules that
  refuse the rest.
- [Asset resolution](docs/asset-resolution.md) — what a mesh or texture URI may name, what a relative
  path is measured against, and how long a resolved path stays valid.
- [URDF profile](docs/urdf-profile.md) — every rule the reader applies, the diagnostic code each
  refusal carries, and the frame and unit conventions the numbers follow.
- [Known limitations](docs/known-limitations.md) — every defect and caveat live at this point in the
  library's life, described by its user-facing effect.

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
