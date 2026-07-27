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

## Diagnostics

**A successful `load()` does not mean a clean load.** A value on the success arm tells you meios
produced a model, not that the document was clean. A material collision under a `warn` policy, a
topology issue under a `warn` policy, and an unresolved `package://` reference under a lowered
`on_missing` all pass without failing the load. The result carries every diagnostic the document
raised, in source order, alongside the three completeness claims; branch on those rather than on the
presence of a value. The failure arm carries the same list beside the error that names the failure.

**Nothing is printed unless you ask.** meios installs no output stream of its own. A program that
injects no `log_sink` sees nothing at the moment a diagnostic is raised and reads the same
diagnostics off the result afterwards instead. That is deliberate rather than a defect, and it is
listed here only because silence during a load is easy to mistake for evidence of a clean one. The
engine guide shows a sink that captures typed diagnostics into your own record type as they are
raised.

## Modeling gaps

**Extension fragments are dropped, not carried through.** Robot-level `<gazebo>`,
`<ros2_control>`, `<transmission>`, and `<sensor>` fragments are recognized, reported at the `warn`
tier naming the element, and then discarded. The `extension` record type exists but nothing
populates it, and the model's extension collections are always empty. A consumer that needs
simulator, controller, transmission, or sensor configuration cannot recover it from a loaded model
and must read the source document itself. Preserving these losslessly is planned; until it lands,
treat a meios load as lossy for everything outside the URDF kinematic and visual surface.

**An inertia tensor is checked for mathematical admissibility, never for physical plausibility.**
meios refuses a negative mass, a negative moment of inertia, a tensor that is not positive
semi-definite, and one that violates a triangle inequality on its diagonal — all properties of the
tensor alone. It has no model of the link's shape and will not guess one. A tensor that satisfies
every rule and is off by three orders of magnitude for the body it describes passes, and so does one
whose principal axes point somewhere the geometry cannot support. Checking a tensor against its
geometry is a job for a consumer that owns both.

**A repeated element is silently ignored.** Where a document carries two `<origin>` elements on one
joint, two `<inertial>` elements in one link, or a second `<axis>`, `<limit>`, `<mimic>`,
`<geometry>`, `<parent>` or `<child>`, meios reads the first and discards the rest with no diagnostic
at any level. The specification says nothing about how many of these an element may carry, and the
reader has never counted. This is the one silent acceptance left in the URDF reader.

**Under a permissive document-validity setting a partially-valid element is lost whole.** The default
setting refuses the document. Lower it and the same defect drops the element that contained it
instead: an `<inertial>` whose `<mass>` cannot be read leaves the link with no inertial at all rather
than with the tensor that was present, and a `<visual>` whose geometry cannot be read is not added to
the link. That is the accepted cost of never completing a value nobody wrote — a link with no
inertial is a fact you can see and act on, while a link carrying a zero mass nobody authored is
indistinguishable from a real one.

**`load_into` stages the model before it pushes.** The facade that drives your own `model_sink` from
a path loads into meios's `model` first and walks that into your sink only once the load has
succeeded. That is what makes the atomicity guarantee free — a failed load never touches your sink —
but it means peak memory holds both representations at once, and a description large enough for that
to matter is better served by loading and walking in two explicit steps you control. The staged
`model` also carries only the robot's name, so a sink driven through the facade receives a
`robot_info` whose version and extension collections are empty whatever the document declared.

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

**Unresolved meshes do not stop the reporting verbs.** `info`, `tree`, and `complete` lower
`on_missing` to `warn`, because none of them renders asset bytes and `info`'s mesh listing is the
report of what did *not* resolve. `flatten`, `bundle`, and `deps` keep the refusing default: each
produces an artifact that names or needs the asset.

## What the description corpus proves

**The blocking corpus reaches one vendor.** Every rule about what a description may say is held
against pinned, real robot descriptions, and a rule that refuses one of them turns a pull request
red. What that gate covers, exactly, is the four top-level descriptions
`ros-industrial/kuka_experimental` ships — named outright in the build, never found by globbing a
directory. That is one vendor and one authoring style. A description written in some other house
style can still meet a refusal that nothing here would have caught.

**A second vendor is covered only on the scheduled run, and only on Linux.**
`UniversalRobots/Universal_Robots_ROS2_Description` expands only through the Python evaluator,
because its macros subscript mapping values the built-in evaluator cannot. No macOS or Windows build
carries that evaluator, and building it forces the install option off, so that upstream cannot join
the always-on leg without taking the install-consumer coverage with it. It is loaded on the
scheduled run instead, which does not block a pull request.

**A third vendor is named nowhere because it ships nothing to load.**
`lbr-stack/lbr_fri_ros2_stack` is deliberately not fetched: its published tarball contains no robot
description of any kind. Every top-level document in it includes description packages that are not
inside the tarball. Pinning the description packages themselves would be a different upstream with
its own license determination, and it has not been done.

**Fragments are not loaded, by design.** Macro and include files carrying a `<robot>` root with no
name and no links are not top-level documents, and the corpus does not treat them as such. That
means the rules are proven against assembled descriptions only; a refusal that would fire on a
fragment loaded directly is not exercised.

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
