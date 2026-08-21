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

**A relative asset path anchors to the top-level input document, not to the file it was written in.**
After include and macro expansion a description is commonly assembled from many files, and a
`<mesh filename="meshes/base.stl">` written inside an included file is still measured against the
directory of the document you handed `load()`. Where those two directories differ, that reference does
not resolve. Anchoring to the writing file needs a map from an expanded node back to the document that
produced it, and none is built. The `package://` form is unaffected, and it is what real descriptions
overwhelmingly use. The full rule is in [asset resolution](asset-resolution.md).

**A source that serves bytes leaks its scratch tree on a platform that will not delete an open file.**
Such a source materializes an asset into a scratch directory it owns and removes the tree when it is
destroyed. Removal stops at the first error, so a still-open handle to a file inside the tree leaves
the directory behind rather than corrupting or half-deleting it. What you see is an abandoned
temporary directory, not a damaged one.

**There is a narrow window between creating a scratch root and narrowing its permissions.** The
directory is created with an unpredictable name and its permissions are tightened immediately after; a
process watching the system temporary directory during that interval sees a directory with default
permissions. Nothing has been written into it yet. The window is the whole exposure: a root whose
permissions could not be narrowed at all is removed and refused rather than served from.

**Narrowing that root to its owner replaces permission bits, and not every platform decides access
that way.** Where the real access decision is an access-control list, the call leaves that list
untouched and narrows nothing, so the root's protection there rests on the per-user temporary
directory it was created inside and on the unpredictable name. Treat those two as the exposure on such
a platform rather than reading the narrowing call as a guarantee it cannot give everywhere.

**The guard that refuses an entry offered into the publication staging directory folds ASCII case,
and that fold has run on no case-folding filesystem.** The comparison folds because a differently-cased
leading component names the same real directory where the filesystem folds case. Every run to date has
been on a case-sensitive filesystem, so the fold is correct by construction and unmeasured. A second
guard, at publication rather than at the offer, refuses a target another entry's file already
occupies. It compares the files themselves where it can and the canonicalized paths where it cannot:
a recorded file removed from outside the source cannot be compared as a file, and to that comparison
absence reads as difference. The path comparison answers an aliasing spelling a canonicalization
collapses onto the recorded one. It does not answer a spelling that a canonicalization keeps distinct
while the recorded file is absent — a differently-cased package on a volume that folds case is that
shape — so that combination is the one the guard still cannot see. The guard itself has since run
where case folds, on APFS and on NTFS, and refuses the differently-cased package there while the
recorded file is present. It is the absence, not the folding, that remains unexercised.

**Creation beneath a resolved temporary directory is not driven through a source.** What a source does
when the system temporary directory itself does not exist is driven end to end — pointing the
temporary-directory environment at a path that does not exist is the steering, and what runs is the
diagnostic the source raises and the refusal that follows. The narrower branch, where the temporary
directory resolves and the root creation beneath it fails, is driven against the creation step
directly — with an unusable parent, and with scripted candidate names and a scripted narrowing outcome
behind the operations seam — but never with a source above it.

**A byte-backed source resolves the system temporary directory to its real path once, when it creates
its scratch area.** The root it reports and every asset path it derives are therefore the same kind of
path, and a resolved asset lies under the root its own resolution names. Before that, the root was
reported with whatever spelling the environment carried: where the system temporary directory is
reached through a symbolic link, that spelling is not a prefix of the paths derived beneath it, so a
consumer relating a resolved path to the root it was served from — the relation the reported root
exists for — got the wrong answer and refused. That is measured rather than hypothetical: an installed
consumer performing exactly that relation refused on both platforms whose temporary directory is
reached through a link. The relationship is now driven by a case that points the temporary-directory
environment at a symbolic link, so the shape is reproduced on any host rather than waited for.

**What has been driven natively, and where, stated exactly.** The candidate-collision sweep, the
occupancy discrimination and the setup-precedence cases have been run and have passed on Linux, on
macOS and on Windows, with the two symlink-occupancy forms compiled out on the platform that has no
symbolic links and nothing reported as skipped in their place. The publication and replacement cases
have been run and have passed on all three as well. The cause-carrying publication step genuinely
differs by platform and each value is its own system's: Linux reports 20, "Not a directory"; macOS
reports 17, "File exists"; Windows reports 183, "Cannot create a file when that file already exists."
The installed consumer — a duplicate refusal, a replacement and rematerialization on one stable path,
a cause-carrying publication failure, and the scratch teardown — has been built against an installed
package and run directly on all three, and it is the run that refused on the two platforms whose
temporary directory is reached through a symbolic link, on the containment relation described above
and on nothing else. **The resolution that closes that refusal has been observed on this project's
development platform only.** A green consumer run on the other two platforms at a revision carrying it
is an observation that has not been made, and nothing here should be read as claiming it.

**Whether a replacement over a destination somebody still holds open succeeds is a platform fact, and
it is measured on POSIX only.** The case asserts whichever branch its host takes rather than assuming
one; on Linux the rename succeeds, the pre-existing handle keeps reading the prior bytes and a reopen
reads the replacement. On the other two platforms the case runs and passes, but which branch it took
is not retained: a passing case's output is not kept, and both branches pass. **No native value for
the refusal has been recorded on any platform**, so the input a retry bound would need does not exist
yet. No bound is derived from it, and none appears anywhere in the publication path.

**A directory-backed source makes its configured root absolute but does not resolve it.** Where that
root is reached through a symbolic link, the root such a source reports and the paths it derives are
again not the same kind of path, so a consumer relating one to the other gets the same wrong answer
the byte-backed source used to give. The fix shape is identical and it has not been applied there.
Until it is, treat a directory-backed source's reported root as a spelling of where that source was
configured rather than as a prefix a resolved path can be measured against.

**Whether containment over-refuses on Windows is unmeasured.** The containment check canonicalizes
both paths and compares them lexically, folding no case and expanding no short (8.3) name of its own.
Whether a candidate that differs from its root only in case, or that arrives in short form, is
therefore refused despite genuinely sitting inside that root depends on how much the standard
library's `weakly_canonical` normalized first — and no test exercises it on any platform. The
direction is safe either way: the comparison can only refuse a path it should have accepted, never
the reverse. Suspect it first behind an unexplained containment refusal on Windows.

**Containment is enforced at resolution, not at every subsequent copy.** An asset path is checked
against the configured package roots and the input document's directory once, when it is resolved.
Whether a component that later consumes a resolved path re-checks what it handles is that component's
own business, so do not read the containment rule as an end-to-end guarantee about every file that
ends up somewhere.

## Diagnostics

**A successful `load()` does not mean a clean load.** A value on the success arm tells you meios
produced a model, not that the document was clean. A `<visual>` naming a material nothing defines
under a `warn` material policy, a topology issue under a `warn` topology policy, and an unresolved
`package://` reference under a lowered `on_missing` all pass without failing the load. A material
*collision* is not among them: a duplicate name is reported outside the policy tiers and fails the
load at every setting. The result carries every diagnostic the document raised, alongside the three
completeness claims; branch on those rather than on the presence of a value. The failure arm carries
the same list beside the error that names the failure. The diagnostics are ordered by the reader's
own pass — every material, then every link, then every joint — which is not the order a document
that interleaves them was written in.

**Two of the five policies still leave their claim standing when set to `skip`.** A completeness claim
answers for the document rather than for the log, so silencing a class of diagnostic must not restore
the content it reported. The document-validity, material and missing-asset policies hold to that: a
drop they silence withdraws its claim all the same. `topology_policy::skip` and `eval_policy::skip` do
not. Both suppress at a layer with no reach into the claim accumulator, so a graph defect silenced by
the first leaves `topology_valid` set on a model whose graph was never checked, and a substitution left
verbatim by the second leaves `parsed` set on a document that still carries an unevaluated expression.
Closing them means threading the accumulator through the topology reconstruction and the substitution
scanner. Until then, do not set either of those two to `skip` and then branch on the claim it governs;
`warn` costs nothing but the diagnostic and keeps both claims honest.

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

**An unreadable `<axis>` on a `fixed` or `floating` joint leaves a zero vector in a field nothing
reads.** The wiki states those two kinds do not use the axis, so this profile applies no rule to it
there. An `xyz` whose component count is wrong is reported under the document-validity policy and
withdraws the parse claim, but the joint keeps the zero vector the reader seeded, which is
indistinguishable from the `<axis xyz="0 0 0"/>` this profile accepts on those kinds. On the four
kinds that do use the axis the same zeros reach the zero-axis rule and the joint is refused at every
setting, so what remains is confined to a field the profile already states nothing reads.

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
reader has never counted.

**An empty fixed-length numeric attribute is silently accepted.** A `<origin xyz="  "/>`, a `<mesh
scale=""/>` or a `<color rgba=""/>` carries zero tokens where the reader wants three or four. It is
not reported at any level: the reader treats "no tokens at all" as "attribute not written" and keeps
the default the caller seeded, so the document reads as though the attribute were absent. A wrong
*count* is reported and withdraws the parse claim; only the empty case passes quietly. Together with
the repeated element above, these are the two silent acceptances left in the URDF reader.

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
property named `format` is refused the moment it enters a composed expression. This backend is off by
default and nothing reaches it unless a build asks for it: the [evaluation
guide](evaluation.md) leads with the built-in grammar that actually runs, and carries this backend's
subset, its four refusal rules and `meios::unrestricted_python_evaluator` — which applies none of
those rules and is reachable only from C++ — in a section of its own at the end. The built-in
evaluator has no such exposure. Its grammar is closed rather than restricted: it has no interpreter to
widen, no object protocol and no way to name a file, and it loud-fails on anything outside what it
carries.

**The Python backend has no bound on resource exhaustion.** Under `meios::eval-python` an expression
such as `${10**10**10}` or `${[0]*10**12}` is refused by nothing — it names no withheld builtin,
traverses no attribute and touches no file — and will burn processor time and memory. The
restriction above is about authority (the filesystem, the network, the process), never about
availability. A real bound needs a per-expression watchdog against an embedded interpreter holding
the interpreter lock, portable across macOS, Linux and Windows; there is none today. The built-in
evaluator, which is the default path, does not share this: eleven finite ceilings bound one load, and
both expressions above fail loudly there — the first on an integer range check, the second because
the grammar has no list literal to multiply.

**Python xacro expressions are recovered by literal re-parsing.** With the Python evaluation
enrichment enabled, the result of a Python expression is re-hydrated by re-parsing its literal form.
A result that is not expressible as a Python literal is therefore not representable through this seam;
the principled opaque-value path is not yet in place.

## Expression evaluation

**Every way this evaluator differs from canonical xacro is written down, and twelve of them are
measured.** The reviewed manifest carries twelve rows and the differential drives them on every push in
both directions, so a divergence that quietly stopped reproducing fails the comparison as loudly as a
new one does. Most of them are a meaning upstream has and this grammar refuses:

- A string repeated by an integer, and one string ordered against another. Only the addition of two
  strings and equality between two were measured into this grammar.
- `split()` with no separator, which collapses runs of whitespace and drops the leading and trailing
  fields, and `split(sep, n)` with a count. One separator, given explicitly, is the whole of what the
  named split takes.
- The mapping constructor built from a sequence of pairs, and from a positional argument mixed with
  keyword ones. `dict(a=1, b=2)` is the one admitted shape.
- The remainder operator against a string, which formats upstream: `${'%.3f' % 1.2345}` renders there.
- An auxiliary document whose anchor contains an alias to itself. Upstream's reader builds a value that
  contains itself; a value here is immutable once constructed, so the alias meets an anchor that is
  still incomplete and refuses.
- A member name the reference's own mapping wrapper answers before it consults the document —
  `${config.keys}` and the ten others like it. Here that spelling refuses and names `${config['keys']}`
  instead, because answering it would mean something other than what upstream means by it.
- The mathematics names reached through a `math` namespace, and the reference's own `xacro.arg`,
  `xacro.tokenize` and message helpers, which are not exposed at all.
- `map` and `filter`, which are not among the functions this grammar carries — the omission most likely
  to be met in a real description.

Two differ in the other direction, rendering here where upstream refuses: a closing brace inside a
string literal, and one expression span written inside another. Both follow from a span scanner that
folds over quotes and counts depth where the reference's pattern does neither. And two render on both
sides with different answers: `and` and `or` yield a boolean here rather than the deciding operand, so
`${1 or 2}` is `True` here and `1` there. That one is deliberate and measured, not an oversight.

The last three entries above are the ones the manifest does not carry, because none of them can be
matched by exact text: the colliding member names produce upstream text embedding an object's own
address, and the other two are absences rather than divergent renderings. They are recorded here and on
the evaluation page instead. The colliding names are checked against a committed measurement of what
the reference's wrapper answers, and the `math` namespace refuses by a named case; the argument and
message helpers and the two missing functions refuse as any unrecognized name does, and nothing names
them individually.

**An expression inside a discarded block argument is never evaluated here, and is evaluated upstream.**
Where a macro takes a block argument and then drops it — `<xacro:if value="${off}">` around the
`<xacro:insert_block>` — the reference expands that block's contents at the *call* site, before the
receiving macro decides anything, while meios expands them only if the block is inserted. On a
well-formed description the two agree, because the work is discarded either way; the whole rendered
document is compared against upstream's on every push, and one pinned description takes exactly this
path. What differs is a description that is *not* well formed there: a block argument naming a property
nobody bound fails the load upstream and loads clean here. So a description validated only against
meios can carry a defect in a discarded block that the reference will refuse. This one is not in the
reviewed manifest — it was measured after that record was last written — and the direction is the
permissive one, which is why it is written down here rather than left to be met.

**A mapping key an auxiliary document did not write as text loads, and nothing can read it.** A
document may key a mapping by a number, a boolean or an empty scalar, and it parses — the key is
admitted, it counts toward the mapping's extent, and it collides with another key exactly as the
reference's own comparison would have it collide. What no expression has is a spelling that reaches it:
every subscript key is text, and a dotted member is a name. So such an entry is present and unreadable
rather than refused at the parse, and a description that needs one needs the explicit backend.

**The string operations compare and count by bytes where the reference counts code points.** Substring
containment, the named split, the addition of two strings and a string's truth value all work over
bytes. The two agree over the ASCII text every measured description carries. Whether any real
description carries non-ASCII text through one of them is unmeasured, so this is written down rather
than claimed absent.

## Command-line behavior

**`info` and the completion engine fail loudly on a broken topology.** On a robot whose graph does
not reconstruct — a link with more than one parent, or a cycle — `meios info` exits non-zero with no
output and the completion engine yields no completions, rather than printing a partial view. This
follows from silent-by-default plus the default `fail` topology policy. `meios tree` uses a `warn`
policy and still prints what it can. An *undeclared* link is not in that set: it is reported outside
the policy tiers and returns from the reader before any record is emitted, so it defeats `tree` too,
at every setting.

**Unresolved meshes do not stop the reporting verbs.** `info`, `tree`, `validate` and the completion
engine lower `on_missing` to `warn`, because none of them renders asset bytes and `info`'s mesh
listing is the report of what did *not* resolve. `flatten`, `bundle`, and `deps` keep the refusing
default: each produces an artifact that names or needs the asset.

**The completion engine is not a verb you can type.** The verb `meios completion` only prints a
shell script. What that script calls back into is `meios __complete`, which is hidden, carries no
help entry, and is the thing the paragraphs above mean whenever they name the completion engine.

## What the description corpus proves

**The blocking corpus reaches four vendors.** Every rule about what a description may say is held
against pinned, real robot descriptions, and a rule that refuses one of them turns a pull request
red. What that gate covers, exactly, is seventeen top-level documents named outright in the build,
never found by globbing a directory: `ros-industrial/kuka_experimental`'s `kr6r900sixx.xacro` and the
four pre-expanded descriptions beside it, five variants of
`UniversalRobots/Universal_Robots_ROS2_Description`, the single entry point
`lbr-stack/med14_r820_description` carries, and six of the eight `frankarobotics/franka_description`
ships. Four of those seventeen — the pre-expanded KUKA set — sit behind a breadth option that the
blocking job turns on and a plain local build does not, so a developer running the corpus by hand
sees thirteen. Four vendors is not every authoring style, and a description written in some other
house style can still meet a refusal that nothing here would have caught.

**Two of the pinned family's own entry points do not load.** All eight documents
`franka_description` ships were rendered against the pinned upstream and compared; the six that are
corpus documents load and agree, and the two that are not — its two-arm and mobile two-arm
assemblies — build their arm list with a bracket literal and then take a slice of it. Neither
spelling is read here, so both refuse rather than loading with the wrong model. A description
assembling its parts that way is outside what the corpus proves.

**The corpus exercises two of the asset reference forms.** Nearly every asset reference in it is a
`package://` URI or a `$(find …)` substitution. The absolute `file://` form is covered by one
document — the Universal Robots entry point loaded with `force_abs_paths=true`, whose recorded mesh
rows are `file://` throughout — and no shipping document carries a relative reference at all. So
what the corpus leaves to a case table of crafted documents is the containment rule and the
relative base, not the whole URI contract.

**The comparison against a fresh upstream render is confined to Linux.** Every corpus document
the built-in evaluator handles is expanded by it, with the interpreter binding switched off, and
both that comparison and the pinned corpus run on every push. What is confined to Linux is the
render each is compared against: producing one needs the pinned upstream tooling, and only the Linux
workflow installs it. macOS and Windows load the same pinned documents natively and check them
against recorded facts, so what those two platforms leave unproven is agreement with a freshly
rendered upstream, not whether the documents load.

**The third vendor is pinned as a description package, not as its umbrella repository.**
`lbr-stack/lbr_fri_ros2_stack` is deliberately not fetched: its published tarball contains no robot
description of any kind, and every top-level document in it includes description packages that are
not inside the tarball. What is pinned in its place is `lbr-stack/med14_r820_description` at
`v2.5.0`, with its own license determination and one loadable entry point. The remaining description
packages that umbrella repository refers to are still unpinned, so the authoring styles they carry
are not covered.

**One upstream revision is pinned in the build but absent from the recorded pins.**
`tests/golden/oracle/PINS` names the upstream tooling, `ur_description`,
`lbr_med14_r820_description` and `franka_description`. It does not name `kuka_experimental`, whose
commit and archive digest are pinned in the build alone — so the KR6 measurement is recorded against
a revision that record does not state.

**Fragments are not loaded, by design.** Macro and include files carrying a `<robot>` root with no
name and no links are not top-level documents, and the corpus does not treat them as such. That
means the rules are proven against assembled descriptions only; a refusal that would fire on a
fragment loaded directly is not exercised.

## The CMake resource modules

**The acquisition tests never reach the network.** Every acquisition case builds its own origin on
the local disk, so nothing in that set exercises a transport failure, a redirect, a certificate
problem, or a host that serves different bytes than it did last week. The split is deliberate: the
tests that must pass on every machine cannot depend on a third party being up. Two paths do reach a
real host — the corpus acquisition, behind its own option, and the example build, behind
`MEIOS_EXAMPLE_FETCH_NETWORK`, which defaults on — so a configure on a machine with no network fails
in the examples unless that option is turned off.

**The `GITHUB` short form is exercised only through the examples.** It rewrites into `URL` or
`GIT_REPOSITORY`, and both of those are exercised offline. The rewrite itself needs a real host, and
what reaches one is the example build: it declares a `GITHUB` resource behind
`MEIOS_EXAMPLE_FETCH_NETWORK`, which defaults on and is left on in every workflow, so the composed
archive URL and the `STRIP_TOP_LEVEL` it implies do run against the real host on all three
platforms. What no run covers is `GITHUB` combined with `SPARSE_PATHS`.

**`MEIOS_RESOURCE_TLS_CAINFO` is exercised by nothing.** Forwarding a CA bundle to the download
needs an origin served over TLS, which nothing offline can be. On a machine whose CMake ships
without a trust store that variable is the documented way through, and it is also the one thing on
this page with nothing at all standing behind it.

**Flattening is not proven from an installed package.** That an installed meios carries acquisition
and deployment to a `find_package` consumer is proven on all three platforms — the install-consumer
example is built, installed, and run — but no automated run flattens a description through an
installed meios; the tests reach the modules through the module path instead.

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
loudly rather than shipped as stub geometry. The LFS half of that is measured: a case clones a local
origin carrying an unsmudged pointer and asserts the refusal — though the origin is a plain
repository holding pointer text, not one with real LFS objects behind it. Nothing clones a
repository carrying a submodule, so what happens there is still reasoned about.

**Nothing in the automated set cross-compiles.** `meios_target_flatten_resource` refuses a cross
build that has not been handed a host-runnable binary, and both that refusal and the path it points
at — a flatten driven by a binary built for the build host — are reasoned about rather than
measured. If you cross-compile and flatten, you are the first to do it.

**How the build integration finds the command-line tool is proven for two of the three ways it can
be found.** A test hands the module an explicit path to a binary, another hands it nothing and
checks that it refuses, and a third builds a project carrying a `meios` target and asserts that the
binary that target names is the one that ran. The route left unexercised is picking the tool up from
an installed package. That is one of the routes that motivated exporting the tool as a target rather
than recording a path string, because a target resolves to the right binary on a generator that
builds several configurations out of one project — and for the installed case that property is
reasoned about, not measured.

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
