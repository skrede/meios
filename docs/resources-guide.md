# Resource guide: acquiring and deploying descriptions

A robot description is data, not code. meios ships CMake functions so a non-ROS project can acquire
a description package and put it where its program will look for it, without vendoring the tree into
the repository and without hand-rolling copy commands.

Two of them carry the whole story, and they are deliberately separate:

- **`meios_declare_resource`** answers *where do the bytes come from* — a pinned archive, a Git
  clone, or a directory already on disk. It registers the resulting tree under a name.
- **`meios_target_deploy_resources`** answers *which target needs them, and where* — beside the
  built executable, in the install tree, or both.

They split because they vary independently. One description may be deployed beside several
executables, and a target's runtime layout is decided in the directory that owns the target, not in
the file that fetched the tree. Declare at the top level, deploy in the subdirectory that builds the
program:

```cmake
# top-level CMakeLists.txt
find_package(meios CONFIG REQUIRED)

meios_declare_resource(
    NAME kuka
    URL  https://github.com/ros-industrial/kuka_experimental/archive/<sha>.tar.gz
    HASH SHA256=<hex>
    STRIP_TOP_LEVEL)

add_subdirectory(app)
```

```cmake
# app/CMakeLists.txt
add_executable(app main.cpp)
target_link_libraries(app PRIVATE meios::urdf)

meios_target_deploy_resources(app RESOURCES kuka SUBDIR urdf)
```

The registry is global, which is what makes the cross-directory case work — a name declared in the
top-level listfile is visible in any subdirectory, including a sibling of the one that declared it.

A third function, `meios_target_flatten_resource`, expands one description to a file during your
build. It is a deployment convenience for a program that needs a plain URDF on disk rather than the
way descriptions are meant to be consumed, and it is documented after the two above.

What these functions are *not* proven to do — the paths no automated test reaches — is listed under
[Known limitations](known-limitations.md).

## `meios_declare_resource`

```cmake
meios_declare_resource(
    NAME <name>

    # exactly one acquisition mode:
    GITHUB <owner>/<repository> REF <tag|branch|commit> [HASH <ALGO>=<hex>]
    URL <url> [HASH <ALGO>=<hex>] [STRIP_TOP_LEVEL]
    GIT_REPOSITORY <url> [GIT_TAG <tag>]
    SOURCE_DIR <dir>

    [SPARSE_PATHS <relative/path>...]
    [SUBDIR <relative/path>]
    [OUT_DIR <variable>])
```

`GITHUB` is the short form and the one to reach for first. `REF` takes a tag, a branch, or a commit
alike, so it reads the way `FetchContent_Declare` does:

```cmake
meios_declare_resource(NAME kuka GITHUB ros-industrial/kuka_experimental REF melodic-devel)
```

It expands to the archive URL and implies `STRIP_TOP_LEVEL`, since GitHub always wraps the tree in a
`<repository>-<ref>` directory.

`URL` downloads and extracts any archive, for hosts other than GitHub or for a release asset rather
than a generated tarball.

`GIT_REPOSITORY` with `GIT_TAG` shallow-clones. It is the escape hatch for Git-LFS, submodules, and
private auth — clones are slower and a branch is not reproducible, but it is the only mode that
handles those three, and the only one that can fetch *part* of a tree (see `SPARSE_PATHS`). The
clone's `.git` directory is dropped once the checkout is done: an acquired tree is data, and every
mode produces the same shape.

`SOURCE_DIR` registers a tree already on disk. Nothing is fetched, but the tree is still validated.

### Pinning: you do not have to compute the hash

`HASH` is what makes a fetch reproducible and tamper-evident, and it is also what enables caching —
without one there is no trustworthy cache key, so every configure re-downloads. But you do not have
to work the value out yourself. Declare the resource without it, configure once, and the warning
carries the value to paste back:

```
meios_declare_resource(kuka): fetched WITHOUT an integrity hash. […] Pin it by adding:
    HASH SHA256=703ea2a8502afd10ab8f5ec1775147263f3658e09ad81f14a1ce8788cfea7ce9
```

Paste that line into the call and the warning goes away, the fetch becomes reproducible, and later
configures hit the cache instead of the network.

One caveat specific to `GITHUB` and to `URL`s pointing at GitHub's generated archives: those tarballs
are produced on demand rather than stored, and their bytes have changed in the past when GitHub
changed its compression, invalidating pinned hashes across the ecosystem. A hash over a generated
archive is therefore a strong integrity check but not an eternal one. Pinning `REF` to a commit
rather than a branch removes the content drift; if you need an artifact guaranteed byte-stable, point
`URL` at an uploaded release asset, or use `GIT_REPOSITORY` with a commit `GIT_TAG`.

### Fetching only part of a repository

`PACKAGES` on the deploy call trims what gets *shipped*; the whole tree is still downloaded first.
`SPARSE_PATHS` trims what gets **fetched**, using a partial clone plus a cone-mode sparse checkout:

```cmake
meios_declare_resource(
    NAME         ur_description
    GITHUB       UniversalRobots/Universal_Robots_ROS2_Description
    REF          4.3.1
    SPARSE_PATHS urdf config meshes/ur5e)
```

For that repository the difference is 3.5 MB fetched against 27.5 MB for the tarball, and 9.7 MB on
disk against 104 MB — the meshes for a dozen robot variants are what fills it, and one variant is
what a program needs.

`SPARSE_PATHS` implies a clone even when the declaration is written as `GITHUB`, because no GitHub
endpoint serves part of a tree: the mode is re-routed to the git protocol, and `HASH`, which pins an
archive's bytes, is rejected as meaningless there. Pin the revision with a commit `REF` instead. It
is likewise rejected with `URL` and with `SOURCE_DIR`.

Two things are checked because git will not fail on its own. A path list is verified against the
worktree after checkout — cone mode reports success for a pattern that matches nothing, so a
mistyped `meshes/ur5X` would otherwise leave a tree quietly missing `meshes/` altogether. And the
path set is folded into the cache key, so widening the selection re-fetches rather than handing back
the narrower tree. Cone mode needs Git 2.28 or newer; an older Git fails the configure naming its
own version.

Cone mode always brings the files at each parent level along, so `package.xml` and the repository's
root files arrive whether or not you ask for them — which is what makes the slice a resolvable
package rather than a bag of directories.

`SUBDIR` narrows the resource to a subdirectory of whatever was acquired — use it to treat a nested
directory as the tree itself. To ship selected *packages* out of a monorepo, keeping their names so
`package://` still resolves, see `PACKAGES` on the deploy call below. `SUBDIR` is rejected if it
escapes the acquired tree.

`OUT_DIR` writes the resolved path into a variable for callers that want the path directly rather
than through a target. `meios_resource_dir(<name> <variable>)` does the same from any directory
scope, and is the better choice across directory boundaries.

### Unsmudged Git-LFS meshes fail the configure

Every mode scans the resolved tree for `.stl`, `.dae`, `.obj`, `.ply`, and `.glb` files that are
Git-LFS pointer stubs rather than geometry, and fails the configure if it finds any. A pointer is a
valid file at a valid path, so without this check the description resolves cleanly and draws
nothing — a failure that otherwise surfaces much later and much less obviously.

### Configuring without a network

`URL` and `GIT_REPOSITORY` need connectivity on first configure. For an offline machine, set the
per-resource cache variable to a pre-placed tree; it overrides the declared mode without editing the
listfile that declares it:

```
cmake -S . -B build -DMEIOS_RESOURCE_kuka_SOURCE_DIR=/opt/descriptions/kuka
```

`MEIOS_RESOURCE_CACHE_DIR` relocates the acquisition cache out of the build tree, so several build
directories can share one download. `MEIOS_RESOURCE_TLS_CAINFO` supplies a CA bundle where CMake
ships without a trust store.

## The acquisition cache

A tree is stored under a name derived from every argument that determines its bytes — the URL and
whether the top level is stripped, or the repository, revision and slice — and not under the name
you gave the resource. Two consequences are worth knowing, because both are things the obvious
layout gets wrong:

- **Changing any of those arguments re-fetches.** A key assembled by hand can forget an argument and
  hand back a tree fetched under different ones; a key that *is* the argument list cannot.
- **Two build trees can pin different revisions of the same resource through one shared cache.**
  Sharing a directory named after the resource, whichever configured last would overwrite the
  other's tree — and the loser would go on to *build* against the wrong revision without anything
  changing in its own listfiles.

The layout inside the cache is an implementation detail; ask for a path with `OUT_DIR` or
`meios_resource_dir` rather than composing one. Each entry keeps a small `.stamp` beside it naming
what it holds, which is what makes a directory called `ur_description-4b20623385db` legible.

### Collecting what is no longer used

Every configure rewrites a claim file listing exactly the entries that build tree declared. Rename a
resource, change a revision, or delete a declaration, and the entry it used stops being claimed:

```
cmake -DMEIOS_RESOURCE_CACHE_DIR=<dir> -P <cmakedir>/MeiosPruneResources.cmake
```

That reports what nothing claims any more, along with interrupted fetches and leftover scratch.
Add `-DMEIOS_PRUNE_REMOVE=ON` to actually delete it; reporting is the default.

Collection is a separate step on purpose, and never part of a configure. A configure sees only its
own declarations, so collecting from inside one would delete the trees its siblings are still
building against — the same failure the content-addressed layout exists to prevent. A build tree
whose `CMakeCache.txt` is gone is treated as gone, so deleting a build directory releases its claims
without any further ceremony.

For the default cache inside the build tree none of this is pressing: `rm -rf build` reclaims
everything. It matters when `MEIOS_RESOURCE_CACHE_DIR` points somewhere that outlives a build.

## `meios_target_deploy_resources`

```cmake
meios_target_deploy_resources(<target>
    RESOURCES <name>...
    [PACKAGES <name>...]
    [SUBDIR <relative/path>]
    [INSTALL_RUNTIME_RELATIVE | INSTALL_DESTINATION <dir>]
    [INSTALL_COMPONENT <component>])
```

`SUBDIR` is relative to the target's runtime directory. Several resources may share one `SUBDIR`,
which is how sibling description packages end up under a single directory you can hand to
`load_options::package_roots`:

```cmake
meios_target_deploy_resources(app RESOURCES kuka universal_robots SUBDIR urdf)
```

### Shipping part of a monorepo

Many upstream description repositories hold a dozen packages and you need two of them. `PACKAGES`
copies only the named top-level entries, each keeping its own directory name:

```cmake
meios_declare_resource(NAME kuka URL … HASH SHA256=… STRIP_TOP_LEVEL)

meios_target_deploy_resources(app
    RESOURCES kuka
    PACKAGES  kuka_kr6_support kuka_resources
    SUBDIR    urdf)
```

Keeping the names is the whole point: `package://kuka_kr6_support/…` resolves to
`<package root>/kuka_kr6_support/…`, so a selection that flattened a package's contents into the
package root would strip the very name the reference is looked up under. This is also why `SUBDIR`
on the *declaration* is the wrong tool for the job — it descends into a directory and makes that
directory the resource, which is right for narrowing to a mesh folder and wrong for picking packages.

Because `PACKAGES` names entries inside one tree, it takes exactly one `RESOURCES` name. An entry
that does not exist is a configure error, so a typo fails immediately rather than producing a
package root that silently cannot resolve.

### Selecting inside a single package

The other shape is one package whose bulk sits two levels down — a `meshes/` directory holding a
dozen robot variants when the program needs one. An entry may be a nested path, and it keeps that
path at the destination, so putting the package's own name in `SUBDIR` selects within it:

```cmake
meios_target_deploy_resources(app
    RESOURCES ur_description
    PACKAGES  urdf config meshes/ur5e
    SUBDIR    models/ur_description)
```

That deploys `models/ur_description/{urdf,config,meshes/ur5e}`, leaving the package root at
`models`, which is the directory `load_options::package_roots` is handed. The rule is the same one
as above — an entry keeps its own name — read at a depth greater than one. Pair it with
`SPARSE_PATHS` on the declaration to avoid downloading the variants you then drop.

Work out the full set before trimming: a description usually pulls in a shared package for materials
and constants, and dropping it is a hard failure at load, not a cosmetic one. For the example above,
`kuka_kr6_support/urdf/kr6r900sixx_macro.xacro` includes
`$(find kuka_resources)/urdf/common_materials.xacro`, so `kuka_resources` is required even though no
`package://kuka_resources/…` reference appears anywhere. Sibling variants in the same package can
differ: `kr6r900_2_macro.xacro` additionally reaches into `kuka_kr10_support`. Deploying the whole
tree always works and is the right default; trim when the size difference earns it.

Deployment is wired into the build graph on the tree's contents, not attached as a post-build step,
so editing a description redeploys it on the next build even when no source file changed.

### Deploying is not installing

The call above populates the **build tree only**. `cmake --install` will not carry those resources
anywhere, so an installed program that looks for them beside its own executable will fail to find
them — and it fails only after install, because the build tree resolves fine either way.

Say where they go in the install tree as well:

```cmake
meios_target_deploy_resources(app RESOURCES kuka SUBDIR urdf INSTALL_RUNTIME_RELATIVE)
```

`INSTALL_RUNTIME_RELATIVE` installs the tree to `${CMAKE_INSTALL_BINDIR}/<SUBDIR>`, mirroring the
build-tree layout so binary-relative lookup keeps working after install. Use it whenever the program
resolves its resources from its own executable's directory. If you install the executable somewhere
other than `CMAKE_INSTALL_BINDIR`, name the location yourself with `INSTALL_DESTINATION` instead.

`INSTALL_DESTINATION <dir>` installs the tree wherever you say. It is the right choice when the
program finds its resources some other way — a configured path, an environment variable, a
command-line argument — because a conventional data location like `share/<app>/urdf` installs
cleanly and still breaks a program that looks beside its own binary. Pick the destination to match
how your program actually resolves the path; the two are not independent.

`INSTALL_COMPONENT` names the component for either install form.

## `meios_target_flatten_resource`

Flattening at build time runs the `meios` binary over one description out of an acquired tree and
writes the resolved document beside your program: xacro expanded, arguments applied, `package://`
and `$(find …)` references resolved, one file with nothing left to look up.

It is a **deployment convenience**, and it is worth being explicit about what it is not. It exists
for something downstream that insists on a plain file — a viewer that reads URDF and nothing else, a
tool that cannot run an expander, a runtime that must not. It is not how this library is meant to be
consumed: `load()` resolves a description into your own types with no intermediate file and no
expansion step in anybody's build, and that is the path to reach for. Flatten because something
outside your program needs the file, never to spare your own program the work.

```cmake
meios_target_flatten_resource(<target>
    RESOURCE <name>
    INPUT    <relative/path>
    OUTPUT   <relative/path>

    [ARGS <key>:=<value>...]            # no ';' inside a value
    [PACKAGE_PATH <relative/path>...]
    [EVAL core|python]
    [INSTALL_RUNTIME_RELATIVE | INSTALL_DESTINATION <dir>]
    [INSTALL_COMPONENT <component>])
```

`RESOURCE` names a declared resource and `INPUT` is a path inside the tree that declaration acquired
— the acquired tree, not the deployed copy. A path naming no file there fails the configure and
prints the full path it looked at.

`OUTPUT` is relative to the target's runtime directory, the same root that
`meios_target_deploy_resources` writes into, so `OUTPUT urdf/ur5e.urdf` lands inside a tree deployed
with `SUBDIR urdf`. `INPUT`, `OUTPUT` and `PACKAGE_PATH` each have to stay inside their tree: an
absolute path, a `..` component, or a leading `-` is refused by name.

`ARGS` passes xacro argument overrides, one `key:=value` per entry — the spelling the command line
takes, and the one place this call is easy to get wrong; see below.

`PACKAGE_PATH` adds package search roots, each relative to the acquired tree, and the tree itself is
always passed as the first and highest-precedence root — so an entry cannot outrank the tree for a
package the tree's own crawl found. What an entry adds is reach. A root is crawled until the first
package on each branch and no further, so a package vendored inside another package's directory is
invisible from the tree root and resolves only when the directory holding it is named here:
`PACKAGE_PATH pkg_outer/vendor` reaches a package under `pkg_outer/vendor/`. A package sitting under
ordinary directories is found at any depth with no entry at all.

`EVAL` selects the evaluator backend, `core` or `python`. `core` is the default and evaluates the
fixed numeric and boolean grammar; `python` needs a binary built with the evaluation enrichment. The
choice is not a detail — see below.

`INSTALL_RUNTIME_RELATIVE`, `INSTALL_DESTINATION` and `INSTALL_COMPONENT` install the flattened
document under the same rules the deploy call uses, and carry the same trap: without one of them the
build tree alone is populated. `INSTALL_RUNTIME_RELATIVE` installs it to `${CMAKE_INSTALL_BINDIR}`
followed by the directory part of `OUTPUT`, mirroring the build-tree layout; `INSTALL_DESTINATION
<dir>` installs it to exactly `<dir>`, with nothing appended. `INSTALL_COMPONENT` without either is
a configure error.

With `ur` declared at the top level the way the examples above declare a resource, deploy the tree
and expand one description out of it — here a parameterized top-level xacro resolved for one robot
variant:

```cmake
add_executable(app main.cpp)
target_link_libraries(app PRIVATE meios::urdf)

meios_target_deploy_resources(app
    RESOURCES ur
    SUBDIR    urdf
    INSTALL_RUNTIME_RELATIVE)

meios_target_flatten_resource(app
    RESOURCE ur
    INPUT    ur_description/urdf/ur.urdf.xacro
    OUTPUT   urdf/ur5e.urdf
    ARGS     ur_type:=ur5e name:=ur5e
    INSTALL_RUNTIME_RELATIVE)
```

`app` ends up with the deployed tree under `urdf/` and `urdf/ur5e.urdf` beside it, in the build tree
and in the install tree alike.

### Which binary does the expanding

The rule needs a `meios` binary that runs on the build host, and it takes the first of three it
finds: `MEIOS_CLI_EXECUTABLE`, a cache entry naming a binary you already have; the imported
`meios::cli` target, from a `find_package(meios)` whose installed package carries the tool; or the
in-tree `meios` target, in a build that builds meios itself with `MEIOS_BUILD_TOOLS=ON`.

Nothing searches `PATH` — a binary found there is version skew between these modules and whatever
expansion semantics that binary happens to carry — and nothing turns the tools option on for you,
because a function call that quietly rewrites a build setting you chose is not a diagnostic. With
none of the three present the configure fails, naming both the option and the cache entry.

Ask for the tool explicitly if your build depends on it, and the failure moves to `find_package`
where it belongs:

```cmake
find_package(meios CONFIG REQUIRED COMPONENTS urdf cli)
```

Cross-compiling always needs `MEIOS_CLI_EXECUTABLE`: the flatten runs on the build host while the
binary this build produces is built for the target. That is refused at configure time rather than
left to surface as an exec-format error out of the build tool.

### The evaluator is stated at configure time, and never substituted

Every call prints which backend it will use:

```
-- meios flatten (app): ur.urdf.xacro with the core evaluator
```

and the python backend says outright what asking for it means:

```
-- meios flatten (app): ur.urdf.xacro with the python evaluator, which executes Python during the build
```

That is the whole disclosure, and it is informative rather than a gate — you opened the door by
writing the backend into your own listfile. What is a gate is the other direction: asking for
`python` in a build whose binary does not carry the evaluation enrichment is a configure error
naming `MEIOS_EVAL_PYTHON_SUPPORT`, never a quiet substitution of the core evaluator. A backend name
that is neither of the two is refused with the accepted values listed.

The command-line tool is not as strict about that last one. See
[Known limitations](known-limitations.md).

### Argument overrides are a CMake list

Each `ARGS` entry is one `key:=value` token. The key must be non-empty; the value need not be, so
`name:=` is a legal override setting the argument to the empty string.

Two spellings are refused rather than passed along. An entry beginning with `-` reaches the binary
as an option instead of an override. And a `;` inside a value does not survive the list: CMake
splits the entry into two elements before the function is ever entered, leaving a half that carries
no assignment at all, so the call fails with a message saying to write the value without one. There
is no escape that gets a semicolon through this argument — run the binary yourself if you need one.

### Ordering and rebuilds

The rule watches the acquired tree's `.urdf`, `.xacro`, `.xml` and `.yaml` files, so editing a
description re-runs the flatten on the next build even though no source file changed — the same
reason deployment is wired into the build graph rather than attached as a post-build step. It also
depends on the binary, and it is ordered after the deploy rule for the same resource on the same
target, so the tree and the document expanded out of it are never written in the other order.

### A failed flatten leaves nothing behind

A description that does not resolve fails the build, and the binary's typed diagnostic reaches your
build log with its file and line intact:

```
…/ur_description/urdf/ur.urdf.xacro:8:27: [error] (undefined_property) name 'ur_typ' is not defined
```

Nothing is written: not a truncated document, not an empty one, and not the temporary the rule
captures into. The document is published by renaming that temporary once the binary has exited zero,
because build tools disagree about what to do with a failed command's leftovers and a half-written
URDF parses far enough to look like a whole one.

## Loading what you deployed

Deploying beside the executable means the runtime path is derived from the binary rather than baked
in at configure time, so the build directory stays relocatable. Point `package_roots` at the
directory you deployed into and `package://` references resolve against it:

<!-- meios:snippet name=resource-load tu -->
```cpp
#include <meios/urdf.h>

#include <iostream>
#include <filesystem>

int main(int, char **argv)
{
    const std::filesystem::path runtime = std::filesystem::path(argv[0]).parent_path();

    meios::load_options options;
    options.package_roots.push_back(runtime / "urdf");

    const auto loaded = meios::load(runtime / "urdf/kuka_lbr_iiwa_support/urdf/arm.urdf", options);
    if (!loaded)
    {
        std::cout << "load failed: " << loaded.error().message << '\n';
        return 1;
    }

    std::cout << "loaded " << loaded->robot.links.size() << " links\n";
}
```

A `package://<name>/<path>` reference resolves to `<package root>/<name>/<path>`, so the deployed
directory must contain the package directory — deploy the tree that *holds* the packages, not one
package's own root.

If a reference does not resolve, the load fails: `load_options::on_missing` defaults to
`missing_asset::fail`, so a description naming a mesh that is not there never reaches you looking
complete. Lower it to `missing_asset::warn` when you genuinely want the model without the assets —
an inspection pass, a kinematics-only consumer. You still get an honest value: the result's
`claims` no longer carry `completeness::deployment_complete`, and its `diagnostics` name every
reference that failed, one entry each. `missing_asset::skip` drops the diagnostics too, and with
them any record that anything was missing.

## Worked examples

Two, covering the two halves of the story:

- **`examples/resources/deploy_resources.cpp`**, declared in `examples/CMakeLists.txt` and deployed
  in `examples/resources/CMakeLists.txt` — build-tree deployment, declared both offline via
  `SOURCE_DIR` and, under `MEIOS_EXAMPLE_FETCH_NETWORK`, from a pinned archive. Both land in one runtime directory, which is what the plural `RESOURCES` argument is for.
  Built as part of the meios build with `MEIOS_BUILD_EXAMPLES=ON`.

- **`examples/install_consumer/`** — a standalone project that configures against an *installed*
  meios, deploys with `INSTALL_RUNTIME_RELATIVE`, installs its own executable, and resolves the
  description from the installed binary's own directory. This is the one to copy if you ship or
  package your program:

  ```
  cmake -S examples/install_consumer -B build -DCMAKE_PREFIX_PATH=<meios install prefix>
  cmake --build build
  cmake --install build --prefix <somewhere>
  <somewhere>/bin/install_consumer
  ```

  Because it configures through `find_package(meios)` rather than as part of the meios build, it is
  also what proves these functions reach install-mode consumers at all; the `install-test` CI job
  builds, installs, and runs it on Linux, macOS, and Windows.
