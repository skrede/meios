# Example programs

Eighteen small programs, each one a complete `main` that runs, checks what it claimed, and exits
non-zero if it did not get it. Configure with `-DMEIOS_BUILD_EXAMPLES=ON`, build, and run the set
with `ctest -R '^example_'`. Every option named on this page, and everything else the build accepts,
is in the [build reference](../docs/cmake-integration.md).

A program marked **needs** is registered only when its enrichment target was built. If you do not see
it in the `ctest` list, the option beside it is off — that is the whole reason, and turning it on is
the fix.

## Loading a description — `loading/`

Start here. These three cover the two ways to take delivery of a resolved robot.

- **`load_robot`** — `load()` the vendored xacro description, then print the link count, the
  root-first order meios reconstructed while reading, and one joint origin that arrived as a number
  rather than as `${…}` text.
- **`load_into_sink`** — declare a type satisfying the `model_sink` concept and let `load_into` drive
  the description straight into it, with no `model` in between to copy out of.
- **`xacro_overrides`** — pass a xacro argument in through `load_options::args`, refuse anything the
  evaluator will not run, and read back the joint origin the argument moved.

## What a load has to say — `diagnostics/`

Every one of these runs under the default policies. Nothing is loosened to provoke a diagnostic; the
content that provokes them is content shipped descriptions genuinely carry.

- **`capture_diagnostics`** — load a description naming a package that is deliberately absent, and
  catch what the default missing-asset policy refuses through a `capturing_log_sink`.
- **`diagnostic_codes`** — branch on typed `diagnostic_code` values instead of matching message
  text, over a vendor extension block and a material the document never declares.
- **`completeness_claims`** — read the three claims a successful load carries, which is how you tell
  a load that produced a model from a load that produced a clean one.

## Where the bytes come from — `sources/`

How `package://` and `$(find)` are answered, one layer at a time.

- **`directory_source`** — resolve against a directory laid out on disk, and tell a reference the
  package simply does not hold apart from one a source with no root refuses outright. Both hand back
  nothing; only one of them says why.
- **`memory_source`** — serve a description and its mesh from an in-memory tree with no filesystem
  layout at all, and read back exactly what was added.
- **`stacked_sources`** — stack a memory layer over a directory layer, show which one wins a
  reference both carry, count the shadow diagnostic the loser raises, and fall through to the layer
  only the lower one holds.
- **`ros_resolve`** — crawl ROS package manifests under a directory deployed beside the executable
  and resolve every reference named on the command line. **Needs `MEIOS_ROS_PACKAGE_SUPPORT`**, which
  is on by default. On the default path it also resolves a pinned, hashed upstream package fetched at
  configure time alongside the one authored here; `MEIOS_EXAMPLE_FETCH_NETWORK=OFF` leaves the
  in-repository trees alone and needs no network.

## Writing a description back out — `bundling/`

- **`flatten_description`** — expand one description to a plain URDF in memory, and report the emit
  status, how many assets were seen, and how many went unresolved.
- **`bundle_folder`** — walk what a folder bundle would write for a description served from memory,
  and write nothing, so the closure can be inspected before anything lands on disk.
- **`archive_bundle`** — write a description into a `.zip` through `zip_writer` and report the emit
  status. **Needs `MEIOS_ARCHIVE_ZIP_SUPPORT`.**

## What a mesh file references — `scanners/`

A scanner reads which files a mesh points at, never its triangles. Each returns references verbatim,
so the bundle closure can re-anchor them against the referring package.

- **`mesh_scan`** — read the `mtllib` and `map_*` references out of a Wavefront `.obj`. **Needs
  `MEIOS_SCAN_OBJ_SUPPORT`.**
- **`collada_scan`** — read the image references out of a COLLADA document, including a
  percent-encoded filename and a `file://` one. **Needs `MEIOS_SCAN_COLLADA_SUPPORT`.**
- **`gltf_scan`** — read the external `uri` references out of a glTF document, and show the inline
  `data:` image being skipped rather than handed on as a path. **Needs `MEIOS_SCAN_GLTF_SUPPORT`.**
- **`stl_scan`** — show that the STL scanner is a typed no-op rather than a missing one: an STL
  references nothing, and an empty result from a registered scanner is told apart from an empty
  result for an extension nothing ever registered. **Needs `MEIOS_SCAN_STL_SUPPORT`.**

## Getting a description to where the program looks — `resources/`

- **`deploy_resources`** — read a description out of a package deployed beside the executable, with
  the runtime path derived from `argv[0]` rather than baked in at configure time, so the build tree
  can be moved or handed out whole.

## A consumer that installs — `install_consumer/`

Not part of this build and not in the `example_` set. It is a standalone project that configures
against an *installed* meios with `find_package`, deploys its description both beside the built
binary and under the install prefix, and is then installed and run from there. It is the worked
example for shipping a program together with the description it loads; see its own listfile for the
four commands.

## The descriptions these programs read

`robot.urdf.xacro` and `arg_robot.urdf.xacro` are the two documents the loading and diagnostics
groups read, copied beside the build tree at configure time so no program reaches back into the
source tree. `description/` and `ros_description/` are the two package trees the resource and
manifest examples resolve against. All four are authored here.
