# Asset resolution: what an asset URI may name, and what meios will reach

Every asset a description references is a string somebody authored and a file meios has to find: a
`<mesh filename="…">`, a `<texture filename="…">`. This page is the contract for that step. It answers
which spellings are accepted, what a relative one is measured against, what normalization is applied
before anything touches the filesystem, what meios declines to reach, how long a resolved path stays
valid, what happens when the file is simply absent, and what a document written back out carries.

One boundary, one page. What the surrounding document may legally contain is the
[URDF profile](urdf-profile.md); what a `${…}` may evaluate to, and how `load_yaml` finds a
configuration file, is [evaluation](evaluation.md). Acquiring a description package during your build
is an unrelated mechanism for a different audience and shares nothing with anything described here —
that is the [resource guide](resources-guide.md).

Each rule is published with a bare slug, in the table at the end. Those slugs are the executable half
of this page: `tests/golden/urdf/uri.cases` names exactly the same set, one row per rule, and a check
compares the two sets on every build. A rule published here with no case, or a case with no rule
published here, is a red build rather than an omission nobody notices.

## Accepted forms

Four forms are accepted, and a single test decides which one a string is: **a scheme followed by two
slashes**.

meios finds the first `://`. What stands before it is a scheme when it is non-empty, begins with an
alphabetic character, and otherwise contains only letters, digits, `+`, `-` and `.` — RFC 3986 section
3.1, read at its stricter end. `package` selects the package form, `file` the absolute one, and any
other scheme is refused by name under `unsupported_uri_scheme`. A string with no scheme is an absolute
path when the platform says it is one, and a relative path otherwise.

**An empty reference is not one of the forms.** A `<mesh filename="">`, and a `<texture>` element
carrying no `filename` attribute at all, name no asset, and both are refused by name under
`malformed_asset_uri` before anything is classified. The guard is worth stating because the failure it
replaces is silent: an empty reference joined onto the input document's directory normalizes back to
that directory, which is contained and which exists, so unguarded it yields a resolved path naming a
directory rather than a diagnostic.

| Form | Example | How it is resolved |
|---|---|---|
| `package://<package>/<path>` | `package://arm_description/meshes/base.stl` | handed to the source stack, which locates it inside the source carrying that package |
| relative path | `meshes/base.stl` | joined onto the input document's directory |
| absolute path | `/opt/arm/meshes/base.stl` | taken as authored |
| `file://` URI | `file:///opt/arm/meshes/base.stl` | stripped and decoded to a plain absolute path, then treated as one |

**The test is `://` and never a bare colon before the first separator.** `C:/meshes/arm.dae` carries no
double slash after its colon, so it is not read as a one-letter scheme. Refusing on the colon alone
would misread every Windows absolute path as a scheme and break one of the three supported platforms.

**A scheme's first character must be alphabetic**, so a name opening with a digit is a path and not a
scheme, however many slashes follow it.

**The classification is identical on all three platforms. The load verdict for a drive-letter path is
not.** `C:/meshes/arm.dae` is never a scheme anywhere. Whether it is an *absolute* path or a *relative*
one is the platform's own answer: on Windows it is absolute and is held to the containment rule as
authored, while on a POSIX host it is a relative path whose first component happens to contain a colon,
and it is joined onto the input document's directory. Nothing normalizes that divergence away, and no
case table can portably name a Windows path, so it is stated here rather than asserted there.

## Base context

A relative asset path is joined onto **the top-level input document's directory** — the path you handed
`load()`, made absolute before anything is joined against it, so a document named by a bare filename
anchors to the directory it really sits in rather than to an empty one. That directory is also a
containment root, which is what lets a self-contained description work with no configuration at all.

**Named limitation: a relative path does not anchor to the file the reference was written in.** After
xacro and include expansion, one document is commonly assembled out of many files, and a mesh path
written inside an included file is still measured against the top-level document's directory rather
than against the included file's own. Anchoring to the writing file is the semantically righter rule
and is not what meios does: it needs a map from an expanded node back to the document that produced it,
and meios does not build one. Where those two directories differ, a relative asset reference written in
an included file will not resolve. Note that this is a different rule from the one `load_yaml` follows
for a relative resource spec, which does resolve against the document it was written in; the two are
described in different places because they are genuinely different rules.

## Normalization

The `file://` form is normalized before anything else looks at it, and after normalization it is
indistinguishable from a bare absolute path — one normalization rule, one containment decision, one
outcome.

Normalization is three steps, in order: strip the `file://` prefix; strip a single leading separator
when the two characters after it are an alphabetic character and a colon; percent-decode the remainder.

The second step is RFC 8089 appendix E.2, which spells an authority-less DOS drive as `file:///c:/path`
— the separator ahead of the drive letter belongs to the URI grammar and not to the path. meios
recognizes it by the shape of the text and not by a platform macro, so the rule is the same everywhere.
On a POSIX host `/opt/...` is untouched, because `/o` is not a drive letter followed by a colon.

**Three `file://` spellings are therefore recognized here, and the two drive spellings normalize to
one and the same path.** Whether a spelling is then allowed to resolve is a different question, and it
is the one the paragraph beneath the table answers.

| Spelling | Example |
|---|---|
| POSIX, authority-less | `file:///opt/arm/meshes/base.stl` |
| DOS drive, lenient | `file://c:/arm/meshes/base.stl` |
| DOS drive, strict (RFC 8089 appendix E.2) | `file:///c:/arm/meshes/base.stl` |

**The two drive spellings name an absolute path only where a drive letter is absolute.** Both
normalize to `c:/arm/meshes/base.stl`, which is an absolute path on Windows and a relative one on a
POSIX host. A normalized `file://` candidate that is not absolute is refused under
`malformed_asset_uri` rather than joined onto the document's directory, so on a POSIX host the two
drive spellings are refused and on Windows they are held to the containment rule as authored. That is
the same platform split drawn for a bare drive-letter path in [Accepted forms](#accepted-forms), and
it is drawn the same way: nothing normalizes the divergence away, and no case table can portably name
a Windows path.

Percent-decoding is applied to the `file://` form and to nothing else. A bare relative or absolute path
is used exactly as authored, so `%20` in one of those names a directory whose name really contains
those three characters.

**A `file://` URI carrying a host is refused under a code that names the authority.** meios reads the
authority as the text between `file://` and the next separator, and decides it before the path reaches
the containment rule. An empty authority is the local filesystem. So is `localhost` — RFC 8089 section
2 names the two as equivalent — and it is matched without regard to ASCII case, stripped, and resolved
to exactly the path the authority-less spelling names. Any other authority names a host this library
will not reach, and `file://host/share/base.stl` is refused under `unsupported_uri_authority`.

Deciding the authority there rather than letting it fall through is what makes the refusal a statement
about the URI. Read as the first component of a relative path instead, the host would be measured
against the process's own working directory, and the same document would resolve or refuse depending
on where it was loaded from.

**A two-character drive prefix is not an authority.** `file://c:/arm/meshes/base.stl` carries `c:`
where an authority would sit, and that shape is exempted by the same text test that recognizes a drive
elsewhere on this page. Without the exemption the lenient spelling in the table above would be refused
as though `c:` named a host.

**The single-separator form RFC 8089 also permits is not accepted.** `file:/opt/arm/base.stl` carries
no `://`, so by the test above it names no scheme at all: it is read as a relative path whose first
component is the literal directory name `file:`, and it ends as an unresolved asset. Keeping the `://`
test is what stops a drive-letter path being misread as a one-letter scheme, and the single-separator
spelling is vanishingly rare in descriptions, so it is named here rather than accepted.

## Containment

**A path that reaches the filesystem must resolve under a configured package root or under the input
document's directory.** Roots are tried in the order they were configured, and the document's own
directory last. Containment is judged on the *canonicalized* candidate, never on the authored spelling,
so a climb-out spelled through a real subdirectory and a symlink whose target leaves the root are both
caught where a textual prefix test would miss them.

**One rule covers a relative climb-out and an absolute path alike**, which is the point: the form a
path was written in cannot change what it is allowed to reach. `meshes/../../../stray.stl` and
`/somewhere/else/stray.stl` are refused for the same reason and under the same code.

**The escape is configuration, not a policy knob.** A consumer who wants a directory reachable
registers it as a package root, so authority is granted per directory and named, rather than wholesale
by lowering a setting. There is no option that turns containment off.

**No policy setting softens a containment refusal.** `uncontained_asset` is logged at error level
unconditionally and refuses the load at every value of the missing-asset policy. That policy governs
assets that are *absent*; it does not govern assets meios declines to reach. Conflating the two would
let a lowered setting quietly grant reach that was never granted.

**Two roots exist at resolution, not three.** The contract as designed also names a source's own
scratch area as a legitimate place for a resolved path to live, and in a sense it is — a byte-backed
source writes there and checks containment against its own scratch root when it writes. But nothing at
resolution ever tests a path against a scratch root, and nothing can: a scratch path only ever arrives
as the answer to a `package://` lookup through the source stack, and the source stack is consulted for
no other form. That third root therefore has no reachable case here, and this page does not claim an
enforcement nothing can exercise.

**The `package://` form is contained by the source that answers it, not by the rule above.** A package
lookup goes to the source stack, and each source decides what its own root contains — a directory
source refuses a package-plus-relative pair that canonicalizes outside its root, under the same
`uncontained_asset` code. What comes back from a source is not re-checked against the configured roots.
Containment is the source's own answer and is settled there; whether the thing a source named is an
asset at all is a different question, and it is asked where that answer leaves the resolver.

**Containment is enforced at resolution.** That is a statement about one step and not about the whole
pipeline. Whether a component that later consumes a resolved path — the bundle writer copying assets
into a bundle, for one — independently re-checks what it copies is a separate concern this page does
not answer for, and nothing here should be read as promising end-to-end enforcement.

## Ownership

**A resolved path is valid for exactly as long as the source stack you constructed is alive.** That is
the whole lifetime rule. Sources that name files already on disk hand back paths that outlive
everything; a source with no file on disk is what makes the rule necessary.

**A source holding bytes materializes them into a scratch area it owns.** It creates a directory under
the system temporary directory with an unpredictable name, narrows it to its owner, and writes the
asset beneath it *mirroring the requested relative path* — which preserves the original extension by
construction and lets a sibling reference from inside the asset resolve. The scratch tree is removed
when the source is destroyed. Keep the source stack alive for as long as you intend to read the paths
it produced.

**A publication stages in one place and renames from there.** The new bytes are written to a
fixed-name staging file in a single staging directory directly under the scratch root — a sibling of
the mirrored package directories, never a child of any of them — and that file is renamed onto the
entry's path. Both ends stay inside the root, so the rename never crosses a filesystem; the mirrored
directory holds one file per entry throughout, so a sibling reference still resolves by a plain
relative join; and no staging path can be another entry's materialized path. The staging name is a
fixed length rather than the entry's name plus a suffix, so an entry close to a filesystem's
per-component length limit stays publishable. A publication that fails removes the staging file and
nothing else.

**An entry is the file it names, not the text it was spelled with.** A source holding bytes decides
which entry an offer is from its package and relative halves composed and normalized, so two
spellings that name one file are one entry: the second is refused as a duplicate at the default
behavior, and published through the one entry where the source was built to replace. A publication
that would reach a file another entry already holds is refused before anything is written, which
closes the spellings — a differently-cased package on a volume that folds case, a link resolving
onto an entry's directory — that no reading of the text alone can tell apart.

**An entry offered into the staging directory is refused when it is offered**, and the refusal names
the package and the relative. The check reads the leading component of the normalized path the two
halves compose, so a spelling that climbs into the staging directory is refused by the same rule as
the direct one. It happens at the offer rather than at the publication because the collision is
created by the offer, and the file that would prove it does not exist until a later lookup.

**A source that could not obtain a scratch area serves nothing.** It says so once, at error level and
carrying the system's own reason for the failure, and then declines every lookup with the same code
rather than writing somewhere it did not intend to. A load through such a source is refused whatever
the missing-resource policy is set to, because the failure is meios's own environment and not the
description's.

**A lookup whose relative half is empty asks where a package is**, not which file inside it, which is
what `$(find <pkg>)` asks. A source rooted at a directory answers it with the package directory
beneath that root, which already exists and already holds whatever was put there. **A source holding
bytes declines it.** Such a source writes an entry only when that entry is asked for by name, so a
directory it named would be empty at the moment it named it, and every path a description composed
beneath that answer would name nothing. Declining sends the lookup on to a layer that really holds the
package; where no layer does, the `$(find)` fails loudly under `unresolved_find` rather than
substituting a directory nothing was written into. A source holding bytes still refuses an entry
offered under that same empty relative, which names a directory and therefore cannot also name a file.

**The asset-URI layer takes the opposite position, deliberately, and not only for that one spelling.**
A `package://` URI with nothing after the package name is malformed and is refused, and any path a
source answers with which the filesystem does not hold as a regular file is graded by the missing-asset
policy exactly as an absent one is — because a `<mesh>` or a `<texture>` asks for a file and a
directory is not one. The two positions do not contradict each other: a source answers where a package
is, and the asset layer never resolves a directory into an asset, whichever spelling named it.

Four residuals are real and are not smoothed over:

- **A platform that refuses to delete a file with an open handle leaks the scratch tree.** Removal
  stops at the first error rather than corrupting or partially deleting anything, so what is left
  behind is a temporary directory, not a damaged one.
- **A narrow window exists between creating the scratch root and narrowing its permissions.** The
  directory is created first and its permissions tightened immediately after; a process watching the
  temporary directory in that interval sees a directory with default permissions and an unpredictable
  name. It is that interval and nothing longer: a root whose permissions could not be narrowed at all
  is removed and refused rather than served from.
- **The branches inside the creation step are driven; what stays undriven is that step reached through
  a source.** What a source does when the temporary directory itself does not exist — the diagnostic it
  raises, carrying the system's reason, and the refusal that follows at every missing-resource setting —
  is driven end to end, by pointing the temporary-directory environment at a path that does not exist.
  The narrower branch, where the temporary directory resolves and the root creation beneath it fails, is
  driven against the creation step directly rather than through a source. Inside that step both
  branches run: an injectable operations seam scripts the candidate names and the narrowing outcome, so
  the retry that tells a name collision apart from a permanent failure is exercised at both edges of its
  bound — the last attempt below it takes a free name, the attempt at it exhausts — and a root whose
  permissions could not be narrowed is observed to be gone from the filesystem afterwards. Four
  occupancy forms are told apart at a candidate name where the platform has symbolic links — a
  directory, a regular file, a symlink to a directory, and a dangling symlink; where it does not, the
  two symlink forms are compiled out rather than skipped and two occupancy forms are driven. The
  step's cases number ten on Linux, ten on macOS and eight on Windows, all passing, and no case is
  reported as skipped on any of the three.
- **Narrowing the root to its owner replaces permission bits, and not every platform decides access
  that way.** Where the system's real access decision is an access-control list, the call leaves that
  list untouched and narrows nothing; the root's protection there rests instead on the per-user
  temporary directory it was created inside, together with the unpredictable name. The refusal path is
  the same everywhere: a root the step reports as unnarrowable is removed and refused rather than
  served from.

### Where this diverges from the recommended asset lease

A review of this library asked for materialized-asset ownership to be made part of the returned model
or load result, or for resolution to return a stable owned asset object instead of a bare path. meios
diverges from that in letter, and the divergence is a position rather than an oversight.

**The source owns the scratch.** The complaint the recommendation answers is a resolved path naming a
file that has already been deleted — a temporary owned by a value that died at the end of the function
that produced it. Binding the scratch to the source removes that failure rather than managing it: there
is no shorter-lived owner left to outlive, because the object whose lifetime governs every resolved
path is the object the consumer built and already holds. A lease attached to the load result would make
the same paths valid for a *different* span, and a consumer holding a model would then have to reason
about two lifetimes instead of one.

The cost is stated plainly: a consumer that destroys its source stack and keeps the model still holds
paths that may no longer name files. That is a rule you can state in one sentence and check by reading
a scope, which is why it was chosen over an owned-asset wrapper that every consumer would have to
thread through its own types.

## Missing-resource policy

The missing-asset policy governs one situation: a path that meios is *allowed* to reach and that the
filesystem does not hold, plus a `package://` reference no configured source can supply. Nothing else
is routed through it.

| Setting | What happens to an absent asset | Claim consequence |
|---|---|---|
| `fail` (default) | `unresolved_asset` at error level; the load fails | the deployment-completeness claim is withdrawn |
| `warn` | `unresolved_asset` at warning level; the load succeeds with no resolved path recorded | the deployment-completeness claim is withdrawn |
| `skip` | no diagnostic at all; the load succeeds with no resolved path recorded | the deployment-completeness claim is withdrawn |

The claim answers for the document rather than for the log, so silencing the diagnostic does not
restore the asset: all three settings withdraw the same claim, and `skip` records what it would have
withdrawn rather than leaving the claim standing. Branch on the claim, not on the presence of a
warning.

**Absent is not the same as unreachable.** A path no root contains is refused under `uncontained_asset`
at every setting, as described above. A path a root contains but the filesystem does not hold is
absent, and only that one is the policy's business.

**A path that is not a regular file is absent.** A candidate a root contains which the filesystem
holds as a directory, a device or a broken link is graded by this policy exactly as a wholly absent one
is: the reference is well formed and the root is permitted, and what is wrong is only that there is no
asset there. The three are deliberately not told apart. Requiring a regular file rather than mere
existence is what stops a directory from becoming a resolved path that a later file copy would open
and read as empty geometry. **The `package://` form is held to this too**: a path a source answers with
is put to the same test before it leaves the resolver, so the same directory is graded absent whether
it was named relatively or through a package.

**Absent is not the same as meios failing.** When a byte-backed source cannot create its scratch
directory or cannot write an asset into it, that is meios's own environment failing — a full disk, a
temporary directory it may not write. It is reported under `asset_write_failed` at error level and
refuses the load at every setting, and it is deliberately never routed through the missing-asset
policy. Doing so would misattribute the fault to the description's author, and would let a lowered
setting turn a full disk into a merely unresolved asset.

**A refusal that is the offering program's own doing carries its own code.** `asset_write_failed` is
for a filesystem failure. An entry offered a second time to a source built to reject the second offer
says something about the program's configuration and nothing about the filesystem, so it is reported
under `duplicate_asset_entry` instead, and the first bytes keep resolving. The split matters to
anyone counting: a consumer tallying write failures is not handed a policy refusal in the same total.

Several refusals are unconditional and belong to no policy. An empty reference, a `file://` authority
naming a host, and a `file://` candidate that does not normalize to an absolute path are three, each
described in its own section above. Two more are worth naming here, beside the policy they are not
part of. **A `package://` URI must carry both
halves at this layer** — a package name and a relative path beneath it — so the spelling that names a
package and nothing after it, the spelling whose package name is empty, and the spelling that ends at
the separator immediately after the package name are all malformed under `malformed_asset_uri`. That
guard measures that one separator and no deeper one, and what a reference ending at a separator
further down does depends on what precedes that separator. Where a directory below the package name
is named, the reference carries a relative half, the lookup happens, and a directory it resolves to is
graded absent by the policy above under `unresolved_asset` rather than being called malformed. Where
that segment is empty — `package://<pkg>//` — the relative half is a bare separator. For a source whose
canonical root is below its filesystem root, joining that separator onto the package directory
discards the directory instead of extending it and leaves a filesystem-root candidate outside the
source root; what answers is the containment refusal described in [Containment](#containment),
unchanged. The missing-asset policy still handles the unresolved lookup as usual, but the unsoftened
`uncontained_asset` remains at error level under every setting, so the load through such a source fails
even at the silent one. This behavior is a known residual rather than a promise, which is why no rule
below covers it and a case pins it instead. A `<mesh>` and a `<texture>` ask for a file, and a package
directory is not one. At the source layer, the genuinely empty relative used by the bare
`$(find <pkg>)` substitution command is answered with the package directory by a directory-rooted
source holding the package, while a source holding bytes declines it; that split is deliberate, and
its halves are stated in [Ownership](#ownership) so none of them is met without the others. And a
resolved asset whose contents are an unsmudged Git-LFS pointer rather than geometry is refused under
`lfs_pointer_asset`, which reaches meshes and textures alike.

## Serialization

**A document meios writes always carries the authored URI, never a resolved path.** Resolution is a
load-time enrichment. It records the resolved path on the mesh and on the material beside the authored
string, and it never round-trips into a written document: both serializers write the `filename`
attribute the description authored.

Two things follow. A flattened document's asset references stay byte-identical to its input's, so
flattening never silently rewrites what a downstream tool will look for. And it is structurally
impossible for a scratch path — a temporary directory name, valid on one machine for one run — to reach
a document somebody commits.

The bundle writer rewriting references to bundle-relative form is that writer's own concern and is
described where the bundle is described. It is a deliberate rewrite of an authored URI into another
authored URI, not a resolved path leaking into a document.

## Rules

| Element | Situation | Verdict | Diagnostic code | Rule id |
|---|---|---|---|---|
| `<mesh>` | `package://<pkg>/<path>` naming a file a configured source holds | accept | — | `rule:package-uri-resolves` |
| `<mesh>` | `package://<pkg>` with nothing after the package name | refuse | `malformed_asset_uri` | `rule:package-uri-names-a-path` |
| `<mesh>` | `package:///<path>` whose package name is empty | refuse | `malformed_asset_uri` | `rule:package-uri-names-a-package` |
| `<mesh>` | `package://<pkg>/` ending at the separator | refuse | `malformed_asset_uri` | `rule:package-uri-carries-a-relative-half` |
| `<mesh>` | `package://` naming a package no configured source holds | graded by the missing-asset policy | `unresolved_asset` | `rule:package-uri-backed-by-a-source` |
| `<mesh>` | a `filename` attribute present and empty | refuse | `malformed_asset_uri` | `rule:empty-reference-refused` |
| `<mesh>` | a relative path resolving under the input document's directory | accept | — | `rule:relative-path-against-document-base` |
| `<mesh>` | a path a root contains which is not a regular file | graded by the missing-asset policy | `unresolved_asset` | `rule:reference-is-a-regular-file` |
| `<mesh>` | `package://` naming something a source holds which is not a regular file | graded by the missing-asset policy | `unresolved_asset` | `rule:package-reference-is-a-regular-file` |
| `<mesh>` | `package://` ending at a separator after a named directory below the package, which the lookup resolves | graded by the missing-asset policy | `unresolved_asset` | `rule:package-reference-below-the-name` |
| `<mesh>` | a relative path climbing out of every root | refuse | `uncontained_asset` | `rule:relative-path-contained` |
| `<mesh>` | an absolute path inside a registered package root | accept | — | `rule:absolute-path-inside-a-root` |
| `<mesh>` | an absolute path inside no root | refuse | `uncontained_asset` | `rule:absolute-path-contained` |
| `<mesh>` | a `file://` URI naming a file inside a root | accept | — | `rule:file-uri-normalized` |
| `<mesh>` | a `file://` URI whose path carries a percent-encoded byte | accept | — | `rule:file-uri-percent-decoded` |
| `<mesh>` | a `file://` URI carrying a host name | refuse | `unsupported_uri_authority` | `rule:file-uri-authority-refused` |
| `<mesh>` | a `file://` URI naming the local host explicitly | accept | — | `rule:file-uri-local-host-accepted` |
| `<mesh>` | an `http://` URI | refuse | `unsupported_uri_scheme` | `rule:foreign-scheme-refused` |
| `<mesh>` | a `model://` URI — the SDF spelling of a package reference | refuse | `unsupported_uri_scheme` | `rule:sdf-scheme-refused` |
| `<texture>` | `package://<pkg>/<path>` naming a file a configured source holds | accept | — | `rule:texture-uri-resolves` |
| `<texture>` | a relative path climbing out of every root | refuse | `uncontained_asset` | `rule:texture-path-contained` |
| `<texture>` | an `http://` URI | refuse | `unsupported_uri_scheme` | `rule:texture-foreign-scheme-refused` |
| `<texture>` | a `<texture>` element carrying no `filename` attribute | refuse | `malformed_asset_uri` | `rule:texture-empty-reference-refused` |

**A texture takes the mesh's vocabulary exactly.** The same classifier, the same base, the same
normalization, the same containment rule, the same policy and the same claim consequence. Containment
is a property of the path, so the element carrying it cannot change the answer; the four texture rows
above exist to hold that claim rather than to state a different one.

**There is no dropping situation in this contract, and the set is empty rather than unpopulated.** The
verdict vocabulary the case table uses has three values — refuse, drop, accept — and only two of them
occur here. A foreign scheme is a loud typed refusal; a containment escape is a structural refusal no
setting softens; a malformed `package://` URI is a refusal; an unbacked package is graded by the
missing-asset policy, which is not a drop either. A drop — the load succeeds, a warning discloses the
code, the parse claim still stands — has no member in that set, and the case that would drive one
reports itself skipped with that reason rather than passing on zero rows.

## What the description corpus does and does not prove

The pinned description corpus gates every change here, and it is worth being exact about what that
gate covers. Classifying every asset reference in it gives **256 `package://` references and 25
`$(find …)` substitutions, and not one relative, absolute or `file://` reference.** Real descriptions
in the wild use the package form and essentially nothing else.

So the corpus confirms that the rules on this page cause no regression on the package form, which is
the form that matters most in practice. It does **not** exercise containment, the relative base, the
`file://` normalization, or any refusal on this page. Those are held by the case table and by nothing
else, which is why the drift check between this page and that table is worth having.
