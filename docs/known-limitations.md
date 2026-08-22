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

**Every way this evaluator differs from canonical xacro is written down, and fourteen of them are
measured.** The reviewed manifest carries fourteen rows and the differential drives them on every push
in both directions, so a divergence that quietly stopped reproducing fails the comparison as loudly as
a new one does. Nine of the fourteen are a meaning upstream has and this grammar refuses — the first
five entries below, and the mapping key an auxiliary document did not write as text, which has an
entry of its own further down:

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

Three differ in the other direction, rendering here where upstream refuses: a closing brace inside a
string literal, one expression span written inside another, and the absolute value. The first two
follow from a span scanner that folds over quotes and counts depth where the reference's pattern does
neither, so the reference truncates the span and never hands its evaluator a whole expression to
refuse. The third is a difference in what the evaluator itself carries: the reference's expression
globals hold the mathematics module and the two reducers and no builtins at all, so `${abs(-3)}`
renders `3` here and raises an undefined name there, while every other name on this grammar's
mathematics list renders on both sides. And two render on both sides with different answers: `and` and
`or` yield a boolean here rather than the deciding operand, so `${1 or 2}` is `True` here and `1`
there. That one is deliberate and measured, not an oversight.

The last three entries above are the ones the manifest does not carry, because none of them can be
matched by exact text: the colliding member names produce upstream text embedding an object's own
address, and the other two are absences rather than divergent renderings. They are recorded here and on
the evaluation page instead. The colliding names are checked against a committed measurement of what
the reference's wrapper answers, and the `math` namespace refuses by a named case; the argument and
message helpers and the two missing functions refuse as any unrecognized name does, and nothing names
them individually. A fourth difference the manifest cannot carry is the separator a located path is
written with, further down: it is observable on Windows alone, and the job that renders a fresh
upstream to compare against runs on Linux, where both implementations write the same character.

**A block argument's contents now take effect where the caller wrote them, and a description that
relied on them not doing so will refuse.** A macro's block argument is expanded once, at the call site,
in the caller's macros and scope — which is what the reference does, and which this evaluator did not
do until recently: it used to expand a block only if the receiving macro inserted it. Two consequences
are visible on descriptions that load today. A property, an include or a nested macro call written
inside a block the receiving macro never inserts now takes effect anyway. And a block argument naming a
property nobody bound now fails the load whether or not the block is ever reached. The change moves
this evaluator toward the reference rather than away from it, so nothing the reference accepts is
newly refused here — but a description that was only ever validated against meios, and that hid a
defect inside an unreachable block, will stop loading. There is no shim, alias or migration note: this
project is pre-release and the behavior was changed outright.

**An argument is readable by its bare name in an expression, where upstream has no such name.** This
evaluator keeps one symbol table; the reference keeps two, an argument table the `$(arg n)` command
reads and a property table an expression reads. So `${flag}` naming an argument resolves here and
raises an undefined-name error there, for the whole spelling — a description written against meios
can therefore carry a read the reference will refuse. The permissive direction is the older of the
two facts here; what such a name binds is the second. An argument binds the characters its author
wrote, so a bare name reading one reads text: `${not flag}` over a `false`-spelled default answers
the truth of a non-empty string, not the truth of a boolean. Reach an argument through the `$(arg n)`
command and both implementations agree, so long as no property of that name has been written. Binding
it to a property does *not* restore agreement here and is the second half of the same divergence: an
argument and a property share one binding table on this side and are two contexts upstream, so a
property written over an argument's name changes what `$(arg n)` answers here and changes nothing
there. Measured: an argument defaulted to `A` with a property of the same name written `B` renders
`Barm` here and `Aarm` upstream.

**A `$(arg n)` standing above its own declaration resolves here and refuses upstream.** The main
expansion pass is single-forward, so a pre-pass seeds every literal default a document declares
before the pass begins; a use written above its own `<xacro:arg>` element therefore resolves. The
reference reads its argument table in document order and refuses the whole document with an
undefined-substitution-argument diagnostic. The acceptance is deliberate and is pinned by cases, but
the direction is the permissive one: a description that relies on it loads here and fails there.

**A mapping key an auxiliary document wrote as a number or a boolean loads, and nothing can read
it.** Such a key is admitted, it counts toward the mapping's extent, and it collides with another key
exactly as the reference's own comparison would have it collide. What no expression has is a spelling
that reaches it: every subscript key is text, and a dotted member is a name. So the entry is present
and unreadable rather than refused at the parse, and a description that needs one needs the explicit
backend. An **empty** scalar written as a key is a different answer, and this page gave the wrong one
until it was measured: a document writing the explicit-key form with nothing after it does not parse
here at all — the reader meets a mapping standing where a key belongs and refuses the document at that
line and column. So an empty key is refused rather than admitted-and-unreadable, and the sentence above
covers the two kinds it names and no third.

**A located path is written with forward separators on every platform, where the reference writes the
platform's own.** `$(find)` and `$(dirname)` render the path they resolve in the generic spelling, so a
document reads the same separator on Windows as it does elsewhere. The reference writes the native one,
which on Windows puts a backslash inside the document text — and where the command sits inside an
expression span, that text is resolved before the expression is lexed, so the backslash lands inside a
string literal. A backslash in a literal is refused here rather than given the escape meaning nothing
measured, so the native spelling would make a description that loads everywhere else refuse on Windows
alone. The generic spelling is accepted by the platform's own filesystem interfaces and is what a mesh
URI carries in any case. The reviewed manifest cannot carry this one. A row there is matched by exact
text against a render the pinned reference produces at that moment, and the job producing that render
runs on Linux, where the reference writes the same separator this project does — so on the platform
where the difference exists there is no fresh render to compare against, and on the platform that
renders there is no difference. What carries it instead is the set of unit assertions that compose the
expected path in the generic spelling and therefore hold on every platform the suite runs on,
including the one that would otherwise write a backslash.

**The string operations compare and count by bytes where the reference counts code points, and no
description can tell the difference.** Substring containment, the named split, the addition of two
strings and a string's truth value all work over bytes. That was written down here as an unmeasured
risk; it has since been measured, by driving all four through both implementations over text carrying
multi-byte characters, and the two agree character for character. They have to. Well-formed UTF-8 is
self-synchronizing, so a byte-wise search for a well-formed needle cannot match across a character
boundary and a byte-wise split cannot cut one in half; byte equality and code-point equality coincide
exactly; and none of the four operations exposes a length or an index, because this grammar has no
string-length function and no string subscript. A byte sequence that is not well-formed text cannot be
written into an XML document in the first place. The difference is real in the implementation and has
no spelling that reaches it from a description.

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

**The blocking corpus reaches five vendors, plus one description this project wrote itself.** Every
rule about what a description may say is held against pinned, real robot descriptions, and a rule that
refuses one of them turns a pull request red. What that gate covers, exactly, is twenty top-level
documents named outright in the build, never found by globbing a directory:
`ros-industrial/kuka_experimental`'s `kr6r900sixx.xacro` and the four pre-expanded descriptions beside
it, six documents of `UniversalRobots/Universal_Robots_ROS2_Description` — five variants of its main
entry point and a second entry point the same package ships — the single entry point
`lbr-stack/med14_r820_description` carries, six of the eight `frankarobotics/franka_description`
ships, the seven-joint variant of `Kinovarobotics/ros2_kortex`, and a two-file gantry authored in this
repository because no surveyed vendor writes a merge key into a file a description loads. Four of
those twenty — the pre-expanded KUKA set — sit behind a breadth option that the blocking job turns on
and a plain local build does not, so a developer running the corpus by hand sees sixteen. Five vendors
is not every authoring style, and a description written in some other house style can still meet a
refusal that nothing here would have caught.

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

**The comparison against a fresh upstream render is confined to Linux, and so is the corpus that
feeds it.** Every corpus document the built-in evaluator handles is expanded by it, with the
interpreter binding switched off, and both that comparison and the pinned corpus run on every push —
on Linux. Producing a fresh reference render needs the pinned upstream tooling and only the Linux
workflow installs it, which is the older half of this. The other half is that the corpus tier is a
Linux gate too. What macOS and Windows fetch the corpus for is the install-consumer job, and that job
loads two of the twenty documents — the `ur3e` variant and the KR6 — through a consumer built against
an installed package, checking them against the same recorded facts. So on those two platforms
eighteen of the twenty pinned documents are not loaded at all, and no statement here that a pinned
entry point loads on three platforms reaches further than that pair.

**The KUKA LBR family is pinned as a description package, not as its umbrella repository.**
`lbr-stack/lbr_fri_ros2_stack` is deliberately not fetched: its published tarball contains no robot
description of any kind, and every top-level document in it includes description packages that are
not inside the tarball. What is pinned in its place is `lbr-stack/med14_r820_description` at
`v2.5.0`, with its own license determination and one loadable entry point. The remaining description
packages that umbrella repository refers to are still unpinned, so the authoring styles they carry
are not covered.

**One upstream revision is pinned in the build but absent from the recorded pins.**
`tests/golden/oracle/PINS` names the upstream tooling, `ur_description`,
`lbr_med14_r820_description`, `franka_description`, `kortex_description`, and the in-repository gantry
by a digest of the two files that are the whole of it. It does not name `kuka_experimental`, whose
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
without a trust store that variable is the documented way through, and nothing here drives it. It is
not the only claim on this page carried by nothing; the index below answers *Nothing* for each one it
applies to, which is where to read them off rather than from a count in this sentence.

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

## What carries each claim

This page makes fifty-five claims — the bold sentence that opens each entry above — and this table
says, for every one of them, what would fail if the claim stopped being true. A claim is carried by a
case (named by the stem it lives in and its own wording), by a row in a committed record, or by
nothing at all. **Nothing at all is a legitimate answer here and it is written as one**, because a
good half of this page exists to say what is *not* measured; what is not legitimate is leaving a
reader to guess which kind a sentence is. Where an entry says *nothing*, the sentence above it is the
claim that nothing measures it, and the entry says what would have to be written to change that.

The pairing is not enforced by a test. It is checked by reading, and it is written down so the
reading can be repeated rather than redone from scratch by whoever next doubts a sentence.

### Sources and resolution

| Claim | Carried by |
|---|---|
| No runtime remote fetch | Nothing, and that is the claim: it is an absent capability. Every source the library has is driven by `io_sources.*` and `io_source_stack.*`, and none of them takes a URL. |
| Symlink-installed workspaces are covered by fixtures only | `ros_source.*` and `ros_prefix.*` over crafted layouts. Nothing builds a real `--symlink-install` workspace — which is the claim. |
| A relative asset path anchors to the top-level input document | `uri_cases.uri relative base survives a document named relatively`, and the rule marker the asset-resolution page publishes, held to the table by `uri_drift.the asset document and the uri table name the same rules`. |
| A byte-serving source leaks its scratch tree where an open file cannot be deleted | `io_sources.the scratch tree is removed when the source that owns it dies` and the `scratch_teardown.*` stems, which drive removal stopping at its first error. The platform that will not delete an open file is not one the suite runs on. |
| A narrow window between creating a scratch root and narrowing it | Nothing drives the window itself. Its two ends are carried: `scratch_setup.the candidate loop takes the first free name and exhausts at its bound` and `scratch_setup.a root that could not be narrowed is removed and refused`. |
| Narrowing replaces permission bits, and not every platform decides access that way | `io_sources.the scratch root is reachable by its owner alone`, on the platforms whose access decision *is* a permission bit. Nothing drives an access-control-list platform, which is the claim. |
| The staging-directory guard folds ASCII case, and that fold has run on no case-folding filesystem | `scratch_case_fold.a differently-cased package reaching one file is refused` — which now runs where case folds — and `scratch_alias.a publication onto a vanished entry's file is refused rather than silently taken` for the absence half. The combination of the two is what nothing drives. |
| Creation beneath a resolved temporary directory is not driven through a source | `scratch_unusable.*` drives the outer branch through a source; `scratch_setup.a collision ahead of a permanent creation failure reports the permanent cause` drives the narrower one against the creation step alone. The claim is the seam between them. |
| A byte-backed source resolves the temporary directory once | `scratch_symlinked.a resolution lies under the scratch root it reports when the temporary directory is a symbolic link` and `scratch_symlinked.the reported scratch root resolves the link rather than repeating it`. |
| What has been driven natively, and where | The three platform workflows and the install-consumer harness under `tests/integration/consumer`. The sentence about the resolution being observed on one platform only is carried by nothing, and says so. |
| Whether a held-open replacement succeeds is a platform fact, measured on POSIX only | `scratch_publish.a replacement with the destination held open is measured, not assumed` — it asserts whichever branch its host takes, which is why no native refusal value exists. |
| A directory-backed source makes its root absolute but does not resolve it | `io_sources.directory_source resolves an in-root file to a path` and `io_sources.a relative source root resolves an in-root asset and still refuses an escape`. Nothing drives the symlinked-root relation there — the claim. |
| Whether containment over-refuses on Windows is unmeasured | Nothing, on any platform. A case would hand the containment check a candidate differing from its root only in case, or in short form, and assert the verdict. |
| Containment is enforced at resolution, not at every subsequent copy | `io_sources.a containment decision on a candidate that is not absolute is refused`, `uri_cases.uri rows unsoftened containment refusal`. That a later consumer does not re-check is carried by nothing. |

### Diagnostics

| Claim | Carried by |
|---|---|
| A successful `load()` does not mean a clean load | `load_report.a successful load returns every diagnostic the document raised, not the first`, `model_diag.a document that said nothing wrong claims everything`, and the duplicate-material exception by `urdf_profile`'s `duplicate_material_name` row. |
| Two of the five policies leave their claim standing under `skip` | Half carried: `load_report.a document the reader could not read whole still reports a valid topology` pins the topology half. The evaluation half — `parsed` surviving a span left verbatim — is asserted by no case; `eval_policy.a leading python-only span is left verbatim under skip and warn` drives the span but not the claim. |
| Nothing is printed unless you ask | `silent_default.a default load writes nothing to cerr or cout`, and `model_diag.default log_sink is a silent no-op on both overloads`. |

### Modeling gaps

| Claim | Carried by |
|---|---|
| Extension fragments are dropped, not carried through | `urdf_vocabulary.each recognized extension block is disclosed under its own code` and `urdf_material.a dropped robot-level ros2_control child raises a loud WARN naming it`. |
| An unreadable `<axis>` on a `fixed` or `floating` joint leaves a zero vector | `urdf_axis.a zero axis on a fixed joint loads clean through the whole entry point`, against `urdf_axis.a zero axis on a turning joint is refused at a real location`. |
| An inertia tensor is checked for admissibility, never for plausibility | `urdf_inertia.*` — six cases over the tensor rules, including `a real tensor sits orders of magnitude inside the boundary`. That plausibility is unchecked is the absence those six leave. |
| A repeated element is silently ignored | **Nothing.** The published rule table carries no row for it, because a row states a diagnostic and this raises none, and no case asserts the silent acceptance either. |
| An empty fixed-length numeric attribute is silently accepted | **Nothing**, for the same reason. The wrong-*count* half is carried by `urdf_fields.a non-finite value is refused under a code rather than under none` and the profile table's own rows; the empty case is not. |
| Under a permissive setting a partially-valid element is lost whole | `profile_permissive.a visual whose origin the reader refuses is dropped whole` and `urdf_fields.a required part that is missing drops its containing element at every setting`. |
| `load_into` stages the model before it pushes | `load_into.a failed load leaves every counter on the sink at zero`, `load_into.an unopenable path leaves the sink untouched too`, `load_into.the success summary agrees with the entry point it delegates to`. |
| The Python evaluator runs a restricted subset with false refusals | `eval_python_refusal.a scope-bound name spelled format is refused all the same`, `eval_python_refusal.an expression reaching past arithmetic refuses under a named rule`, and the `eval_python_abuse.*` stem. |
| The Python backend has no bound on resource exhaustion | **Nothing** — there is no bound to drive. The contrast is carried: `native_ceilings.*` and `native_limits.*` drive the built-in evaluator's eleven, and `xacro_depth.*` and `xacro_budget.*` the expansion three. |
| Python xacro expressions are recovered by literal re-parsing | `eval_python_container.an authored literal-shaped property value stays a string under subscript`, `eval_python_container.a set is refused rather than emitted as a container it is not`, and the `eval_python_container_table` stem. |

### Expression evaluation

| Claim | Carried by |
|---|---|
| Fourteen measured differences | The fourteen rows of `tests/golden/oracle/differential_divergences.cases`, driven in both directions by `native_differential`. The count on the page and the row count of that file are the same number and are meant to be compared. |
| A block argument's contents take effect where the caller wrote them | `xacro_block.a property written inside a block the macro never inserts takes effect at the call site` and `xacro_block.a block argument naming an unbound property fails the load where the macro discards it`, over the minimized document both implementations were driven through. |
| An argument is readable by its bare name | `xacro_arg_domain.an argument read by its bare name renders the characters that were written` and `xacro_arg_domain.a bare name computing on an argument computes on the text that was written`. No manifest row: the upstream side is an undefined-name failure of the whole document rather than a differing render. |
| A `$(arg n)` above its own declaration resolves | `xacro_arg.a use above its own declaration still resolves the default` and `xacro_arg_domain.a use above its own declaration renders the same spelling the declaration seeds`. No manifest row, for the same reason. |
| A non-text mapping key loads and nothing reads it | The `non_text_mapping_key` row of the divergence manifest, driven as a whole document through both implementations, plus `native_yaml.a key is read in any of the scalar kinds a document can write`. The empty-scalar sentence beside it is carried by the reader's own refusal, measured while that row was minimized. |
| A located path is written with generic separators | The assertions in `xacro_subst_command` that compose an expected path in the generic spelling, and `xacro_arg.a nested arg default resolves at declaration`, which is where the last native spelling was found. No manifest row is possible, and the entry says why. |
| The string operations count bytes | Measured through both implementations over multi-byte text while this page was swept: the four operations agree, so there is no divergence for a row to carry. `native_string.membership against a string haystack tests substring containment` and `native_split.the named split yields the fields between separators, the empty ones included` carry this side's behavior. |

### Command-line behavior

| Claim | Carried by |
|---|---|
| `info` and the completion engine fail loudly on a broken topology | `cli_verbs.info exits nonzero and prints each failure's typed code exactly once`, `cli_tree.tree exits nonzero and prints each failure's typed code exactly once`, and `cli_tree.an unknown --root link exits nonzero and renders nothing`. |
| Unresolved meshes do not stop the reporting verbs | Half carried. The four lowering sites are one assignment each in the verbs' own sources, and `cli_table_drift: resolve names every accepted asset form and deps carries the override surface` holds the surface; no case drives a reporting verb over a document whose mesh does not resolve. |
| The completion engine is not a verb you can type | `cli_verbs.completion bash equals the golden and calls __complete`, `cli_table_drift: the completion verb is part of the shared surface`, and the three `completion_emit` golden cases. |

### What the description corpus proves

| Claim | Carried by |
|---|---|
| Five vendors, twenty documents | `cmake/corpus_documents.cmake`, which names every one outright, and the sixteen rows of `tests/golden/oracle/compatibility_ledger.cases`, which the ledger test pairs against the listfile in both directions — a pinned document with no row fails, and a row naming no pinned document fails. |
| Two of the pinned family's own entry points do not load | **Nothing drives them**, because they are deliberately not corpus documents. The measurement that put them outside is recorded in the listfile beside the six that are in. |
| The corpus exercises two of the asset reference forms | The `file://` rows of `ur5e_abs_paths_facts.cases`, and `uri_cases.uri rows accepted` for the forms the corpus does not reach. |
| The fresh-render comparison, and the corpus, are Linux gates | The three platform workflows, read against each other: one installs the pinned upstream tooling and registers the corpus tier, and the other two fetch the corpus only for the install-consumer job. |
| The KUKA LBR family is pinned as a description package | Its `PINS` row and its license determination in `cmake/corpus.cmake`. That the umbrella repository's other packages are unpinned is carried by their absence from the listfile. |
| One upstream revision is pinned in the build but absent from the recorded pins | Nothing asserts the absence — it *is* the claim. Both halves are readable: `tests/golden/oracle/PINS` and the fetch in `cmake/corpus.cmake`. |
| Fragments are not loaded, by design | The listfile's own rule, and the count it states: pointing the selection at the macro extension would load twenty-one documents carrying a `<robot>` root with no name and no links. |

### The CMake resource modules

| Claim | Carried by |
|---|---|
| The acquisition tests never reach the network | `tests/integration/cmake/meios_cmake_origin.cmake`, which builds every origin on local disk. That no transport failure is exercised is the absence that leaves. |
| The `GITHUB` short form is exercised only through the examples | `cmake_declare_url_hash` and `cmake_declare_git_sparse` offline, and the example build behind `MEIOS_EXAMPLE_FETCH_NETWORK` for the rewrite. `GITHUB` beside `SPARSE_PATHS` is carried by nothing. |
| `MEIOS_RESOURCE_TLS_CAINFO` is exercised by nothing | **Nothing.** No case, no row and no run: forwarding a CA bundle needs a TLS origin, which nothing offline can be. |
| Flattening is not proven from an installed package | `cmake_flatten_install_destination` and `cmake_flatten_install_component` install a flattened document; both reach the modules through the module path, which is what leaves the claim standing. |
| `PACKAGE_PATH` precedence is reasoned about, not measured | `cmake_flatten_package_path_reaches_vendored_package` carries the adds-reach half. Two roots offering one package name is carried by nothing. |
| Flattening with the Python backend is only really run on Linux | `cmake_flatten_eval_python_live` and `cmake_flatten_eval_python_statement` where the enrichment is built; `cmake_flatten_eval_python_refusal` where it is not. |
| Submodules and large-file objects are left alone | `cmake_declare_lfs_pointer_refusal` and `urdf_mesh.a resolved mesh that is an unsmudged Git-LFS pointer is rejected, not accepted`. Nothing clones a repository carrying a submodule. |
| Nothing in the automated set cross-compiles | **Nothing**, by construction. A case would need a toolchain file and a host-runnable binary. |
| Tool discovery is proven for two of three routes | `cmake_flatten_runs_a_cli_target` and `cmake_flatten_no_cli_refusal`. The installed-package route is carried by nothing. |
| An unrecognized evaluator name is refused by the build and accepted by the tool | `cmake_flatten_eval_unknown_refusal` for the refusal, and the three `cmake_eval_vocabulary` cases holding the module's backend list to the binary's. The tool's silent acceptance is asserted by nothing. |

### API stability

| Claim | Carried by |
|---|---|
| No deprecation cushion before `v1.0.0` | The absence of any deprecation attribute in the shipped headers, and `version.*`, which holds the declared version to the one the package config exports. |
