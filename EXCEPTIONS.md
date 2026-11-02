# Size-gate exceptions

This file is the single sanctioned registry of source units that are allowed to
exceed the size ceilings in `CONVENTIONS.md` (functions 25 lines, files 200 lines).
There is no in-code marker for an exception: a unit is exempt only if it is listed
here, with the reason it cannot be decomposed further.

An entry stays as small as its justification: the path, the ceiling it exceeds, and
one line on why the "one thing" genuinely cannot be split. Anything not listed here
must stay under the ceiling — the default is to decompose, never to widen the budget.

| Path | Ceiling exceeded | Reason |
|------|------------------|--------|
| `lib/meios-core/src/meios/xacro/unicode_decomposition_data.cpp` | file, 200 lines | Generated from Python 3.14's unicodedata and diffed against a fresh regeneration in CI; one table whose entries index one shared pool, so splitting it would scatter a generated whole no reader edits by hand. |
