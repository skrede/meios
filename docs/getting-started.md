# Getting started

meios reads a **robot description** — a URDF or xacro document describing a robot's rigid bodies and
the joints between them — resolves everything it references, and hands the finished robot to your own
C++ code as plain data. There is no intermediate format to parse and no silent partial result: a
description that cannot be resolved comes back as a typed `file:line` error.

A description's expressions are evaluated natively, by an evaluator compiled into the library, so
everything below happens inside your own process and nothing is started beside it.

This page takes you from an empty directory to a program that loads a description and prints what is
in it. It assumes you have never used the library. Everything you need is here; the guides linked at
the end pick up where it stops.

## What you need

- **A C++20 compiler** — GCC 14 or newer, Clang 18 or newer, or MSVC 19.38 or newer. meios is built
  and tested on Linux, macOS and Windows.
- **CMake 3.28 or newer.** That is the minimum meios's own build declares, so an older CMake refuses
  the configure outright rather than failing later.
- **pugixml** — the one dependency the core carries. You do not have to install it: if CMake cannot
  find a copy, meios fetches and builds a pinned one during your configure. The same holds for
  yaml-cpp, which the default build carries so that a description's expressions can read an
  auxiliary configuration document without anything extra being installed.

That is the whole list — with one caveat about how the two libraries arrive: a fetch needs Git and
network access on your first configure, so an offline machine wants both already discoverable.
Beyond that, meios needs no ROS installation, no Python interpreter and no robotics framework. Those
are optional add-ons for capabilities this page does not use.

## Adding meios to your build

Assuming nothing is installed, fetch the source and build it as part of your own configure. A
complete `CMakeLists.txt` for a first program:

```cmake
cmake_minimum_required(VERSION 3.28)
project(first_load CXX)

include(FetchContent)
FetchContent_Declare(
    meios
    GIT_REPOSITORY https://github.com/skrede/meios.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(meios)

add_executable(first_load main.cpp)
target_link_libraries(first_load PRIVATE meios::urdf)
```

Link **`meios::urdf`**. It is the target that carries the public API, and it brings its include
directories and the C++20 requirement along with it, so that one line is the entire integration. Do
not link `meios::core` reaching for the API — `meios::core` is the dependency-light nucleus and
carries no reader.

If meios is already installed — a system package, a vendored prefix, a build server's install tree —
replace the `FetchContent` block with `find_package(meios CONFIG REQUIRED)` and link the same target.
Which of the two acquisition paths is yours, every build option and its default, and what an install
exports are all in [CMake integration](cmake-integration.md). A first program needs none of it.

## Loading one description

Two more files, beside that `CMakeLists.txt`.

### The description

A **link** is a rigid body. A **joint** connects two links and fixes where the child sits relative to
its parent. The description below has three links in a chain and the two joints holding them
together. Save it as `arm.urdf`:

```xml
<?xml version="1.0"?>
<robot name="two_joint_arm">
  <link name="base_link"/>
  <link name="arm_link"/>
  <link name="tool_link"/>

  <joint name="base_to_arm" type="revolute">
    <parent link="base_link"/>
    <child link="arm_link"/>
    <origin xyz="0 0 0.25" rpy="0 0 0"/>
    <axis xyz="0 0 1"/>
    <limit lower="-1.57" upper="1.57" effort="10" velocity="1"/>
  </joint>

  <joint name="arm_to_tool" type="fixed">
    <parent link="arm_link"/>
    <child link="tool_link"/>
    <origin xyz="0 0 0.4" rpy="0 0 0"/>
  </joint>
</robot>
```

### The program

`meios::load` takes a path and hands back an `expected<load_result, load_error>` — a value holding
either the result or the error, never both and never neither. Test it before you reach through it;
that is the whole error-handling protocol. Save this as `main.cpp`:

<!-- meios:snippet name=first-load tu -->
```cpp
#include <meios/urdf.h>
#include <meios/model.h>

#include <cstddef>
#include <iostream>

int main()
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load("arm.urdf");
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

    for(const int index : robot.topo.order)
        std::cout << "  " << robot.links[static_cast<std::size_t>(index)].name << '\n';

    for(const meios::joint<double> &edge : robot.joints)
        std::cout << edge.name << ": " << edge.parent << " -> " << edge.child
                  << ", z = " << edge.origin.translation.z << '\n';

    return 0;
}
```

### Building and running it

```console
$ cmake -S . -B build
$ cmake --build build
$ ./build/first_load
two_joint_arm: 3 links, 2 joints
  base_link
  arm_link
  tool_link
base_to_arm: base_link -> arm_link, z = 0.25
arm_to_tool: arm_link -> tool_link, z = 0.4
```

Run it from the directory holding `arm.urdf`, because that is the path it passed. On Windows the
default generator is multi-configuration, so the program lands at `build\Debug\first_load.exe`
instead.

### What each step produced

**`load` returned a `load_result`.** Its `robot` is the resolved model — the thing you came for.
Alongside it the result carries `diagnostics`, the ordered list of everything the load had to say,
and `claims`, what the load is willing to assert it established: that the document parsed, that the
topology holds, that every referenced asset was found. This program reads only `robot`; the consumer
guide covers the other two.

**The model carries flattened records.** `robot.links` and `robot.joints` are plain vectors of
resolved values, not a tree of nodes to walk. The `0.25` and `0.4` printed above are the joint
origins as numbers, already substituted and already in meters — nothing is left as text for you to
evaluate. `robot.link_index` and `robot.joint_index` map a name to its position when you want one
record by name rather than all of them.

**The order was reconstructed, not read.** `robot.topo.order` is the root-first visitation sequence,
and `robot.topo.parent_of` gives each link's parent by index, with `-1` for a root. URDF states
parent and child on each joint and nothing about the shape of the whole; meios rebuilds that shape
while reading, which is why `base_link` came out first without the file saying so.

**A diagnostic would have named where it gave up.** Misspell the second joint's child as
`tool_lnik` and the same program prints:

```text
arm.urdf:17:6: undeclared_link: joint 'arm_to_tool' names undeclared child link 'tool_lnik'
```

That is the failure arm doing its job: `loc` is the `file:line:column` of the offending element —
line 17 is the `<child>` element — `code` is a typed value you can switch on instead of matching
message text, and `message` is the human-readable sentence. Nothing was written to `stderr` behind
your back, and no half-built model was returned.

## Where to go next

You have a resolved model. Pick the page that owns what you want to do with it:

- [Consumer guide](consumer-guide.md) — everything a `load_result` carries, and the caveat about
  what a successful load does *not* promise.
- [Engine guide](engine-guide.md) — push the description straight into your own scene graph, solver
  or renderer types through the `model_sink` concept, with no `model` in between.
- [URDF profile](urdf-profile.md) — what a description may contain, and the diagnostic code each
  refusal carries.
- [Asset resolution](asset-resolution.md) — what a `<mesh>` or `<texture>` filename may name, how
  `package://` is resolved, and what happens when the file is not there.

If your description is xacro rather than plain URDF, the call above is unchanged — expansion happens
inside `load`, through the built-in evaluator, with nothing to install for it. The grammar it
evaluates, the rules that refuse the rest, and the ceilings that bound one load are
[evaluation](evaluation.md).
