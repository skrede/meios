# Upstream oracle

Measures upstream xacro so the records under `tests/golden/oracle` can authorize what the native
evaluator is allowed to do. The records are the authority; this runner exists only to regenerate
them and is never invoked from a test.

Two packages are pinned exactly and installed into a virtual environment beneath the build tree:
`xacro==2.1.1` and `PyYAML==6.0.3`. Nothing else is installed, and neither is linked into or
installed by this project.

`ament_index_python/packages.py` closes upstream xacro's one unsatisfiable import. It resolves
`$(find <package>)` against the package roots named in `ORACLE_PACKAGE_ROOTS` — the tree the
corpus fetch copies into the build directory — rather than against a sourced ROS environment,
which is what makes a run reproducible on any machine with no ROS installed.

```
cmake -S . -B build/corpus -DMEIOS_BUILD_TESTS=ON -DMEIOS_FETCH_CORPUS=ON -DMEIOS_CORPUS_EXPRESSION_DOCUMENTS=ON
python3 tests/tools/oracle/record_upstream.py --package-root build/corpus/_meios_corpus_packages --out tests/golden/oracle
```
