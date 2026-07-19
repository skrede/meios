# Flatten baseline

`lbr_iiwa_14_r820.flat.urdf` is the byte-for-byte baseline of the library's flatten
output — the single serializer reached through `flatten(model, ostream, log)` from
`meios/bundle/flatten.h`. The flatten instrument (`tests/integration/urdf_flatten_instrument_test.cpp`)
loads the pinned source below, drives that serializer into a string, and asserts the
bytes are identical to this file. Any drift fails the blocking corpus CI leg — this
is the scope-creep detector for flatten output.

## Pinned source

- Upstream: `ros-industrial/kuka_experimental`, `melodic-devel` head commit
  `8d9292b04a22628b1b78d989e2ddd3abb913bf92` (archive-pinned, SHA256-verified in
  `cmake/corpus.cmake`).
- Description: `kuka_lbr_iiwa_support/urdf/lbr_iiwa_14_r820.urdf` — a pre-expanded,
  pure-URDF serial arm that loads with zero error-level diagnostics (its
  `package://` meshes surface only as warn-level `unresolved_mesh`, which do not
  affect the identity-rewritten flatten output).

Because the source is SHA256-pinned through the fetcher, this baseline is
reproducible: the same pin yields the same bytes.

## Regeneration

A legitimate upstream-pin bump (updating the pin in `cmake/corpus.cmake`) is the
only reason to regenerate this baseline. Configure with the corpus fetched, build the
instrument, and run it once in bless mode:

```sh
cmake . -B build -DMEIOS_BUILD_TESTS=ON -DMEIOS_CMAKE_FETCH_DEPS=ON -DMEIOS_FETCH_CORPUS=ON
cmake --build build --target flatten_instrument_test
MEIOS_FLATTEN_BLESS=1 ./build/tests/flatten_instrument_test
```

Bless mode rewrites this file from the current flatten output, so the baseline is
always produced by the same load-and-flatten path the test then asserts against —
never hand-edited. Review the resulting diff before committing.

This file is pinned to LF via `.gitattributes` (`tests/golden/** text eol=lf`) so the
comparison is byte-identical on every platform, including the Windows runner.
