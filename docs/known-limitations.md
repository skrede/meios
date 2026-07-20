# Known limitations

meios never fails silently, and that discipline extends to its own status: this page is the candid,
complete list of the defects and caveats that are live right now. Each is described by the effect you
will see, not by the work that would close it. If a behavior below surprises you, it is documented
here on purpose.

## Sources and resolution

**No runtime remote fetch.** meios cannot pull a description over the network at run time. A source
whose location is only known once the program is running — supplied by a host configuration engine,
say — is not fetchable through the library; you must have the description on local disk (or fetched
during your build) before you call `load()`. Descriptions are hand-vendored today, with the licensing
exposure and unresolvable references that implies. Fetching HTTPS with a single self-contained
dependency across macOS, Linux, and Windows is not currently achievable under the library's
one-extra-dependency rule, and the rule is not bent to force it.

**Symlink-installed workspaces are covered by fixtures only.** A colcon `--symlink-install`
workspace, where installed package files are symlinks back into the source tree, is exercised through
crafted fixtures rather than a real symlink-installed layout. The path resolution is believed correct,
but it has not been proven against a live `--symlink-install` workspace.

## Diagnostics you must opt into

**A successful `load()` does not mean a clean load.** `load(path).has_value() == true` tells you the
description resolved far enough to produce a model — it does **not** tell you no warnings were raised.
The convenience `load()` overload is silent by default: it surfaces only the first fatal error, on the
`load_error` channel, and drops every `level::warn` diagnostic. An unresolved `package://` reference,
a material collision under a `warn` policy, or a topology issue under a `warn` policy will all pass
without a trace on this path.

**Warnings require an injected sink.** To see the dropped `warn`-tier diagnostics, call the overload
that takes a `log_sink` and inject one. Without a sink, the whole `warn` tier is discarded — by
design, so the library stays silent by default, but it means a consumer who wants the warnings must
ask for them explicitly. The engine guide shows a sink that captures typed diagnostics into your own
record type.

## Modeling gaps

**Extension fragments are carried through, not interpreted.** Robot-level `<gazebo>`,
`<ros2_control>`, `<transmission>`, and `<sensor>` fragments are preserved verbatim as opaque
extensions and flagged at the `warn` tier; meios does not parse or model their contents. If your
consumer needs the semantics inside those fragments, it must interpret the carried-through text
itself.

**Python xacro expressions are recovered by literal re-parsing.** With the Python evaluation
enrichment enabled, the result of a Python expression is re-hydrated by re-parsing its literal form.
A result that is not expressible as a Python literal is therefore not representable through this seam;
the principled opaque-value path is not yet in place.

## Command-line behavior

**`info` and `complete` fail loudly on a broken topology.** On a robot whose topology does not
reconstruct — a link with more than one parent, a cycle, an undeclared link — `meios info` exits
non-zero with no output and `meios complete` yields no completions, rather than printing a partial
view. This follows from silent-by-default plus the default `fail` topology policy. `meios tree` uses a
`warn` policy and still prints what it can.

## API stability

**No deprecation cushion before `v1.0.0`.** meios is pre-release. A superseded type or function is
deleted outright rather than marked `[[deprecated]]`, so an API you depend on can change or disappear
between versions with no compiler warning to cushion the transition. This holds until the `v1.0.0`
boundary, which is where the pre-release breaking-change license ends. Pin a revision if you need
stability in the meantime.
