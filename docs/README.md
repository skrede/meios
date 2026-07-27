# meios documentation

Start here. This page routes you to the right tier before you read a line of the guides — pick how
you consume the library, then follow the one guide that matches.

## Which API tier is yours?

meios has two tiers. You are on exactly one of them; read only its guide.

**Use the consumer tier if you want the resolved robot and nothing else.** You call `load()`, get an
`expected<model<double>, load_error>` back, and read the flattened links, joints, materials, and
topology out of the `model`. This is the tier for a tool that inspects a description, a converter, or
any consumer that is happy to hold the model in meios's own `model` type. Read the
[consumer guide](consumer-guide.md).

**Use the engine tier if you want the resolved robot pushed straight into your own types.** You
declare a struct that satisfies the `model_sink` concept and meios drives it — one call per robot,
material, link, and joint — so the description lands directly in your scene graph, your solver's
bodies, or your renderer's nodes with no intermediate `model` to copy out of. Injecting a `log_sink`
alongside it captures typed diagnostics into your own record type. Read the
[engine guide](engine-guide.md).

If you are unsure: start on the consumer tier. It is the smaller surface, and every engine-tier
program can fall back to reading a `model` first. The engine tier pays off once copying out of
`model` into your own representation is the thing you were going to do anyway.

## Which build integration is yours?

Both tiers link the same target, `meios::urdf` — the target that carries the public API. Choose how
you acquire it:

**Use `find_package` if meios is installed** — a system package, a vendored install prefix, or a
CI-built install tree. The installed package exports `meios::urdf` with its include directories and
the C++20 requirement baked into the interface, so a single line wires it in:

```cmake
find_package(meios CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE meios::urdf)
```

**Use `FetchContent` if you build meios in-tree** — you have no install step and want the source
pinned and built as part of your own configure. It produces the same `meios::urdf` target:

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

Link `meios::urdf` either way. Do not link `meios::core` reaching for the API — `meios::core` is the
dependency-light nucleus and carries no reader; `meios::urdf` is the surface a consumer reaches for.

## Guides

- [Consumer guide](consumer-guide.md) — `load()` a description and read the resolved `model`.
- [Engine guide](engine-guide.md) — receive the resolved robot into your own types via `model_sink`.
- [Resource guide](resources-guide.md) — acquire a description package in CMake, deploy it where
  your program looks for it, and flatten one description to a file when something downstream needs
  a plain URDF, instead of vendoring the tree into your repository.
- [Evaluation](evaluation.md) — what a description's expressions may run, the rules that refuse the
  rest, and how the resource helper resolves a file.
- [URDF profile](urdf-profile.md) — the expanded URDF document meios reads, the rules that refuse the
  rest, and the diagnostic code each refusal carries.
- [Known limitations](known-limitations.md) — every defect and caveat live at this point in the
  library's life, described by its user-facing effect.
