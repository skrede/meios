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

**Extension fragments are dropped, not carried through.** Robot-level `<gazebo>`,
`<ros2_control>`, `<transmission>`, and `<sensor>` fragments are recognized, reported at the `warn`
tier naming the element, and then discarded. The `extension` record type exists but nothing
populates it, and the model's extension collections are always empty. A consumer that needs
simulator, controller, transmission, or sensor configuration cannot recover it from a loaded model
and must read the source document itself. Preserving these losslessly is planned; until it lands,
treat a meios load as lossy for everything outside the URDF kinematic and visual surface.

**The Python evaluator runs a restricted subset, and the restriction has false refusals.**
`meios::eval-python` refuses an expression that reaches past arithmetic, comprehensions, the
mathematics names and twenty builtins — the import machinery, the filesystem and the process are not
reachable through it. That costs coverage in both directions: `map` and `filter` are not available,
every use of the string formatting method is refused including an innocent one, and a description
property named `format` is refused the moment it enters a composed expression. The full subset, the
four refusal rules and the divergences from canonical xacro are in the [evaluation
guide](evaluation.md), which also covers `meios::unrestricted_python_evaluator` — a supported backend
that applies none of those rules and is reachable only from C++. The built-in core evaluator has no
such exposure — it evaluates a fixed numeric and boolean grammar and loud-fails on anything outside
it.

**The evaluator has no bound on resource exhaustion.** An expression such as `${10**10**10}` or
`${[0]*10**12}` is refused by nothing — it names no withheld builtin, traverses no attribute and
touches no file — and will burn processor time and memory. The restriction above is about authority
(the filesystem, the network, the process), never about availability. A real bound needs a
per-expression watchdog against an embedded interpreter holding the interpreter lock, portable across
macOS, Linux and Windows; there is none today.

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

## The CMake resource modules

**The automated tests never reach the network.** Every acquisition case builds its own origin on the
local disk, so nothing in the routine test set exercises a transport failure, a redirect, a
certificate problem, or a host that serves different bytes than it did last week. A live fetch runs
separately, in the corpus acquisition behind its own option, and the split is deliberate: the tests
that must pass on every machine cannot depend on a third party being up.

**The `GITHUB` short form is never exercised end to end.** It rewrites into `URL` or
`GIT_REPOSITORY`, and both of those are exercised — but the rewrite itself needs a real host, so a
defect in the archive URL it composes, or in the `STRIP_TOP_LEVEL` it implies, would surface on your
first configure rather than in the test set.

**`MEIOS_RESOURCE_TLS_CAINFO` is exercised by nothing.** Forwarding a CA bundle to the download
needs an origin served over TLS, which nothing offline can be. On a machine whose CMake ships
without a trust store that variable is the documented way through, and it is also the one thing on
this page with nothing at all standing behind it.

**Flattening is not proven from an installed package.** That an installed meios carries acquisition
and deployment to a `find_package` consumer is proven on all three platforms — the install-consumer
example is built, installed, and run — but no automated run flattens a description through an
installed meios. Every build carrying the Python evaluator has the install step switched off, and
the tests reach the modules through the module path instead.

**Precedence between `PACKAGE_PATH` entries is reasoned about, not measured.** That an entry adds
reach is exercised in both directions: a package vendored inside another package's directory is
invisible to the crawl of the tree root and resolves once the directory holding it is passed. What
no test covers is two roots offering the same package name. The acquired tree goes first and the
entries follow in the order written, and the first root to supply a name is the one that wins, so
an entry cannot displace the tree — but that ordering is read off the module rather than observed.

**Flattening with the Python backend is only ever really run on Linux.** No macOS or Windows build
carries the evaluation enrichment. On those platforms the configure-time behavior — the disclosure,
the refusal, the accepted backend names — is exercised with the capability supplied by hand, and the
expansion itself is not exercised at all.

**Submodules and large-file objects are left alone in a real clone, by design.** meios initializes
no submodule and fetches no Git-LFS object of its own; what a clone brings back is whatever the
host's git is configured to bring back, and an unsmudged mesh pointer that survives is refused
loudly rather than shipped as stub geometry. No test clones a repository that has either, so the
behavior you get on a machine without git-lfs configured is the loud refusal and nothing more
graceful.

**Nothing in the automated set cross-compiles.** `meios_target_flatten_resource` refuses a cross
build that has not been handed a host-runnable binary, and both that refusal and the path it points
at — a flatten driven by a binary built for the build host — are reasoned about rather than
measured. If you cross-compile and flatten, you are the first to do it.

**How the build integration finds the command-line tool is proven for one of the three ways it can
be found.** Every test either hands the module an explicit path to a binary or hands it nothing and
checks that it refuses. Neither of the two ordinary routes is exercised: picking the tool up from an
installed package, and picking it up as a target in a build that also builds the tool. Those are
exactly the routes that motivated exporting the tool as a target rather than recording a path
string, because a target resolves to the right binary on a generator that builds several
configurations out of one project — and that property is reasoned about, not measured. The effect
for you is that the two ordinary ways of getting the tool are the two the tests do not exercise.

**An unrecognized evaluator name is refused by the build integration and accepted by the
command-line tool.** `meios_target_flatten_resource(… EVAL pyhton)` is a configure error listing the
names that are accepted. `meios flatten … --eval pyhton` is not an error at all: the tool carries no
list of accepted names, silently uses its default for anything it does not recognize, and prints a
document with no diagnostic. A typo therefore changes which evaluator runs, and the only sign is
that an expression the core evaluator refuses starts failing to resolve.

## API stability

**No deprecation cushion before `v1.0.0`.** meios is pre-release. A superseded type or function is
deleted outright rather than marked `[[deprecated]]`, so an API you depend on can change or disappear
between versions with no compiler warning to cushion the transition. This holds until the `v1.0.0`
boundary, which is where the pre-release breaking-change license ends. Pin a revision if you need
stability in the meantime.
