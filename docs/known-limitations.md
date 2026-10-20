# Known limitations

meios never fails silently, and that discipline extends to its own status: this page is the candid
list of the defects and caveats that are live right now. Each is described by the effect you
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

**A symlink-installed workspace is measured on crafted layouts opened through the install tree, not on
a live colcon build.** A colcon `--symlink-install` workspace links its installed files back into the
source tree, either as one linked directory or as one link per file. Both shapes are built by hand and
a description is opened through `install/`. On both, a relative include, a relative auxiliary document
and a relative mesh beside the document resolve with the default options, because a file beside a
document is admitted under the directory the document was named in and under the one it really lives
in, and the install's links lead into the second. A sibling linking into neither of those nor into a
configured package root — a file the build generated and the install linked in, say — refuses in all
three unless the caller follows outside links, which the entry on outside links below describes. A
package-qualified reference is judged against its package root where its link leads, and under a root
at `install/share`, whose package files link into the source tree, the links lead out of the root.
Measured on both shapes with that root, a `package://` mesh refuses under `uncontained_asset`, and a
`package://` or `$(find)` include and a `package://` auxiliary document each raise an
`uncontained_asset` record before the load refuses, the include under `unresolved_include` and the
auxiliary document as unreachable; a `$(find)` auxiliary document refuses there as well. The pinned
reference reads the `$(find)` include and the `$(find)` auxiliary document through `install/share` on
both shapes, because it opens a path wherever its links lead. The remedy is to configure the source
tree as the root, which lets a description read every file under it: on both shapes each of those
references resolves once `src` is the root. A root that is itself a link is read through the directory
it leads to, so a root given as a link to the source tree serves them too. The ROS layer registers a
package whose directory is a link under the directory the link leads to. Through `ROS_PACKAGE_PATH`, a
package file that is a link staying inside its package reads as it does through a configured root, a
package under an entry that is itself a link reads, and a package file whose link leads out of its
package refuses, with an `uncontained_asset` record naming the package and the path within it. Nothing
builds a real `--symlink-install` workspace, so what a real build lays down beyond these shapes is not
covered.

**A relative asset path anchors to the top-level input document, not to the file it was written in.**
After include and macro expansion a description is commonly assembled from many files, and a
`<mesh filename="meshes/base.stl">` written inside an included file is still measured against the
directory of the document you handed `load()`. Where those two directories differ, that reference does
not resolve. Anchoring to the writing file needs a map from an expanded node back to the document that
produced it, and none is built. The `package://` form is unaffected, and it is what real descriptions
overwhelmingly use. The full rule is in [asset resolution](asset-resolution.md).

**A relative include resolves beside the document that writes it, as the reference resolves it, and
refuses to climb above that directory unless the caller admits the climb.** The reference joins
`<xacro:include filename="arm.xacro"/>` onto the directory of the file that writes it, at the path that
file was named by, and so does this evaluator: inside a macro body the anchor is the document that
calls the macro, a nested include reads the file beside the include that names it, and a document
opened through a link anchors at the link rather than at the file the link points to. A document a
package lookup served, `package://` or `$(find)` through a directory, bundle or ROS layer, anchors the
same way, at the path the lookup named it by, `root/package/relative`, even where that path is a link;
the file the link leads to stays its identity, the file its cycle check keys and its read opens.
Measured on a package file that is a link into another directory, each of the two directories holding a
same-named sibling, its nested include and its auxiliary document read the sibling beside the named
path, which is the file the reference reads through `$(find)`. A document a memory source served is
read beside the file that source materialized for it. Where nothing is beside a served document, its
sibling is asked of the layer that served it, under that document's package and directory. A file
beside the document that another directory admitted, a configured root reached through a link say,
carries no record of that layer, so a miss beside that file is not answered from the layer: measured
with the serving layer's directory holding the missed name, the load refuses, as the reference does. A
filename that climbs out of the document's directory refuses here as climbing above the directory of
the document that writes it: measured on a document whose included file includes
`../include_parent_directory.leaf.xacro`, upstream renders the property that file binds and here the
load refuses under `xacro_structural_error`, which is the `include_parent_directory` row the manifest
carries. An auxiliary document's leading `..` is not held by that gate: it is admitted wherever it
lands under one of the directories the outside-link entry below lists, so it loads under a configured
root holding the file, as the reference loads it, and with no configured root refuses as unreachable
where the reference reads it. A caller that sets `include_options::climb_above_document`, on
`load_options::includes` or as the trailing argument of `meios::expand`, admits the climb as the
reference reads it: a spelling that leaves the document's directory is joined as written onto it and
read where the operating system resolves it, and a climb that names nothing refuses without consulting
a configured source. That same document then renders as upstream renders it, which is the
`include_parent_directory_climbed` verdict. Where a served document's admitted climb is answered is
decided by the type of the layer that served it, and the layer is asked nothing to decide it. A layer
whose type names no paths, a memory source, holds what it served only as the bytes it materialized, so
the climb in such a document is answered by that layer alone, inside the document's package, and loads
or refuses the same whatever was included before it; a climb out of the package has an entry of its own
below. A directory, bundle or ROS layer names paths, and an admitted climb in a document one of them
served is read where the operating system resolves it, as a document opened from disk is: measured
through the ROS layer alone, a climb inside the package and one out of it each read the file the
reference reads. That holds through a beside link into another configured root: measured on a package
file whose sibling is a link into a second configured root and climbs from there, the load reads the
file the climb names from the link's own directory with no error record, and the reference reads the
same file. A `..` that stays inside the directory has an entry of its own below too. Admitting the
climb is a security choice, not a parity one. A description can then name any file the process can
open, short of the package bound a byte-served document keeps, so set it only for descriptions you
trust as much as the process itself.

**A `..` that climbs back inside the document's directory is read where the operating system resolves
it, and refused through a path element that is not a directory, as the reference refuses it; two shapes
still differ.** A filename such as `sub/../x.xacro` does not leave the document's directory as written.
An include, an auxiliary document and a mesh each join it onto that directory as written, as the
reference joins an include or an auxiliary document, and judge it where the operating system resolves
it, by the rule the outside-link entry below states, with `climb_above_document` set or unset. With
`sub` a link to a directory under a configured package root, each of the three reads the file the link
leads to, and a file the include reads anchors its own relative includes at the spelling as written.
With `sub` a link to a directory under nothing that admits it, each refuses unless the caller follows
outside links, and then reads where the link leads. Measured against the pinned reference with `sub` a
link to a directory elsewhere, an `x.xacro` beside the document and another one level above where the
link leads, the reference renders the property the second binds, and so does this side with outside
links followed. Where a path element exists but is not a directory, `f.txt/../x.xacro` or
`f.txt/x.xacro` with `f.txt` a regular file, the operating system reports that it is not a directory,
and an include and an auxiliary document refuse on that, with `..` or without it, as the reference
refuses all four with `[Errno 20] Not a directory`; a mesh spelled so does not resolve, and its record
carries the same reason. No configured root's copy stands in for such a spelling, as the fallback entry
below states. At the command line an auxiliary document refused so prints only that it cannot be
reached; the reason is in the load's diagnostics, which the command line does not print. Two shapes
still differ. Where `sub` does not exist, the standard library canonicalizes the missing tail by its
spelling ([fs.op.weakly.canonical]), so `sub/..` cancels and the file beside the document is read here,
by all three readers; measured the same way, the reference refuses an include or an auxiliary document
of it for want of `doc/sub/../x.xacro`. And where `sub` is a link and nothing is at the spelling as the
operating system reads it, that miss takes the lookups the fallback entry below describes, and they
look for the spelling's lexical reduction, `x.xacro`: measured with `sub` a link to `a/b`, nothing at
`a/x.xacro` and a configured root holding `x.xacro`, an include and an auxiliary document of
`sub/../x.xacro` render the root's copy here, whether or not an `x.xacro` stands beside the document,
and refuse with no configured root, while the reference refuses each for want of `doc/sub/../x.xacro`.
The same holds in a document a package lookup served: measured with an include of `sub/../x.xacro` in
it and an `x.xacro` beside the path it was named by, that file renders here, and the reference refuses.

**Under the climb option, a climb out of a byte-served document's package refuses, whatever was
included before it.** A byte-served document is one served by a layer whose type names no paths: such a
layer holds the document only as the bytes it materialized. Whether a layer names paths is read from
its type, and the layer is asked nothing to decide it. A memory source names none. A directory, bundle
or ROS layer names paths, so a document one of them served is not byte-served, whatever is at the path
it was served from. With `climb_above_document` set, the layer that served a byte-served document is
the only place a climb in that document is answered. Inside the document's package the layer resolves
it; out of the package the include refuses under `xacro_structural_error`, naming the spelling and the
package, and never reads what an earlier lookup left in the source's scratch tree. Without that rule
the answer would depend on whether an earlier include had made the layer write the file. The reference
has no source layers, so this has no counterpart there to compare against.

**A link beside the document whose target lies under neither of the document's directories nor any
configured package root refuses, for an include, an auxiliary document or a mesh, unless the caller
follows outside links.** The reference opens an include or an auxiliary document beside its document
wherever a link standing there points. Here the three readers share one rule for a spelling that stays
inside the document's directory as written. A leading `..` is where they part, as the relative-include
entry above describes: an include's meets the climb gate first, while an auxiliary document's or a
mesh's is admitted wherever it lands under one of the directories this rule lists, so with no
configured root an auxiliary document's refuses where the reference reads it. A file beside a document
is admitted where its canonical target lies under the directory the document was named in, under the
one it really lives in, or under a configured package root, tried in that order; the first that
contains the target admits it, and the order decides only which directory is reported, never which file
is read. The configured roots are `load_options::package_roots` under `meios::load`, and the trailing
`package_roots` a caller hands `meios::expand`; with none, only the document's two directories admit. A
colcon `--symlink-install` tree opened through `install/` therefore resolves its siblings with the
default options, because they link into the directory the document really lives in, and a sibling
linking into a configured root is read through that root, which makes every file under the root
reachable from beside the document. A sibling linking anywhere else refuses in all three: an include
and a mesh under `uncontained_asset`, an auxiliary document with an `uncontained_asset` record before
the load refuses it as unreachable. No configured root's same-named copy stands in for an include or an
auxiliary document. Measured on a document whose included file includes `part.xacro`, a link to a file
one directory up: upstream renders the property that file binds and here the load refuses, which is the
`include_outside_link` row the manifest carries. A caller that sets
`include_options::follow_outside_links`, on `load_options::includes` or on `parse_context::includes`
when driving the parser directly, has all three take such a link where the operating system resolves
it, as the reference reads an include or an auxiliary document, and the same document then renders as
upstream renders it, which is the `include_outside_link_followed` verdict. With the option set, any
link on the spelling's way makes the followed answer final, a link that stays inside the document's
directory included: measured with `sub` a link to an empty directory beside the document and a
configured root holding `sub/leaf.xacro` and `sub/cfg.yaml`, an include of `sub/leaf.xacro` and an
auxiliary document of `sub/cfg.yaml` each refuse, where with the option unset each renders the root's
copy; the reference refuses both. Where nothing is at a followed link's target, the include refuses
under `unresolved_include`, naming the link it followed, and the auxiliary document as unreachable;
where an auxiliary document's target is a directory, it refuses under `cannot_open` with the operating
system's cause. In neither case does a configured root's same-named copy stand in. A dangling link is a
followed link with nothing at its end: with the option set, an include or an auxiliary document of one
refuses so, and a mesh of one is a missing asset, never a root's copy. A spelling that climbs out
lexically, or an absolute path landing outside, stays refused with the option set: the option admits
where a link leads, never where a spelling leads. With the option unset, a dangling link is what the
standard says `weakly_canonical` makes of it ([fs.op.weakly.canonical]): an absent file beside the
document. An include or an auxiliary document of one then takes the lookups that follow a miss, which
the next entry describes, and a mesh of one is a missing asset. Following links is the same kind of
security choice as admitting a climb: whoever can place a link beside a description can then have it
read any file the process can open.

**Where nothing is beside the document, a relative include is looked for through the configured
sources, which the reference never consults, so a document the reference refuses can render here.** The
reference reads a relative include beside its document or not at all. Here a miss beside the document
falls back to the configured sources, and a file one of them holds renders where the reference refuses
for want of it. For a document a source layer served, the miss is first asked of that layer, under the
document's package and directory, and only then of the configured sources. A file read beside such a
document passes its own misses to that layer too only where the directory the document was named in
admitted it; one another directory admitted, a configured root reached through a link say, does not,
and its misses go to the configured sources alone. The manifest carries no row for this, because the
whole-document comparison configures no source on either side, and with none configured both sides
refuse alike; the include tests carry it instead. The fallback answers a genuine miss, and only that. A
spelling through a path element that is not a directory, `f.txt/x.xacro` or `f.txt/../x.xacro`, refuses
before any fallback, as the reference refuses it, even where a configured root holds the name: measured
with a configured root holding both `x.xacro` and `f.txt/x.xacro`, an include of either spelling
refuses, and so does an auxiliary document of either, while an auxiliary document of a name genuinely
absent beside the document still reads from the root. A miss through a link beside the document is a
genuine miss: where nothing is at `sub/../x.xacro` as the operating system reads it, the lookups ask
for its lexical reduction, `x.xacro`, and a root's copy renders, as the `..` entry above measures. A
dangling link beside the document, with outside links unset, is one case of nothing beside it, as the
entry above states: an include of it falls through to the serving layer and the configured sources, and
an auxiliary document of it to the configured roots, so a root's same-named copy renders where the
reference refuses for want of the link's target. That is a remaining divergence from the reference.
Setting `include_options::follow_outside_links` makes such a link refuse as the reference does, since a
followed link's answer is final. Two anchors belong to the same rule. A document expanded under no
name, handed to `meios::expand` with an empty document path, looks for nothing beside it and goes
straight to the configured sources, where the reference's library entry anchors such a document at the
working directory. And a document named by a relative path anchors its relative includes at the working
directory, as the reference's does. No pinned corpus document reaches an include that is not
package-qualified, so none of them is affected.

**A relative auxiliary document written in a description a byte source served resolves only once that
source has written the file.** A source that serves bytes writes an entry into its scratch tree the
first time that entry is looked up, and hands back the path of the file it wrote. The auxiliary-document
reader resolves a relative spec by probing the filesystem beside the document that reads it and then
each configured containment root; it does not ask the layer that served the document. Measured on a
source holding `pkg/urdf/a.xacro` and `pkg/urdf/c.yaml`: a relative `c.yaml` asked for beside the
served `a.xacro` refuses as unreachable, and the same request after `pkg/urdf/c.yaml` has itself been
looked up returns its text. So whether such a spec resolves depends on whether something looked the
file up first. Includes no longer behave so, since a relative include in such a document is asked of
the layer that served it; the auxiliary-document reader does not do that yet. A spec in the package
form, `package://` or `$(find)`, asks the source stack and is unaffected.

**An absolute include filename is looked for only through the configured sources, with its leading
separator removed, where the reference reads the file it names.** The reference opens
`<xacro:include filename="/opt/robot/arm.xacro"/>` at the path written. Here an absolute spelling is
never opened directly: its leading separator is dropped, the rest is looked for as a relative path
under each configured source, and where no source holds it the load refuses under
`unresolved_include`, whose message says the configured sources were the only place looked. That keeps
an include inside the roots the caller configured rather than letting a description name any file on
the host. Measured on a document that includes the file beside it by an absolute path, built from an
argument each side binds to the directory it reads the document from: upstream renders the property
that file binds and here the load refuses, which is the `include_absolute_path` row the manifest
carries. `$(find <pkg>)/...` is the package spelling the reference also reads for an include,
`package://` is the subject of the next entry, and no pinned corpus document writes an absolute
include.

**A `package://` include filename resolves here through the configured sources, where the reference
refuses it.** The reference does not read `package://` in `<xacro:include filename>` as a package
reference. It joins the spelling onto the directory of the including document as a relative path and
refuses with "No such file or directory", naming a path such as `./doc/package://p/a.xacro`. Here the
spelling is split at its first separator into a package and a path within it and resolved through the
configured sources, as a package-qualified include. A description written against this library with
`package://` includes therefore loads here and fails under the reference; `$(find <pkg>)/...` is the
spelling both read. Measured on a document whose include names a package this side's comparison serves
from the fixture directory: upstream refuses and here the included property renders, which is the
`include_package_filename` row the manifest carries. An auxiliary document behaves the same way: the
reference refuses `xacro.load_yaml('package://p/cfg.yaml')` with "No such file or directory", naming
`doc/package://p/cfg.yaml`, while here the file is read through the configured sources. No manifest row
carries that half, because the document probes read auxiliary documents through a loader that consults
no source.

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

**Whether containment over-refuses on Windows is unmeasured.** The containment check canonicalizes
both paths and compares them lexically, folding no case and expanding no short (8.3) name of its own.
Whether a candidate that differs from its root only in case, or that arrives in short form, is
therefore refused despite genuinely sitting inside that root depends on how much the standard
library's `weakly_canonical` normalized first — and no test exercises it on any platform. The
direction is safe either way: the comparison can only refuse a path it should have accepted, never
the reverse. Suspect it first behind an unexplained containment refusal on Windows.

**Containment is enforced at resolution, not at every subsequent copy.** An asset path is checked
against the configured package roots and the input document's directories once, when it is resolved.
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

**Twenty-three ways this evaluator differs from canonical xacro are measured, and further ones no row
can carry are written down beside them.** That is not a claim that the list is complete. The reviewed
manifest carries twenty-three rows and the differential drives them on every push in both directions, so
a divergence that quietly stopped reproducing fails the comparison as loudly as a new one does.
Thirteen of the twenty-three are a meaning upstream has and this grammar refuses — the first five entries
below, the argument helper the seventh names, the mapping key an auxiliary document did not write as
text, which has an entry of its own further down, and an include climbing above its document's
directory, an absolute include filename and a link beside a document leading out of its directory,
whose entries are under sources and resolution:

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
- The mathematics names reached through a `math` namespace, and the reference's own `xacro.tokenize`
  and message helpers, which are not exposed at all. The reference's `xacro.arg`, which reads an
  argument by name inside an expression, is not exposed either, and it is the one of these the
  manifest carries: `${xacro.arg('p')}` over an argument defaulted to `A` renders `Aarm` upstream and
  refuses here, naming the `$(arg)` command.
- `map` and `filter`, which are not among the functions this grammar carries — the omission most likely
  to be met in a real description.

Eight differ in the other direction, rendering here where upstream refuses: a closing brace inside a
string literal, one expression span written inside another, the absolute value, an argument declared
without a default, a `$(arg n)` written above its own declaration, a block parameter a call leaves
without an element, `$(dirname)`, and a `package://` include filename. The first two follow from a
span scanner that folds over quotes and counts depth where the reference's pattern does neither, so
the reference truncates the span and never hands its evaluator a whole expression to refuse. The third
is a difference in what the evaluator itself carries: the reference's expression globals hold the
mathematics module and the two reducers and no builtins at all, so `${abs(-3)}` renders `3` here and
raises an undefined name there, while every other name on this grammar's mathematics list renders on
both sides. The next four have entries of their own further down, and the last has one under sources
and resolution. And two render on both sides with
different answers: `and` and `or` yield a boolean here rather than the deciding operand, so
`${1 or 2}` is `True` here and `1` there, which is deliberate and measured, not an oversight.

The last three entries above, the argument helper apart, are the ones the manifest does not carry,
because none of them can be matched by exact text: the colliding member names produce upstream text
embedding an object's own address, and the other two are absences rather than divergent renderings.
They are recorded here and on the evaluation page instead. The colliding names are checked against a
committed measurement of what the reference's wrapper answers, and the `math` namespace refuses by a
named case; a call to the tokenizing or message helpers refuses naming `xacro.load_yaml` as the only
helper either evaluator exposes, the two missing functions refuse as any unrecognized name does, and
nothing names them individually. A fourth difference the manifest cannot carry is the separator a located path is
written with, further down: it is observable on Windows alone, and the job that renders a fresh
upstream to compare against runs on Linux, where both implementations write the same character.

**A block argument's contents now take effect where the caller wrote them, and a description that
relied on them not doing so will refuse.** A macro's block argument is expanded once, at the call site,
in the caller's macros and scope — which is what the reference does, and which this evaluator did not
do until recently: it used to expand a block only if the receiving macro inserted it. Two consequences
are visible on descriptions that load today. A property, an include or a nested macro call written
inside a block the receiving macro never inserts now takes effect anyway. And a block argument naming a
property nobody bound now fails the load whether or not the block is ever reached. Measured against
the pinned reference on minimized documents, each renders or refuses here as it does there: a block
the macro never inserts that carries a property, a macro call writing a global property, or a macro
call emitting a link renders the property, the global and no emitted link on both sides; one carrying
an include renders the included property on both sides with no source configured; and a block naming an
unbound property refuses on both sides, under a `*` parameter never inserted and under a `**`
parameter inserted only behind a false condition. So a description that was only ever validated
against meios, and that hid a defect inside an unreachable block, will stop loading, as it never
loaded upstream. There is no shim, alias or migration note: this
project is pre-release and the behavior was changed outright.

**A block parameter a call leaves without an element renders here when the macro never inserts it,
where the reference refuses the call.** The reference expands a macro call's own children first and
then takes one element for each `*` or `**` parameter, and refuses the whole call with "Not enough
blocks" for any parameter left without one. Here a block parameter binds whatever the expanded call
holds, and only inserting a block nobody supplied refuses. Measured against the pinned reference on
minimized documents, a macro taking `*content` and never inserting it refuses upstream and renders
here when its call holds nothing but an include of a document that binds only a property, when the
call holds only a `<xacro:property>`, and when the call is empty; a `**content` parameter called with
only such an include does the same. Where the macro does insert the block, both sides refuse, under
different causes: "Not enough blocks" upstream, and here `xacro_structural_error` naming the unknown
block. The direction is the permissive one, so a call that loads here can fail there. The include
case is the `include_sole_block` row the manifest carries; the other shapes are the same rule, and the
measurement above is what records them.

**An argument is read only through the substitution commands, as the reference reads it, and a
description that named one bare now refuses.** Arguments and properties are two binding tables here,
as they are upstream: the `$(arg n)` command and the `$(eval)` command read the argument table, an
expression span reads the property table, and a property of an argument's name is a second binding
beside the argument, never a write to it. This evaluator used to keep one table for both, so
whichever wrote a name last answered both reads. Two consequences are visible on descriptions that
load today. A property's `default=` now binds over
an argument of the same name, and a property's `value=`, a global property or a macro parameter of the
same name leaves what `$(arg n)` answers alone. Measured against the pinned reference, which renders
each the same way: an argument `p` defaulted to `A` under a property `p` defaulted to `B` renders
`${p}base_$(arg p)arm` as `Bbase_Aarm`, and as `Bbase_Carm` with the caller argument `p:=C`; written
with `value=`, before or after the argument's declaration, it renders `${p}_$(arg p)` as `B_A`, and a
global property written by a macro as `G_A`; and a macro parameter named like an argument leaves
`$(arg prefix)` reading the argument. The vendor idiom `<xacro:property name="x" default="$(arg x)"/>`
therefore binds the property it was written to bind. And an expression that named an argument by its
bare name — plainly, in a conditional, under `not`, or as a dotted head — now refuses under
`undefined_property`, under either evaluator; a bare `^` default whose name only an argument binds
refuses under `xacro_structural_error`, while `^|fallback` takes its fallback as the reference does;
and `$(arg n)` naming only a property refuses under `unresolved_arg`. Each refusal names the
`$(arg n)` spelling, and the reference refuses all three. `$(eval)` evaluates its body as upstream's
does: over the arguments, each one it names converted by the reference's own conversion — a real where
the text holds a `.`, an integer otherwise, either boolean spelling in any letter case, the text itself
last, with digits and surrounding spaces read as Python 3.14's `int()` and `float()` read them — after
the reference's own names, its helpers, the boolean spellings `true` and `false` and the mathematics
names, which shadow an argument of the same name, and with no property visible. The digits, the spaces
and the mathematics names are tables taken from Python 3.14 and its Unicode 16.0 data; the reference
running on another interpreter reads whatever that interpreter's tables hold, so a name or a digit
added or retired between versions is read here as 3.14 reads it. A body holding a double underscore
refuses before anything is evaluated, as it does upstream. Its body
ends at the first closing parenthesis, as the reference's command pattern ends it, so a call, a
parenthesized subexpression or a `$(arg n)` written inside it refuses on both sides. Every other
command ends at its first closing parenthesis too, and a `(` or a `$` left in its text once its spans
are resolved refuses, as the reference's command scanner refuses it: `$(arg zz $(arg q))`,
`$(arg p $(arg q))` over a bound `p`, `$(find $(arg pkg))`, `$(optenv NAME (a))`, `$(optenv NAME x$y)`
and an `$(optenv NAME ${w})` whose property holds `a(b` refuse on both sides, and `$(arg ${w})` reads
the argument `w` names, as it does upstream: a span-built name is resolved before its words are
counted, and a second word, written or produced, refuses under `unresolved_arg` before any argument is
looked up, on both sides. Measured against
the pinned reference, which renders each the same way: an argument `q` defaulted to `3` beside a
property `q` written `10` renders `$(eval q + 1)` as `4`; an argument `p` defaulted to `A` renders
`$(eval p + 'arm')` as `Aarm`, and as `Carm` with the caller argument `p:=C`; a conditional testing
`$(eval flag == 'yes')` over a `yes` default takes its branch; the defaults `1.50`, `TRUE`, `1e3`,
`1_000` and ` 7 ` evaluate to `1.5`, `True`, `1e3`, `1000` and, plus one, `8`; arguments named `e`,
`true` and `len` evaluate to the mathematics constant, `True` and the argument; `١٢` plus one is `13`,
`١.٥` is `1.5`, a `7` padded with a no-break space and an ideographic space is, plus one, `8`, and
`²`, which is no decimal digit, stays text; `$(eval a__b)` over an argument `a__b` refuses; and a name
only a property binds refuses. Three readings upstream has refuse here instead, measured but not carried by
the manifest: the names `inf` and `nan` under the built-in evaluator, a name the reference binds to a
function, which renders as the function's own text there, and an argument whose number is outside the
64-bit range, which upstream reads as a Python integer of any size or a real that overflows to `inf`.
Those three apart, every form this entry names renders as the reference renders it or refuses where
the reference refuses, as measured above; the sixteen pinned corpus documents flatten byte for byte
as they did before the change, and none of them reaches `$(eval)`. There is no shim, alias or
migration note: this project is pre-release and the behavior was changed outright.

**An argument declared without a default is accepted here and refused upstream at the declaration.**
The reference requires a `default` attribute on every `<xacro:arg>` and refuses the whole document
without one. Here the declaration binds nothing: a caller supplies the value, or a `$(arg n)` reading
it refuses as unset, and a document that declares one and never reads it renders. Measured on a
document declaring `<xacro:arg name="p"/>` beside one link: upstream refuses with a missing-attribute
error and the link renders here, which is the row the manifest carries.

**A `$(arg n)` standing above its own declaration resolves here and refuses upstream.** The main
expansion pass is single-forward, so a pre-pass seeds every literal default a document declares
before the pass begins; a use written above its own `<xacro:arg>` element therefore resolves. The
reference reads its argument table in document order and refuses the whole document with an
undefined-substitution-argument diagnostic. The acceptance is deliberate and is pinned by cases, but
the direction is the permissive one: a description that relies on it loads here and fails there. The
manifest carries it as the `argument_above_declaration` row: a link named `above_$(arg flag)` written
above a `false` default renders `above_false` here, and upstream refuses the document.

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

**`$(dirname)` renders the directory of the file it is written in, where the reference refuses it
everywhere.** The pinned reference never hands its directory command a file, so `$(dirname)` refuses
there with "Cannot substitute $(dirname),no file/directory information available." wherever it is
written. Here it renders a directory. In the document handed to the load, it is the parent of that
path as it was handed in: relative where that path is relative, and empty for a bare file name.
Inside an include, it is the canonical directory of the file the include located, whichever route
found it (beside the including document, through `package://`, or through a configured source), and
it is the same from any working directory; an include reached through a linked directory names the
real directory, not the link. Measured on a document including `dirname_command.d/inner.xacro`, which
names a link after the last segment of its `$(dirname)`: upstream refuses, and here the link is named
`dirname_command.d`, which is the `dirname_command` row the manifest carries. The value is written into
the expanded document, so it is a path on the machine that ran the load. No pinned corpus document
uses the command.

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
Linux gate too. What macOS and Windows fetch the corpus for is the `clean-consumer` job, and that
job loads two of the twenty documents — the `ur3e` variant and the KR6 — through a consumer that
takes meios both by `find_package` against an installed package and by `FetchContent` from the
source tree, checking them against the same recorded facts. So on those two platforms eighteen of
the twenty pinned documents are not loaded at all, and no statement here that a pinned entry point
loads on three platforms reaches further than that pair.

**The KUKA LBR family is pinned as a description package, not as its umbrella repository.**
`lbr-stack/lbr_fri_ros2_stack` is deliberately not fetched: its published tarball contains no robot
description of any kind, and every top-level document in it includes description packages that are
not inside the tarball. What is pinned in its place is `lbr-stack/med14_r820_description` at
`v2.5.0`, with its own license determination and one loadable entry point. The remaining description
packages that umbrella repository refers to are still unpinned, so the authoring styles they carry
are not covered.

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
and deployment to a `find_package` consumer is proven on all three platforms — the `install-test`
job builds, installs and runs the `examples/install_consumer` program — but no automated run
flattens a description through an installed meios; the tests reach the modules through the module
path instead.

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

**Steps take turns only within one runtime directory of one build.** Every deploy and flatten step
holds a lock keyed on its target's runtime directory, where every write it makes lands, so targets
built into one directory never write at once. Two arrangements fall outside that key, and neither is
serialized: targets whose runtime directories nest, where one target's `SUBDIR` or `OUTPUT` reaches
into the other's directory, and two build trees writing into one directory. The lock orders nothing
either: two targets that write different content to one path end with whichever step ran last, and
that can differ from one build to the next. That no writer races another is shown by repetition,
which cannot prove a race absent; what is shown deterministically is that each step takes the lock and
waits on it.

## API stability

**No deprecation cushion before `v1.0.0`.** meios is pre-release. A superseded type or function is
deleted outright rather than marked `[[deprecated]]`, so an API you depend on can change or disappear
between versions with no compiler warning to cushion the transition. This holds until the `v1.0.0`
boundary, which is where the pre-release breaking-change license ends. Pin a revision if you need
stability in the meantime.

## What carries each claim

This page makes sixty-five claims — the bold sentence that opens each entry above — and this table
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
| A symlink-installed workspace is measured on crafted layouts, not on a live colcon build | Both layouts opened through the install tree: `load_linked_install.a relative include loads beside a document opened through a linked install directory`, `load_linked_install.a relative include loads beside a document opened through per-file install links`, the two `load_linked_install` auxiliary-document cases of the same shapes, `load_linked_mesh.a relative mesh resolves beside a document opened through a linked install directory` and `load_linked_mesh.a relative mesh resolves beside a document opened through per-file install links`. The sibling linking into neither directory: `load_linked_install.an install link leading into neither directory of its document refuses an include unless outside links are followed`, `load_linked_install.an auxiliary document linked into neither directory of its document refuses unless outside links are followed` and `load_linked_mesh.a mesh linked into neither directory of its document refuses with the default options`. The package-qualified reference through `install/share`: `load_linked_root.a package reference through a root whose files link out of it refuses as uncontained`, which links each file under `install/share` into the source tree and asserts the `package://` mesh's `uncontained_asset` refusal, the `package://` auxiliary document's refusal with its one `uncontained_asset` record, and both resolving once the source tree is the root, beside `xacro_include_read.a package include of a link pointing out of its configured root refuses as uncontained` for the include. The `$(find)` spellings, the linked-directory shape and the reference's readings are carried by the measurement the entry states, the `meios` command line and pinned xacro 2.1.1 on crafted trees of both shapes. The root that is itself a link: `load_linked_root.a package root given as a link serves its package includes`, `load_linked_root.a package root given as a link serves its package-form auxiliary documents`, `load_linked_root.a package root given as a link resolves a package mesh` and `load_linked_root.a directory source and a bundle source over a linked root report its real directory and read the asset through it`. The ROS layer's linked package directory: `ros_source.a colcon --symlink-install package dir resolves to the real files`. Its per-file links: `ros_package_link.an in-package link reads through ROS_PACKAGE_PATH as through a configured root`, `ros_package_link.a package under a ROS_PACKAGE_PATH entry that is itself a link reads` and `ros_package_link.a link leading out of its ROS package still refuses as uncontained`. Nothing builds a real `--symlink-install` workspace — which is the claim. |
| A relative asset path anchors to the top-level input document | `uri_cases.uri relative base survives a document named relatively`, and the rule marker the asset-resolution page publishes, held to the table by `uri_drift.the asset document and the uri table name the same rules`. |
| A relative include resolves beside its document and refuses to climb above it unless the caller admits the climb | The `include_parent_directory` row of the divergence manifest, driven as a whole document through both implementations, beside `xacro_include_resolution.a relative include climbing above its document's directory refuses as escaping`, which pins the row's bare refusal to its cause. The admitted climb is held to upstream by the `include_parent_directory_climbed` agreement verdict of `native_differential`, the same document loaded with the climb admitted and compared whole, through the load path and the expansion entry, beside `load_include_options.a caller admitting a climb reads an include above the document's directory`, `load_include_options.an admitted climb that names nothing refuses and consults no source` and `load_include_options.an admitted climb resolves through a linked directory as the operating system does`, which opens a document through a linked directory and climbs out of it, refused unset and read where the operating system resolves it admitted. The climb in a document a memory source served, inside its package, is carried by `load_include_options.a served document's climb inside its package resolves through its layer whatever was included first`, in both include orders. That the layer's type decides it, asking the layer nothing: `load_served_layer.a layer's type alone says whether it names paths`, over a directory, a bundle and a memory source, and a test-local source that names no paths. The climb in a document a bundle source served, read where the operating system resolves it: `load_include_options.a bundle-served document's admitted climb reads where the operating system resolves it`; through a beside link into a second configured root: `load_served_layer.a beside link into a second configured root climbs as the reference reads it, with no record naming a path the description never wrote`. The ROS layer's climb, in its package and out of it, is carried by the measurement the entry states, a program loading through that layer alone against pinned xacro 2.1.1; no case drives it. `load_include_options.the include options default to refusing a climb` pins the default. An auxiliary document's leading `..`: `load_beside_root.an auxiliary document's leading .. landing under a configured root loads, and refuses with none`, beside `load_beside_root.an include's leading .. landing under a configured root meets the climb gate`; the reference's reading of both is carried by the measurement the entry states. The resolution itself is held to upstream by five of the `include_` agreement verdicts (a file beside its document, a nested include, the same nested include with a configured root holding a same-named file, an include in a macro body, and a document opened through a link, whose includes resolve beside the link), beside the other `xacro_include_resolution`, `xacro_include_precedence` and `xacro_include_identity` cases. The anchor of a package-served document: `load_package_anchor.a document a package lookup served through an in-root link anchors its includes where it was named` and `load_package_anchor.a document a package lookup served through an in-root link anchors its auxiliary documents where it was named`, each by both package spellings, with `load_package_anchor.a package-served document reached through an in-root link is keyed by its canonical file` for the identity and `ros_package_link.a document the ROS layer served through an in-package link anchors its includes where it was named` for the ROS layer; the reference's reading through `$(find)` is carried by the measurement the entry states. The served document is carried by `xacro_include_served.a relative include in a document a byte source served resolves through that source` and `xacro_include_served.a served document's sibling resolves the same whether or not an earlier include wrote it`; a sibling where nothing is beside the named path by `load_served_layer.a sibling of a document served through an in-root link refuses where nothing is beside its named path`, and the file another directory admitted by `load_served_layer.a beside hit a configured root admitted carries no served record, so the serving layer does not answer its miss`. |
| A `..` that climbs back inside the document's directory is read where the operating system resolves it, and refused through a path element that is not a directory; two shapes still differ | Through a link to a directory under nothing that admits it, for the include: `load_include_options.a spelling back through a linked directory is judged where the operating system resolves it`, refused with the options unset and with the climb alone and reading where the link leads with outside links followed; for the auxiliary document and the mesh: `load_beside_root.an auxiliary document spelled back through a linked directory leading under no configured root refuses unless outside links are followed` and `load_beside_root.a mesh spelled back through a linked directory leading under no configured root refuses unless outside links are followed`. Through a link into a configured root: `load_beside_root.an include spelled back through a linked directory into a configured root reads where the link leads, and its nested include anchors there`, `load_beside_root.an auxiliary document spelled back through a linked directory into a configured root loads where the link leads` and `load_beside_root.a mesh spelled back through a linked directory into a configured root resolves where the link leads`. The path element that is not a directory, with `..` and without it: `xacro_include_read.an include spelled through a file that is not a directory refuses, and no configured source's copy stands in` and `yaml_resource.a spelling through a file that is not a directory refuses, and no configured root's copy stands in`, each over both spellings; the mesh: `load_linked_mesh.a mesh spelled through a file that is not a directory does not resolve, and its record names why`. The command line printing only the primary line is carried by the measurement the entry states. The directory that does not exist: `load_beside_root.a spelling through a directory that does not exist reads the file beside the document`, for all three readers, beside `load_include_options.a climb that stays inside the document's directory takes the ordinary lookups with the climb admitted` and `load_include_options.a served document's climb that stays inside its directory resolves through its layer with the climb admitted`. The miss through a link looked for under the lexical reduction is carried by the measurement the entry states, the `meios` command line against pinned xacro 2.1.1; no case pins it. The reference's readings, through the link, through the regular file and with no `sub`, are carried by the measurement the entry states; no manifest row compares them. |
| Under the climb option, a climb out of a byte-served document's package refuses | `load_include_options.a served document's climb out of its package refuses whatever was included first`, which asserts the refusal in both include orders over a memory source. The definition, read from a layer's type: `load_served_layer.a layer's type alone says whether it names paths`, which asserts that a directory and a bundle source name paths, that a memory source and a test-local source with no paths do not, and that asking logs nothing. A bundle source's document read from disk: `load_include_options.a bundle-served document's admitted climb reads where the operating system resolves it`; a directory source's, through a beside link: `load_served_layer.a beside link into a second configured root climbs as the reference reads it, with no record naming a path the description never wrote`. The ROS layer is carried by the measurement the relative-include entry states. |
| A link beside the document whose target lies under neither of its directories nor any configured root refuses unless the caller follows outside links | The `include_outside_link` row of the divergence manifest, driven as a whole document through both implementations, beside `xacro_include_resolution.a link beside the document that escapes it refuses without falling through`. The followed link is held to upstream by the `include_outside_link_followed` agreement verdict of `native_differential`, the same document loaded with outside links followed and compared whole, through the load path and the expansion entry, beside `load_include_options.a caller following outside links reads a sibling link that leaves the document's directory`; `load_include_options.the include options default to refusing an outside link` pins the default. The fixture's link is a committed symbolic link, so the comparison runs where the fresh-render job runs, on Linux. The one rule and a configured root's admission, for the three readers: `load_beside_root.an include linked beside its description into a configured root loads`, `load_beside_root.an auxiliary document linked beside its description into a configured root loads` and `load_beside_root.a mesh linked beside its description into a configured root resolves`; the roots an expansion is handed, and none: `load_beside_root.a beside include linking into the roots an expansion is given is read, and refused with none`. Where a leading `..` parts them: `load_beside_root.an include's leading .. landing under a configured root meets the climb gate` and `load_beside_root.an auxiliary document's leading .. landing under a configured root loads, and refuses with none`; the reference's reading of the auxiliary document by the measurement the relative-include entry states, and the mesh's admission by the measurement the [containment](asset-resolution.md#containment) rule states; no case pins the mesh's. The colcon sentence and the refusal in all three: the include and auxiliary-document cases of `load_linked_install`, `load_linked_mesh.a mesh linked into neither directory of its document refuses with the default options` and `load_linked_mesh.a mesh linked into neither directory of its document resolves with outside links followed`. The stand-in sentence for an auxiliary document: `yaml_resource_containment.an escaping link beside the document refuses even where a configured root holds the name` and `yaml_resource_containment.a configured root reached through a link reads the resource it contains`. The followed answer through a link that stays inside the document's directory, in both option states: `load_beside_root.a miss through a link that stays inside the document's directory is final with outside links followed` and `load_beside_root.an auxiliary document's miss through a link that stays inside the document's directory is final with outside links followed`; the reference's refusal by the measurement the entry states. The followed answer through a link leading out: `load_include_options.a followed directory link to an absent target refuses even where a configured root holds the name`, `yaml_resource_followed.a followed link to a directory lacking the spec ends the sweep where a configured root holds the name` and `yaml_resource_followed.a followed link naming a directory ends the sweep where a configured root holds the name`. The dangling link with the option set: `load_include_options.a dangling sibling link with outside links followed refuses even where a configured root holds the name` and `yaml_resource_followed.a dangling link with outside links followed ends the sweep where a configured root holds the name`; a dangling mesh link in both option states: `load_linked_mesh.a dangling mesh link beside its document is a missing asset, never a root's copy, in both option states`; with the option unset, the fallback entry's two cases. The spelling sentence: `yaml_resource_containment.a traversal spec stays refused with outside links followed` and `load_linked_mesh.a mesh path climbing out of its document's directories stays refused with outside links followed` for the lexical climb-out; no case sets the option over an absolute path landing outside, and `yaml_resource_containment.an absolute spec is judged by where it lands, not by how it looks` pins that refusal with it unset. |
| A relative include falls back to the configured sources where the reference refuses | No manifest row, because the whole-document comparison configures no source. `xacro_include_resolution.a relative include with nothing beside its document falls back to a configured source` and `xacro_include_served.a sibling the serving layer lacks falls back to the configured sources`; upstream has nothing to compare either with. The file another directory admitted, whose miss is not asked of the serving layer: `load_served_layer.a beside hit a configured root admitted carries no served record, so the serving layer does not answer its miss`. The spelling through a path element that is not a directory, refused before any fallback even where a configured root holds the name: `xacro_include_read.an include spelled through a file that is not a directory refuses, and no configured source's copy stands in` and `yaml_resource.a spelling through a file that is not a directory refuses, and no configured root's copy stands in`, each over both spellings, the second also reading a genuine miss from the root. The miss through a link looked for under the lexical reduction: the measurement the `..` entry states. The dangling link with outside links unset: `load_include_options.a dangling sibling link with outside links unset falls through to the configured sources` and `yaml_resource_followed.a dangling link with outside links unset falls through to a configured root`; the option's refusal by the followed dangling cases the outside-link entry names. The unnamed document is carried by `xacro_include_resolution.a document expanded under no name looks for nothing beside it`, and the relative document path by `xacro_include_resolution.a document named by a relative path anchors its relative includes at the working directory`. |
| A relative auxiliary document in a byte-served description resolves only once its source has written the file | `yaml_resource.a relative spec in a byte-served document resolves only once its source has written the file`, which asserts both the refusal before the file is written and the text after. |
| An absolute include filename is looked for only through the configured sources | The `include_absolute_path` row of the divergence manifest, driven as a whole document through both implementations, each binding the argument the include is built from to the directory it reads the document from, beside `xacro_include_resolution.an absolute include spelling is not looked for beside its document`, which pins this side's refusal and its message. |
| A `package://` include filename resolves here through the configured sources, where the reference refuses it | The `include_package_filename` row of the divergence manifest, driven as a whole document through both implementations, this side serving the fixture directory as a configured source for that probe alone. The auxiliary-document sentence is carried by the measurement the entry states, pinned xacro 2.1.1 against the `meios` command line with `--package-path` on one crafted tree; no manifest row compares it, because the document probes read auxiliary documents through a loader that consults no source. |
| A byte-serving source leaks its scratch tree where an open file cannot be deleted | `io_sources.the scratch tree is removed when the source that owns it dies` and the `scratch_teardown.*` stems, which drive removal stopping at its first error. The platform that will not delete an open file is not one the suite runs on. |
| A narrow window between creating a scratch root and narrowing it | Nothing drives the window itself. Its two ends are carried: `scratch_setup.the candidate loop takes the first free name and exhausts at its bound` and `scratch_setup.a root that could not be narrowed is removed and refused`. |
| Narrowing replaces permission bits, and not every platform decides access that way | `io_sources.the scratch root is reachable by its owner alone`, on the platforms whose access decision *is* a permission bit. Nothing drives an access-control-list platform, which is the claim. |
| The staging-directory guard folds ASCII case, and that fold has run on no case-folding filesystem | `scratch_case_fold.a differently-cased package reaching one file is refused` — which now runs where case folds — and `scratch_alias.a publication onto a vanished entry's file is refused rather than silently taken` for the absence half. The combination of the two is what nothing drives. |
| Creation beneath a resolved temporary directory is not driven through a source | `scratch_unusable.*` drives the outer branch through a source; `scratch_setup.a collision ahead of a permanent creation failure reports the permanent cause` drives the narrower one against the creation step alone. The claim is the seam between them. |
| A byte-backed source resolves the temporary directory once | `scratch_symlinked.a resolution lies under the scratch root it reports when the temporary directory is a symbolic link` and `scratch_symlinked.the reported scratch root resolves the link rather than repeating it`. |
| What has been driven natively, and where | The `install-test` job in the three platform workflows, which builds the consumer under `tests/integration/consumer` against an installed package and runs it directly. The sentence about the resolution being observed on one platform only is carried by nothing, and says so. |
| Whether a held-open replacement succeeds is a platform fact, measured on POSIX only | `scratch_publish.a replacement with the destination held open is measured, not assumed` — it asserts whichever branch its host takes, which is why no native refusal value exists. |
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
| Twenty-three measured differences | The twenty-three rows of `tests/golden/oracle/differential_divergences.cases`, driven in both directions by `native_differential`. The count on the page and the row count of that file are the same number and are meant to be compared. |
| A block argument's contents take effect where the caller wrote them | The six `block_` agreement verdicts of `native_differential`, each a minimized document loaded here and compared whole against a fresh upstream render, beside `xacro_block.a property written inside a block the macro never inserts takes effect at the call site` and `xacro_block.a block argument naming an unbound property fails the load where the macro discards it`. The include sentence is carried by `block_include_never_inserted`, one of the six. |
| A block parameter left without an element renders when the macro never inserts it | The `include_sole_block` row of the divergence manifest, driven as a whole document through both implementations, for the include instance. The other shapes the entry names, a call holding only a property, an empty call, a `**` parameter and the refusal on both sides where the block is inserted, are carried by the measurement the entry states and by nothing that runs. |
| An argument is read only through the substitution commands | Thirty-five of the agreement verdicts of `native_differential`: thirty-two minimized binding documents, three of them driven again with a caller argument, each loaded as a consumer loads it and compared whole against a fresh upstream render, a refusal on both sides agreeing only where upstream's wording and this side's diagnostic code are the ones the comparator names. Beside them `xacro_property.the minimized binding documents render as the reference renders them`, `xacro_property.a read of an argument other than through the argument command refuses and names it`, `xacro_arg_domain.an argument read by its bare name refuses and names the argument command`, `xacro_arg_domain.a conditional testing an argument by its bare name refuses and names the argument command`, `xacro_arg_domain.a bare name computing on an argument refuses as the plain read does`, `xacro_arg_domain.a dotted read whose head is an argument refuses and names the argument command`, `load_arg_domain.a property default binds over an argument of the same name`, `load_arg_domain.a property default binds over a caller argument of the same name` and `load_arg_domain.a property defaulting to its argument reads the caller's value`. The python evaluator's refusal is carried by `eval_python.an argument read by its bare name refuses and names the argument command`, and on an interpreter that attaches no name to the error by `eval_python_argument.the argument hint is read from the error itself, whether or not the interpreter names the unbound name`. The one-word sentence is carried by the `argument_command_second_word`, `argument_command_second_word_bound`, `argument_command_expression_name` and `argument_command_expression_words` agreement verdicts, beside `xacro_subst_command.substitution command dispatch resolves find, arg, eval and dirname`. The sentences on the other commands are carried by the seven `command_` agreement verdicts and the `xacro_command_body` cases over the same documents. The `$(eval)` sentences are carried by the eleven `eval_` agreement verdicts beside them, by the `xacro_eval_command` cases over the same documents, by `load_arg_domain.the evaluation command reads a caller's argument`, and under the python evaluator by the `eval_python_argument` cases. Of the three readings that refuse here, the out-of-range number is carried by `xacro_eval_command.an argument the evaluation command reads and cannot carry refuses by name`; the other two by nothing beyond the measurement the entry states. The corpus sentence was measured when the change was made, by flattening each of the sixteen before and after it; the corpus arm of `native_differential` compares each against a fresh upstream render on every run, so a later change to their output surfaces there as a disagreement. |
| An argument declared without a default is accepted | The `argument_without_default` row of the divergence manifest, driven as a whole document through both implementations. |
| A `$(arg n)` above its own declaration resolves | The `argument_above_declaration` row of the divergence manifest, driven as a whole document through both implementations, beside `xacro_arg.a use above its own declaration still resolves the default` and `xacro_arg_domain.a use above its own declaration renders the same spelling the declaration seeds`. |
| A non-text mapping key loads and nothing reads it | The `non_text_mapping_key` row of the divergence manifest, driven as a whole document through both implementations, plus `native_yaml.a key is read in any of the scalar kinds a document can write`. The empty-scalar sentence beside it is carried by the reader's own refusal, measured while that row was minimized. |
| `$(dirname)` renders the directory of the file it is written in | The `dirname_command` row of the divergence manifest, driven as a whole document through both implementations, beside `xacro_include_identity.the directory command names the located directory of the file it is written in`, which pins the value inside a package include and a beside include and at the top, from two working directories. The relative and empty top-level values and the linked-directory value are carried by the measurement the entry states. |
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
| The fresh-render comparison, and the corpus, are Linux gates | The three platform workflows, read against each other: one installs the pinned upstream tooling and registers the corpus tier, and the other two fetch the corpus only for the `clean-consumer` job. |
| The KUKA LBR family is pinned as a description package | Its `PINS` row and its license determination in `cmake/corpus.cmake`. That the umbrella repository's other packages are unpinned is carried by their absence from the listfile. |
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
| Steps take turns only within one runtime directory of one build | By repetition, `cmake_deploy_shared_directory` and `cmake_flatten_shared_directory`, plus their `_multiconfig` variants where Ninja Multi-Config is available. Deterministically, `cmake_deploy_waits_for_a_held_lock` and `cmake_flatten_waits_for_a_held_lock`. Nested runtime directories and a second build tree are carried by nothing. |

### API stability

| Claim | Carried by |
|---|---|
| No deprecation cushion before `v1.0.0` | The absence of any deprecation attribute in the shipped headers, and `version.*`, which holds the declared version to the one the package config exports. |
