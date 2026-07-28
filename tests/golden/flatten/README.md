# Flatten baselines

Two baselines live here and they answer different questions. Know which one you are
diffing before you touch it.

| File | Driven by | Source | What it can catch |
|---|---|---|---|
| `lbr_iiwa_14_r820.flat.urdf` | `tests/integration/urdf_flatten_instrument_test.cpp` | a pinned upstream description, fetched | any drift in flatten output over a real robot |
| `resolved_assets.flat.urdf` | `tests/integration/urdf_flatten_resolution_test.cpp` | a small local fixture, assets seeded | a resolved path reaching a written document |

Both are byte-for-byte baselines of the one serializer reached through
`flatten(model, ostream, log)` in `meios/bundle/flatten.h`, and both are pinned to LF
via `.gitattributes` (`tests/golden/** text eol=lf`) so the comparison is byte-identical
on every platform, including the Windows runner.

## `lbr_iiwa_14_r820.flat.urdf` — the byte-identical instrument

The instrument loads the pinned source below, drives the serializer into a string, and
asserts the bytes are identical to this file. Any drift fails the blocking corpus CI leg
— this is the scope-creep detector for flatten output.

**Its assets never resolve.** The pinned description's `package://` meshes are not fetched
with it, so the instrument lowers the asset policy to `warn` and every mesh reaches the
serializer with no resolved path at all. That is fine for what this baseline is for, and it
is the reason the second baseline exists: a bug writing a resolved path instead of the
authored URI would leave this file byte-identical, because there is no resolved path to
write.

### Pinned source

- Upstream: `ros-industrial/kuka_experimental`, `melodic-devel` head commit
  `8d9292b04a22628b1b78d989e2ddd3abb913bf92` (archive-pinned, SHA256-verified in
  `cmake/corpus.cmake`).
- Description: `kuka_lbr_iiwa_support/urdf/lbr_iiwa_14_r820.urdf` — a pre-expanded,
  pure-URDF serial arm that loads with zero error-level diagnostics (its
  `package://` meshes surface only as warn-level `unresolved_asset`, which do not
  affect the identity-rewritten flatten output).

Because the source is SHA256-pinned through the fetcher, this baseline is
reproducible: the same pin yields the same bytes.

### Regeneration

A legitimate upstream-pin bump (updating the pin in `cmake/corpus.cmake`) is the
only reason to regenerate this baseline. Configure with the corpus fetched, build the
instrument, and run it once in bless mode:

```sh
cmake . -B build -DMEIOS_BUILD_TESTS=ON -DMEIOS_CMAKE_FETCH_DEPS=ON -DMEIOS_FETCH_CORPUS=ON
cmake --build build --target flatten_instrument_test
MEIOS_FLATTEN_BLESS=1 ./build/tests/flatten_instrument_test
```

## `resolved_assets.flat.urdf` — the resolution-aware guard

The guard drives `tests/fixtures/urdf/flatten/resolved_assets.urdf`, a one-link document
carrying a `package://` mesh and a relative texture. Its test builds a temporary tree,
seeds both files and registers the package root, then **requires that both assets actually
resolved** before it flattens anything — the mesh's `resolved_path` and the material's
`resolved_texture` are set and differ from the authored strings. Only then does it compare
these bytes.

That ordering is the whole point. Without it the comparison would degrade into the same
blind check the instrument already is, and would keep passing if resolution stopped working.
With it, a serializer writing the resolved path instead of the authored URI rewrites both
`filename` attributes below to a temporary directory nobody else has, and the comparison
fails.

Unlike the instrument, this target is registered by an ordinary configure — it needs no
corpus fetch — so the guard runs in every build.

### Regeneration

A deliberate, reviewed change to flatten output is the only reason to regenerate this
baseline:

```sh
cmake . -B build -DMEIOS_BUILD_TESTS=ON
cmake --build build --target urdf_flatten_resolution_test
MEIOS_FLATTEN_BLESS=1 ./build/tests/urdf_flatten_resolution_test
```

Bless mode rewrites the file from the current flatten output, so a baseline is always
produced by the same load-and-flatten path the test then asserts against — never
hand-edited. Review the resulting diff before committing: a temporary directory name
appearing in either `filename` attribute is the leak this guard exists to stop, not a
baseline that needs updating.
