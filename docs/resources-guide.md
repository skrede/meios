# Resource guide: acquiring and deploying descriptions

A robot description is data, not code. meios ships two CMake functions so a non-ROS project can
acquire a description package and put it where its program will look for it, without vendoring the
tree into the repository and without hand-rolling copy commands.

The two are deliberately separate:

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

## `meios_declare_resource`

```cmake
meios_declare_resource(
    NAME <name>

    # exactly one acquisition mode:
    URL <url> [HASH <ALGO>=<hex>] [STRIP_TOP_LEVEL]
    GIT_REPOSITORY <url> [GIT_TAG <tag>]
    SOURCE_DIR <dir>

    [SUBDIR <relative/path>]
    [OUT_DIR <variable>])
```

`URL` downloads and extracts an archive. Always pin `HASH` — without one the download is trusted on
TLS and server honesty alone, a swapped artifact is accepted silently, and the result is never
cached, so every configure re-downloads. `STRIP_TOP_LEVEL` drops the single wrapper directory that
GitHub's archive tarballs put around the repository.

`GIT_REPOSITORY` shallow-clones. It is the escape hatch for Git-LFS, submodules, and private auth,
not the default — an archive with a hash is reproducible and a branch is not.

`SOURCE_DIR` registers a tree already on disk. Nothing is fetched, but the tree is still validated.

`SUBDIR` narrows the resource to a subdirectory of whatever was acquired, so you can pull one
package out of a monorepo rather than deploying the whole checkout. It is rejected if it escapes the
acquired tree.

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

    const auto robot = meios::load(runtime / "urdf/kuka_lbr_iiwa_support/urdf/arm.urdf", options);
    if (!robot)
    {
        std::cout << "load failed: " << robot.error().message << '\n';
        return 1;
    }

    std::cout << "loaded " << robot->links.size() << " links\n";
}
```

A `package://<name>/<path>` reference resolves to `<package root>/<name>/<path>`, so the deployed
directory must contain the package directory — deploy the tree that *holds* the packages, not one
package's own root.

## Worked examples

Two, covering the two halves of the story:

- **`examples/deploy_resources.cpp`** with `examples/CMakeLists.txt` — build-tree deployment,
  declared both offline via `SOURCE_DIR` and, under `MEIOS_EXAMPLE_FETCH_NETWORK`, from a pinned
  archive. Both land in one runtime directory, which is what the plural `RESOURCES` argument is for.
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
