# CMake integration

This is the build reference. Every option meios declares and what it defaults to, the two ways to
acquire the library, what an installed package carries and what it deliberately withholds, and the
CMake functions that put a robot description where your program will look for it.

If you are wiring meios into a build for the first time, [Getting started](getting-started.md) is
shorter and is enough for a first program. Come here when you need to know what a flag does.

Nothing on this page has to be switched on to load a description whose macros compute their numbers:
expression evaluation is native, compiled into the library, and the module that reads an auxiliary
configuration document from an expression is built by default. Every option below either adds a
capability beside that default or decides what this project builds beside the library.

## The target you link

Link **`meios::urdf`**. It carries the public API and brings its include directories and the C++20
requirement with it, so one `target_link_libraries` line is the whole integration. Do not link
`meios::core` reaching for the API — `meios::core` is the dependency-light nucleus and carries no
reader.

<!-- meios:snippet name=link-and-load tu -->
```cpp
#include <meios/urdf.h>
#include <meios/model.h>

#include <iostream>

int main()
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load("robot.urdf");
    if(!loaded)
    {
        std::cout << to_string(loaded.error().loc) << ": " << loaded.error().message << '\n';
        return 1;
    }

    const meios::model<double> &robot = loaded->robot;
    std::cout << robot.name << ": " << robot.links.size() << " links\n";
    return 0;
}
```

Both API tiers link the same target. What you do with the result is the
[consumer guide](consumer-guide.md) or the [engine guide](engine-guide.md); which of them is yours
is decided in [the documentation hub](README.md).

## Acquiring meios

There are two paths and they produce the same target names.

### Build meios inside your own configure

You have no install step and want the source pinned and built as part of your configure. This is
the `FetchContent` path, and `add_subdirectory` of a checkout behaves identically:

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

What it produces: build-tree targets under the `meios::` namespace, compiled with your own compiler
and flags, and the resource functions available in your listfiles. Two defaults flip on this path
because meios is no longer the top-level project — `MEIOS_INSTALL` and `MEIOS_WERROR` are both off,
so meios neither reaches your install prefix nor lets its own warning drift break your build.

### Find an installed package

meios is already installed — a system package, a vendored prefix, a build server's install tree:

```cmake
find_package(meios CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE meios::urdf)
```

What it produces: imported targets read from the installed `meiosTargets.cmake`, with include
directories and the C++20 requirement baked into their interfaces, plus meios's CMake resource
modules, which are installed beside the config file so this path reaches them too.

### How dependencies are acquired

One option decides the policy for every dependency, and it is set before meios's subdirectories so
the library and its tests never disagree:

- `MEIOS_CMAKE_FETCH_DEPS=OFF` (the default) prefers a discoverable `find_package` for each
  dependency and falls back to the pinned source only when nothing is found.
- `MEIOS_CMAKE_FETCH_DEPS=ON` forces every dependency down the pinned-source path, ignoring
  whatever is installed on the machine.

The dependencies themselves follow the modules that need them. `pugixml` is the one the core always
carries, and `yaml-cpp` follows the auxiliary-document module, which is built by default — so a
first configure resolves those two and nothing else. `nlohmann_json`, `miniz` and `pybind11` are
acquired only when the enrichment that links each is being built. The Python interpreter that
`meios::eval-python` drives is *found*, never fetched — an environment resource rather than a pinned
dependency, and one nothing acquires unless you switch that backend on.

## Options

The options split along a line worth seeing, because the two halves have different audiences. One
half changes what is compiled into the library you link, so it changes what your program can do and
what your install carries. The other half only decides what this project builds beside that library,
so a consumer normally leaves all of it alone.

Every default below is the default the declaration states.

### What is compiled into the library you link

| Option | Default | What it does |
| --- | --- | --- |
| `MEIOS_SCAN_OBJ_SUPPORT` | `OFF` | Build `meios::scan-obj` (dependency-free). |
| `MEIOS_SCAN_STL_SUPPORT` | `OFF` | Build `meios::scan-stl` (dependency-free). |
| `MEIOS_SCAN_COLLADA_SUPPORT` | `OFF` | Build `meios::scan-collada` (pugixml). |
| `MEIOS_SCAN_GLTF_SUPPORT` | `OFF` | Build `meios::scan-gltf` (nlohmann/json). |
| `MEIOS_ROS_PACKAGE_SUPPORT` | **`ON`** | Resolve `package://` through ROS package manifests (pugixml). |
| `MEIOS_YAML_SUPPORT` | **`ON`** | Build `meios::yaml`, which reads an auxiliary configuration document from an expression (yaml-cpp). |
| `MEIOS_ARCHIVE_ZIP_SUPPORT` | `OFF` | Build `meios::archive-zip` (miniz). |
| `MEIOS_EVAL_PYTHON_SUPPORT` | `OFF` | Build `meios::eval-python` (pybind11 + found Python3). |
| `MEIOS_REQUIRE_EVAL_PYTHON` | `OFF` | Turn a skipped `meios::eval-python` into a configure error. |

**Two of these are on by default, and they are the two the default load path needs.**
`MEIOS_ROS_PACKAGE_SUPPORT` is named for the layout it reads rather than for the module it builds
(`meios::ros-package`), and it carries no dependency the core does not already have. Without it,
`package://` resolution succeeds only where a directory name happens to equal the package name it
declares; turning it off gives up that resolution and the environment search lists it reads.
`MEIOS_YAML_SUPPORT` builds `meios::yaml`, which is what answers `xacro.load_yaml` in an expression.
Turn it off and a description reaching for an auxiliary document is told that the build resolves no
such format, rather than being handed an empty one — a real capability loss on the descriptions that
read their joint limits out of a file.

**`MEIOS_REQUIRE_EVAL_PYTHON` enables nothing.** It is a strictness switch over
`MEIOS_EVAL_PYTHON_SUPPORT`, not a second way to turn the module on. With support on and this off, a
missing Python `Development.Embed` component makes the configure print that it is skipping
`meios::eval-python` and carry on. With this on, the same situation is a hard configure error, and
so is the module failing to materialize for any other reason. On its own it does nothing at all.

### What the project builds beside the library

| Option | Default | What it does |
| --- | --- | --- |
| `MEIOS_BUILD_TESTS` | `OFF` | Build unit and compile tests. |
| `MEIOS_BUILD_PROPERTY_TESTS` | `OFF` | Build property tests (RapidCheck). |
| `MEIOS_BUILD_FUZZERS` | `OFF` | Build libFuzzer harnesses (Clang only). |
| `MEIOS_BUILD_EXAMPLES` | `OFF` | Build examples. |
| `MEIOS_BUILD_DOCS` | `OFF` | Build the documentation compile-gate. |
| `MEIOS_BUILD_TOOLS` | `OFF` | Build the meios CLI. |

`MEIOS_BUILD_FUZZERS` needs `-fsanitize=fuzzer`, so the harnesses are skipped whole on any compiler
other than Clang rather than failing to link. `MEIOS_BUILD_DOCS` requires a Python 3 interpreter: it
runs the extractor that turns the marked C++ blocks in these pages into translation units and
compiles them against the build-tree `meios::urdf`, which is how a documented call that no longer
exists breaks a build instead of rotting quietly.

### How the build itself behaves

| Option | Default | What it does |
| --- | --- | --- |
| `MEIOS_CMAKE_FETCH_DEPS` | `OFF` | Use FetchContent for dependencies. |
| `MEIOS_WERROR` | `ON` standalone, `OFF` as a subproject | Treat warnings as errors for first-party targets. |
| `MEIOS_COVERAGE` | `OFF` | Instrument first-party targets for coverage (gcc/clang). |
| `MEIOS_INSTALL` | `ON` standalone, `OFF` as a subproject | Install meios into the install prefix. |

`MEIOS_WERROR` is the only switch that injects `-Werror`, and it is applied per target rather than
directory-wide so the warning set never leaks into a third-party build. Its default is meios's own
top-level status, which is what keeps meios's warning drift from ever breaking a consumer that
builds it in-tree. `MEIOS_COVERAGE` instruments only first-party targets and only under GCC or
Clang, and links the instrumentation through the build interface so it never reaches an installed
export.

`MEIOS_INSTALL` has the same conditional default; see [what an installed package
carries](#what-an-installed-package-carries) for the contract that goes with it.

### Options that exist only inside a subtree

These are declared in the subdirectories they belong to, so they appear in the cache only once the
option that enters that subtree is on. They are listed here because they are options this project
declares, not because a consumer of the library has any use for them.

| Option | Default | Appears when | What it does |
| --- | --- | --- | --- |
| `MEIOS_FETCH_CORPUS` | `OFF` | tests are built | Fetch the pinned robot-description corpus and build the corpus test. |
| `MEIOS_CORPUS_BREADTH` | `OFF` | tests are built | Load every top-level document the pinned known-good upstream ships. |
| `MEIOS_EXAMPLE_FETCH_NETWORK` | **`ON`** | examples are built | Fetch the pinned upstream description for the examples. |

### What the library does over the network

**Nothing.** No part of the library opens a network connection at configure, build or run time. A
program that links `meios::urdf` and reads a description you already have on disk performs file I/O
and nothing else, and a build whose dependencies resolve through `find_package` adds nothing to
that. This is a property worth relying on if you build behind a firewall or on a disconnected
machine.

The two fetching options above are a separate matter and belong to this project's own test and
example builds, not to the library. `MEIOS_EXAMPLE_FETCH_NETWORK` is deliberately on: the
manifest-resolution example resolves a pinned, hashed upstream package alongside the one authored
here, because that example is only worth reading if it resolves a genuine package as well as an
authored one. Turning it off leaves the in-repository trees alone and needs no network. Neither
option changes what the library does, and neither is reachable unless you are building this
project's examples or tests yourself.

### Variables that are not options

These carry the `MEIOS_` prefix and turn up in caches and listfiles, so they are recorded here to
say what they are. The first two groups are not yours to set at all. The third is settable, but it
is documented elsewhere.

- **Config-package gate flags** — `MEIOS_CORE_PUGIXML_INSTALLED`, `MEIOS_SCAN_GLTF_INSTALLED`,
  `MEIOS_ARCHIVE_ZIP_INSTALLED`, `MEIOS_CLI_HAS_EVAL_PYTHON`. The build sets each one as it decides
  which modules it is producing, and the installed config file reads them back to know which
  dependencies it must resolve. They are output, not input.
- **Command-line capability flags** — `MEIOS_CLI_HAS_SCAN_OBJ` and its siblings, `MEIOS_CLI_HAS_ROS`,
  `MEIOS_CLI_HAS_EVAL_PYTHON`. Compile definitions and global properties that the CLI's own listfile
  derives from which enrichment targets exist. Defining one by hand claims a capability that is not
  linked in.
- **Resource-system variables** — `MEIOS_RESOURCE_CACHE_DIR`, `MEIOS_RESOURCE_TLS_CAINFO`,
  `MEIOS_RESOURCE_<name>_SOURCE_DIR`, `MEIOS_CLI_EXECUTABLE`. Real cache entries you may set, but
  they belong to the description-acquisition system rather than to meios's build, and the
  [resource guide](resources-guide.md) is where each is documented.

## What an installed package carries

With `MEIOS_INSTALL` on, the build installs its export set under the `meios::` namespace along with
`meiosConfig.cmake`, `meiosConfigVersion.cmake` and meios's CMake resource modules. The resource
modules are installed on purpose: they are consumer-facing API, so a `find_package` consumer must
reach them the same way an in-tree consumer does.

The version file declares **`SameMinorVersion`** compatibility. A `find_package(meios 0.2 CONFIG)`
is satisfied by 0.2.x and refused by 0.3.0 — while the library's surface is still moving, a minor
bump is treated as a break rather than as an upgrade.

### The install default, and overriding it

`MEIOS_INSTALL` defaults to meios's own top-level status: on when meios is the project being built,
off when meios is a subproject, so a consumer's install prefix receives only what the consumer asked
for.

A parent that wants the subproject case to install anyway **sets `MEIOS_INSTALL` before the
`add_subdirectory` or `FetchContent_MakeAvailable` that adds meios.** A normal variable is enough —
it overrides the default with no cache entry and no ceremony. Setting it afterwards is too late, and
the configure says so in as many words rather than silently installing nothing.

That status line is one of three the build prints when it declines to install, and one of them will
always be there to explain an empty prefix:

- meios is top level and `MEIOS_INSTALL` is off.
- meios is a subproject and the parent did not set it in time.
- a fetched dependency declines to install itself, so it belongs to no export set and meios's own
  export could not generate around it. The message names the dependency and the option that would
  make it install, and it turns `MEIOS_INSTALL` off inside meios's own scope only.

### Requesting components

The installed config file publishes one component per module: `core`, `model`, `io`, `urdf`,
`xacro`, `bundle`, `completion`, `scan-obj`, `scan-collada`, `scan-stl`, `scan-gltf`,
`archive-zip`, `eval-python`, `ros-package`, `yaml` and `cli`. Each is found if and only if the
matching imported target came out of the package:

```cmake
find_package(meios CONFIG REQUIRED COMPONENTS urdf ros-package)
```

A component whose target is present is found; one whose target is absent is not, and a `REQUIRED`
request for a component that is not there fails the configure rather than letting you discover the
absence at a link. The failure comes back as the package itself not being found, so to ask which
component was missing — or to branch instead of failing — read the per-component variable the config
file sets for every name in the list above:

```cmake
find_package(meios CONFIG REQUIRED)
if(meios_ros-package_FOUND)
    target_compile_definitions(my_app PRIVATE MY_APP_HAS_ROS)
endif()
```

That is what makes an installed package answer honestly about what it holds.

### Modules that build but are not installed

A module can be perfectly good in-tree and still be withheld from the installed package. That is a
declared property of the module, not an accident:

- The withholding must carry a **stated reason**. A module marked as not exportable with no reason
  is a hard configure error, so the mechanism cannot be used to make a module quietly disappear.
- The reason is printed as a configure status line, in the form
  `meios: <target> builds but is not installed, because <reason>`.
- **The archive and the headers are withheld together.** Installing the headers alone would hand you
  a translation unit that compiles and then fails to link, with nothing in the package explaining
  why. Making the absence total means the component mechanism above is the single source of truth
  about it.

One module is withheld today: **`meios::eval-python`**, because its embedded-interpreter link edge
cannot be re-resolved from an installed tree. Build it and link it in-tree and it works; neither its
archive nor its headers arrive in your install prefix, and `meios_eval-python_FOUND` is false in
every package you install from this project.

## Descriptions your program needs at runtime

A robot description is data, and getting it beside your program is build wiring, so the functions
that do it are named here. Three of them carry the whole surface:

- **`meios_declare_resource`** — register a description package under a name, from wherever its
  bytes come from.
- **`meios_target_deploy_resources`** — put a declared package beside a built target, in the install
  tree, or both.
- **`meios_target_flatten_resource`** — expand one description out of a declared package to a single
  file during your build, for something downstream that insists on a plain URDF.

Declaring and deploying are separate calls because they vary independently: one description may be
deployed beside several executables, and a target's runtime layout is decided in the directory that
owns the target, not in the file that acquired the tree. **Declare at the top level, deploy in the
subdirectory that builds the program** — the registry is global, so a name declared in the top-level
listfile is visible in any subdirectory, including a sibling of the one that declared it.

Everything else about these functions — where bytes may come from, how acquired trees are stored and
reclaimed, sparse checkouts, install behavior, and the flatten rule's arguments — is the
[resource guide](resources-guide.md)'s to own, and is documented in full there rather than
summarized here.

## Where to go next

- [Getting started](getting-started.md) — the shortest path from nothing to a running program.
- [Resource guide](resources-guide.md) — acquiring, deploying and flattening a description package.
- [Known limitations](known-limitations.md) — what is not yet proven, described by its user-facing
  effect.
