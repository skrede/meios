# Description corpus survey: which public robots would teach this project something

This page is the record of a survey of public, third-party robot descriptions, and it is the contract
for one question only: for each construct this project's evaluator can exercise but no pinned
description does, is there a maintained, publicly available robot description that exercises it — and
if there is not, what was looked at before saying so.

It is not a catalogue of robots, not a ranking, and not a statement about what meios supports. What a
description's expressions may run belongs to [evaluation](evaluation.md), what the reader does with the
expanded document belongs to the [URDF profile](urdf-profile.md), and what is not yet proven belongs to
[known limitations](known-limitations.md). Nothing on this page restates any of those. A candidate
appearing here was not pinned when the survey ran and is not claimed to load on the strength of
appearing here. One of them — the Kinova arm — has since been pinned, after being driven through this
project's own loader and compared against the reference; its row says so, and it is the only one.

A second question was answered later and is recorded in a section of its own near the end: for each
candidate, what happens when this project's own loader is pointed at it. That is a different
measurement from the one the tables below carry, and the two are never mixed.

The survey's breadth is bounded by construct coverage rather than by a candidate count: it continues
until every uncovered construct has either a candidate observed to carry it or a recorded finding that
no maintained carrier was found. That is why the number of rows below is not round, and why two of the
entries are findings rather than robots.

## Authority

Every fact in the candidate tables comes from the upstream's own repository at the revision named in
its row, read directly rather than through a package index, a distribution or a mirror. Revisions are
recorded as commit identifiers rather than as tags or versions, because a tag can be moved and a commit
cannot. Each candidate row below carries an abbreviated identifier beside its license; the table in this
section carries the full one for every upstream the survey touched.

Where a row says a construct was *evaluated*, the description was rendered by the same upstream tooling
this project measures against — xacro 2.1.1 on PyYAML 6.0.3 — with its expression evaluator
instrumented to report the expressions it actually evaluated, and the construct appears in that report.
Where a row says a construct was *written*, the spelling was read in the source at the named revision
and no render reached it; each such row says what stopped the render.

| Upstream | Revision consulted | Declared license |
|---|---|---|
| <https://github.com/enactic/openarm_description> | `1fba2cbc05001f05b4514120b70130b4ac06f409` | Apache-2.0 |
| <https://github.com/Kinovarobotics/ros2_kortex> | `c50057a02fb64e854b2759261994f43173bec703` | BSD-3-Clause |
| <https://github.com/clearpathrobotics/clearpath_common> | `811baf06a32747be0653ea18db2c8868860d193f` | BSD-3-Clause |
| <https://github.com/husky/husky> | `41e15d283a8d955938204e79554a875264417bb9` | BSD-3-Clause |
| <https://github.com/shadow-robot/sr_common> | `172c3d9c3715ee75eb10dfe4ee57e26a342b4529` | BSD-3-Clause |
| <https://github.com/flexivrobotics/flexiv_description> | `f33331a7f75b9a25ec903674595ddfebbe1fb998` | Apache-2.0 |
| <https://github.com/DoosanRobotics/doosan-robot2> | `ec9242546ec6202835900dbcd8498e2daabfa6a6` | BSD-3-Clause |
| <https://github.com/TechmanRobotInc/tmr_ros2> | `c40bfc0d296337311f311c837861fd6318b00407` | BSD-3-Clause (package manifests) |
| <https://github.com/pal-robotics/tiago_robot> | `f1c33c92bdde7c1dd79f0c3e739e98a233dbd30b` | Apache-2.0 |
| <https://github.com/Interbotix/interbotix_ros_manipulators> | `0bb2b0e6d0e619bff02cf74dbd5af5681dcf80c9` | BSD-3-Clause |
| <https://github.com/ROBOTIS-GIT/open_manipulator> | `9187eca0920458be04d2399906388f55242f81f1` | Apache-2.0 |
| <https://github.com/rai-opensource/spot_ros2> | `dbbac7a0afac1766ad43151f8a4b55651066bff1` | MIT |
| <https://github.com/hello-robot/stretch_ros2> | `73decc6adc45986744e36df2bac19fda7eb6aec8` | mixed: Apache-2.0, GPLv3, CC BY-NC-SA 4.0 |
| <https://github.com/unitreerobotics/unitree_ros> | `daadf41ee9afce8f90fdc09a98506012691fa122` | BSD-3-Clause |
| <https://github.com/agilexrobotics/ugv_gazebo_sim> | `27633a956c845903ee630538afeb17fe70afdd84` | mixed: BSD-3-Clause, BSD, one manifest declaring `TODO` |
| <https://github.com/neobotix/neo_simulation2> | `832041452c1a0199afea1e9b65adf37381e96214` | MIT |
| <https://github.com/ANYbotics/anymal_b_simple_description> | `988b5df22b84761bdf08111b1c2ccc883793f456` | BSD-3-Clause |
| <https://github.com/ros-industrial/motoman> | `846fbbc57c60223f12bf4795ea6404982d57709e` | mixed: Apache-2.0, BSD-3-Clause |
| <https://github.com/ros-industrial/universal_robot> | `39ad110d8f2e8f66856a201cca88aa7a7025e3eb` | mixed: Apache-2.0, BSD |
| <https://github.com/UniversalRobots/Universal_Robots_ROS2_Driver> | `f6cae596ae0ba7a5045a89b1e847c47155d7e203` | BSD-3-Clause |
| <https://github.com/PickNikRobotics/moveit_resources> | `0faf2b7c67dc218129c18ff15f13367dab1378a9` | BSD (package manifests) |
| <https://github.com/ubi-agni/agni_robots> | `c51fa23d0d171e32364e7e1a23ab2cdfdad12829` | mixed: BSD, GPLv3 |
| <https://github.com/TAMS-Group/tams_ur5_setup> | `4dacfaf15b6142dec2696791fd5b75024feca301` | BSD-3-Clause |
| <https://github.com/RethinkRobotics/baxter_common> | `6c4b0f375fe4e356a3b12df26ef7c0d5e58df86e` | BSD-3-Clause |
| <https://github.com/agimus-project/agimus-franka-description> | not consulted at a revision — see its row | — |

Two upstreams already pinned by this project's own corpus appear in the findings below, cited by the
revisions the corpus already records rather than by new ones:
`UniversalRobots/Universal_Robots_ROS2_Description` at tag `4.3.1`, archive digest
`3532a25c9942…`, BSD-3-Clause; and `frankarobotics/franka_description` at tag `2.8.1`, archive
digest `4adcc45f83fd…`, Apache-2.0.

A license here is the upstream's own declaration — the repository's `LICENSE` file, or the `<license>`
element of the package manifests where there is no root file. Where the manifests within one repository
disagree, the row says so rather than picking one. Nothing here is a legal determination and nothing
here has been checked for a mismatch between a declaration and the file contents; a candidate that is
ever acquired gets its own determination at that point.

**Two candidates state one license in a package manifest and a different one in the repository's own
license file.** Both are recorded here with both spellings and neither is resolved, because resolving
one in this project's favour would be this project making a claim it cannot support:

| Upstream | Package manifest declares | Root license file is | Note |
|---|---|---|---|
| `shadow-robot/sr_common` | `sr_description/package.xml`: **`GPL`** | verbatim BSD 3-Clause; the description files' own headers carry a BSD notice | not a spelling difference — two different licenses. It is not acquired, so no determination was made. |
| `DoosanRobotics/doosan-robot2` | `dsr_description2/package.xml`: **`Apache License 2.0`** | BSD 3-Clause | the same shape. It is not acquired either. |

Neither is comparable to a manifest writing `Apache 2.0` where the root file is `Apache-2.0`, which is
one license spelled two ways and which this project's own acquisition gate records as such.

## The gap list

The evaluator names fourteen constructs it can observe a load exercising. **Thirteen of the fourteen
are now recorded against at least one pinned entry point, and one is not.** That is the current state
and it is not the state this survey was written in: when the tables below were compiled, six were
recorded and eight were being looked for. Eight rows of this page are therefore a record of a search,
and what the search produced is stated here rather than left to be inferred from an out-of-date count.

What closed the other seven:

| Construct | How it closed |
|---|---|
| `sequence-subscript`, `string-membership`, `named-split` | already inside the pinned Franka description, and reached once a block argument's contents were evaluated where the caller wrote them rather than where a macro inserts them. No acquisition. Five pinned rows carry them. |
| `mapping-literal` | pinned twice: the Kinova arm, and a second top-level document of the already-pinned Universal Robots package. Both reach it through a macro-parameter default the caller overrides, which is only evaluated at all because a default is now evaluated whether or not the caller supplied the parameter. |
| `merge-key`, `document-sequence`, `negative-index` | witnessed together by a gantry description written in this repository, exactly as the finding below recommends: its joint-limit table factors a repeated block behind an anchor, and a table like that writes each axis's range and vector as a short sequence and reads the ends of a range by index. |
| `non-string-key` | **still unwitnessed, and deliberately so.** The key is admitted and takes its place in the mapping, but every subscript spelling here is text, so a load exercising it would exercise a dead end rather than a construct. Its evidence is a unit case and a divergence-manifest row instead of a pinned row. |

The eight the survey set out to find carriers for were:

| Construct | What a description does to reach it |
|---|---|
| `document-sequence` | an auxiliary document holds a list, and the description reads it |
| `sequence-subscript` | an expression indexes a sequence |
| `negative-index` | that index counts back from the end |
| `mapping-literal` | an expression builds a mapping with `dict(name=value, …)` |
| `named-split` | an expression splits a string on an explicit separator |
| `string-membership` | an expression tests one string for containment in another |
| `merge-key` | an auxiliary document flattens one mapping into another with `<<:` |
| `non-string-key` | an auxiliary document keys a mapping by something other than text |

The six already covered when the survey ran — an alias, a duplicate key, a unit tag, a dotted member
read, string concatenation and a string's truth value — needed nothing from it and are not looked for
below.

## What the survey found

Six of the eight have a carrier. Two do not, and that is the more useful half of the answer. Four of
the six are answered by an upstream this project already pins, which is the other useful half.

| Construct | Answer | Where it is answered |
|---|---|---|
| `document-sequence` | carried, evaluated in one | `enactic/openarm_description` |
| `sequence-subscript` | carried, evaluated in two, written in one more | `enactic/openarm_description`; `frankarobotics/franka_description`, already pinned; written in `husky/husky` |
| `negative-index` | carried, evaluated in one | `enactic/openarm_description` |
| `mapping-literal` | carried, evaluated in three, written in one more | `enactic/openarm_description`; `Kinovarobotics/ros2_kortex`; a second top-level document of the already-pinned `ur_description`; written in `clearpathrobotics/clearpath_common` |
| `named-split` | carried, evaluated in one, written in one more | `frankarobotics/franka_description`, already pinned; written in `husky/husky` |
| `string-membership` | carried, evaluated in two | `shadow-robot/sr_common`; `frankarobotics/franka_description`, already pinned |
| `merge-key` | **no carrier found in a document a description loads** | two carriers found, both in motion-planning parameter files rather than in a loaded configuration |
| `non-string-key` | **no carrier found at all** | every auxiliary document read in this survey keys its mappings by text |

## Candidates carrying a construct on the gap list

| Upstream | Revision · license | Contributes | Where, at that revision | Observation |
|---|---|---|---|---|
| `enactic/openarm_description` | `1fba2cbc0500` · Apache-2.0 | `document-sequence`, `sequence-subscript`, `negative-index`, `mapping-literal` | `assets/robot/openarm_v2.0/urdf/utils/openarm_v20_robot.xacro` reads `struct/topology.yaml`, whose `links:` is a list, and takes `['links'][0]` and `['links'][-1]` off it; the same file and its siblings build six-key origin mappings with `dict(x=…, y=…, z=…, roll=…, pitch=…, yaw=…)` | **evaluated** — `assets/robot/openarm_v2.0/urdf/openarm_v20.urdf.xacro` renders to completion and the instrumented run reports `dict(x=1, y=1, z=1)`, `component_topology['links'][0]` and `component_topology['links'][-1]` |
| `Kinovarobotics/ros2_kortex` | `c50057a02fb6` · BSD-3-Clause | `mapping-literal` | `kortex_description/robots/kortex_robot.xacro` declares a seven-joint initial-position mapping as a macro-parameter default | **evaluated** — `kortex_description/robots/gen3.xacro dof:=7` renders to completion and the instrumented run reports `dict(joint_1=0.0, …, joint_7=0.0)`, even though the caller then overrides that parameter with a loaded document |
| `shadow-robot/sr_common` | `172c3d9c3715` · BSD-3-Clause | `string-membership` | `sr_description/hand/xacro/hand_e.urdf.xacro` and its siblings branch on `${'ff' in fingers}`, where `fingers` is documented in the file as a list-like string; `process_sensor_parameters.xacro` tests `'=' in value` the same way | **evaluated** — `sr_description/robots/sr_hand.urdf.xacro` renders to completion and the instrumented run reports `'=' in value`. **Caveat: last pushed 2025-01-09.** It is not archived and it is not deprecated by its own record, but it is the least recently touched of the carriers here, and it is recorded with that attached rather than presented as current |
| `clearpathrobotics/clearpath_common` | `811baf06a327` · BSD-3-Clause | `mapping-literal` | `clearpath_manipulators_description/urdf/lift/ewellix.urdf.xacro` builds `dict(lower=0.0, upper=0.0)`; `clearpath_platform_description/urdf/common.urdf.xacro` builds a twenty-six-argument letter table the same way and then subscripts it | **written, not evaluated.** Both sites are eagerly-evaluated properties inside macros, so any caller reaches them — but this vendor's top-level documents are produced by a generator from a robot specification rather than shipped as files, and no shipped document in the repository is a top-level description. Reaching these needs a generated document this survey did not produce |
| `husky/husky` | `41e15d283a8d` · BSD-3-Clause | `named-split`, `sequence-subscript` | `husky_description/urdf/pacs.urdf.xacro` composes bracket offsets out of `${bracket_xyz.split(' ')[0]}` and its two siblings | **written, not evaluated.** The macro is reached only when the mounting system is switched on through the environment, and the top-level `husky.urdf.xacro` render stopped earlier on accessory packages that live outside this repository and were not fetched. The subscript here indexes a split's result, which is a sequence. Note that the same file's neighboring mount checks write `x not in python.range(…)`, which this evaluator refuses, so this description would need that measured before it could be pinned |

## Findings inside upstreams this project already pins

Four gap-list constructs turn out to be reachable without acquiring anything, which is a more useful
answer than another robot would have been.

**Three of the eight are written in the Franka description this project already pins, and its own
pinned entry points evaluate them upstream.** `robots/common/utils.xacro` and
`end_effectors/common/utils.xacro` define a capsule macro that computes `xyz.split(' ')[0]` and
branches on `'y' in direction` — two spellings, three of the eight constructs. Rendering
`robots/fr3/fr3.urdf.xacro` through the pinned upstream with default arguments evaluates both. This
project's own record of that same document reports neither, and the reason is a real difference in
evaluation order rather than a gap in the record: the macro call sits inside a block argument that the
receiving macro discards unless self-collision geometry is asked for, and the reference expands a block
argument's contents at the call site while this project expands them only if the block is inserted. So
the constructs are present, are reached upstream, and are not reached here. That difference is
described in [known limitations](known-limitations.md); what matters for this survey is that no
acquisition at all stands between this project and `named-split`, `sequence-subscript` and
`string-membership`.

**The keyword-argument mapping constructor is in the Universal Robots description this project already
pins, in a second top-level document it ships and this project does not pin.**
`urdf/ros2_control_mock_hardware.xacro` and the `urdf/inc/ur_joint_control.xacro` it includes both
declare a six-joint initial-position mapping as a macro-parameter default. The entry point that
includes them is `urdf/ur_mocked.urdf.xacro`, which is not the `urdf/ur.urdf.xacro` this project pins.
Both halves of that are measured rather than inferred: rendering `urdf/ur.urdf.xacro` in the variant
this project measures reports no expression at all from the gap list, and rendering
`urdf/ur_mocked.urdf.xacro` at the same revision and with the same variant renders to completion and
reports `dict(shoulder_pan_joint=0.0, …)`. So a second answer to `mapping-literal` also needs no
acquisition — only a document already inside a pinned upstream. The same construct in the same shape is
in the driver repository beside it, `ur_robot_driver/urdf/ur.ros2_control.xacro`, which is a hardware
interface rather than a robot description.

## The two constructs with no carrier

**`merge-key`: found twice, never in a document a description loads.** Every auxiliary document in this
survey was parsed with the reference reader — 1 405 files across twenty-eight upstreams, counting the
four this project already pins. Exactly two repositories write a merge key, and in both it is in a
motion-planning parameter file that a parameter server reads rather than a file any description loads:

| Upstream | Revision · license | File | Why it does not answer the question |
|---|---|---|---|
| `TAMS-Group/tams_ur5_setup` | `4dacfaf15b61` · BSD-3-Clause | `tams_ur5_setup_moveit_config/config/joint_limits.yaml` | anchors and merge keys throughout, but the repository's own description loads a different file, from a package outside this repository |
| `ubi-agni/agni_robots` | `c51fa23d0d17` · mixed BSD / GPLv3 | six files under `agni_moveit_config/*/config/`, including planner and joint-limit configurations | same shape; the descriptions in the same repository load `agni_description/robots/robots.yaml` and a tactile-sensor file, neither of which carries a merge key or an anchor |

So the finding is not that the merge key is rare in this ecosystem — it is ordinary in
motion-planning configuration — but that the files carrying it are not the files a description reads.
A candidate that would answer this question is a description whose own loaded configuration factors a
repeated block out behind an anchor. None was found. One near miss is worth recording so it is not
re-found: `enactic/openarm_description` has an anchor and an alias in
`assets/end_effector/pinch_gripper/config/old/inertials.yaml`, and both lines are commented out.

**What was done about it.** The recommendation implicit in that finding — that a description of this
shape has to be written rather than found — was taken. A two-file cartesian gantry was authored in
this repository: every axis runs on the same drive, so the drive's rating is stated once behind an
anchor and merged into each axis entry, and one axis departs from it by overriding a single key. It is
pinned as a corpus entry point by a digest of its two files. It is the one entry point in the pinned
set that is not a vendor's own document, and it is marked as such wherever it appears, because a
description this project wrote is weaker evidence about how anyone authors than a description somebody
ships.

**`non-string-key`: not found at all.** Of the same 1 405 auxiliary documents, parsed rather than
pattern-matched, **not one** keys a mapping by anything other than text. Every key in every document
read here is a string. This is the strongest negative in the survey and it has a plausible cause: a
robot description's configuration is keyed by joint and link names, which are strings by construction,
and the shapes that produce a numeric or boolean key in YAML — an index table, or a key spelled `on`,
`off`, `yes` or `no` — do not arise in that vocabulary. Evidence for this construct will have to be
authored rather than found, and this survey's recommendation is to stop looking for it.

**And authoring one was declined, on a measurement rather than on effort.** Such a key is admitted by
the reader and takes its place in the mapping, but every subscript spelling this evaluator has is
text, so no expression reaches the entry: a description written to exercise the construct would
exercise a dead end. It is recorded as supported and unwitnessed instead, and what carries it is a
unit case and a row in the reviewed divergence manifest.

## Candidates considered and not selected

Each of these was read at the revision in the authority table. None of them carries anything on the gap
list; the column says what was checked.

| Upstream | Revision · license | Why not selected |
|---|---|---|
| `flexivrobotics/flexiv_description` | `f33331a7f75b` · Apache-2.0 | maintained and it does read auxiliary documents, but every `in` test in it is against a mapping and every subscript is by name — nothing on the gap list. Its sixty-five auxiliary documents hold no merge key, no anchor and no non-text key |
| `DoosanRobotics/doosan-robot2` | `ec9242546ec6` · BSD-3-Clause | 111 auxiliary documents parsed, none carrying a gap-list construct; no split, no sequence index, no mapping constructor in any of its descriptions |
| `TechmanRobotInc/tmr_ros2` | `c40bfc0d2963` · BSD-3-Clause | 124 auxiliary documents parsed, same result |
| `pal-robotics/tiago_robot` | `f1c33c92bdde` · Apache-2.0 | maintained, but its descriptions load no auxiliary document at all and its expressions stay within arithmetic and comparison |
| `Interbotix/interbotix_ros_manipulators` | `0bb2b0e6d0e6` · BSD-3-Clause | 127 auxiliary documents parsed, none loaded by a description; no gap-list construct in any of its descriptions |
| `ROBOTIS-GIT/open_manipulator` | `9187eca09204` · Apache-2.0 | no gap-list construct; its descriptions carry no auxiliary load |
| `rai-opensource/spot_ros2` | `dbbac7a0afac` · MIT | no gap-list construct; the description package here is thin and its expressions are arithmetic |
| `unitreerobotics/unitree_ros` | `daadf41ee9af` · BSD-3-Clause | no gap-list construct; hand-written descriptions with very little expression content |
| `neobotix/neo_simulation2` | `832041452c1a` · MIT | no gap-list construct |
| `ros-industrial/motoman` | `846fbbc57c60` · mixed Apache-2.0 / BSD-3-Clause | its descriptions do load auxiliary documents, but only by name and only for scalars; nothing on the gap list. License is mixed across manifests |
| `ros-industrial/universal_robot` | `39ad110d8f2e` · mixed Apache-2.0 / BSD | the first-generation description of a vendor this project already pins at its current generation; no gap-list construct, and pinning both would buy authoring-style breadth this survey is not looking for |
| `UniversalRobots/Universal_Robots_ROS2_Driver` | `f6cae596ae0b` · BSD-3-Clause | carries the mapping constructor, but it is a driver rather than a robot description: the file holding it describes a hardware interface, and the description it belongs to is the package this project already pins |
| `hello-robot/stretch_ros2` | `73decc6adc45` · mixed Apache-2.0 / GPLv3 / CC BY-NC-SA 4.0 | no gap-list construct — and the licensing would need resolving before anything here could be acquired, because one package in it is CC BY-NC-SA 4.0, a non-commercial term that does not sit beside the rest |
| `agilexrobotics/ugv_gazebo_sim` | `27633a956c84` · mixed BSD-3-Clause / BSD / one `TODO` | no gap-list construct, and one package manifest declares its license as `TODO`, which is not a determination anything can be acquired against |
| `ANYbotics/anymal_b_simple_description` | `988b5df22b84` · BSD-3-Clause | no gap-list construct and no auxiliary document of any kind. **Unmaintained by its own record: last pushed 2020-11-11.** Recorded rather than omitted, because it is a legged robot and every description this project pins is a serial arm or a mobile base — so it remains a candidate for structural breadth even though it contributes nothing semantically |
| `PickNikRobotics/moveit_resources` | `0faf2b7c67dc` · BSD (manifests) | no gap-list construct. **Unmaintained by its own record: last pushed 2019-05-24.** It is also a test-fixture collection rather than a vendor's own description, which makes it weak evidence about how anyone authors |
| `ubi-agni/agni_robots` | `c51fa23d0d17` · mixed BSD / GPLv3 | carries a merge key, but not in a file its descriptions load — see above. Its descriptions do reach further than most, using dotted access into loaded mappings, but that construct is already covered. **Last pushed 2025-01-24**, and its manifests mix BSD with GPLv3 |
| `TAMS-Group/tams_ur5_setup` | `4dacfaf15b61` · BSD-3-Clause | carries a merge key, but not in a file its descriptions load — see above. **Last pushed 2025-01-08**, and it is a laboratory's own integration rather than a vendor's or a community's description |
| `agimus-project/agimus-franka-description` | `not consulted` · not determined | a fork of an upstream this project already pins. Whatever it carries, it carries as derived work, so recording it as an independent carrier would double-count one authoring style. Not consulted at a revision for that reason |
| `RethinkRobotics/baxter_common` | `6c4b0f375fe4` · BSD-3-Clause | see below |

**`RethinkRobotics/baxter_common`, at `6c4b0f375fe4e356a3b12df26ef7c0d5e58df86e` (tag `v1.2.0`),
BSD-3-Clause.** Counted at that revision: 22 xacro files, 1 plain URDF, and **zero** auxiliary
documents of any format. It therefore supplies evidence for nothing on the gap list, and it fails the
maintained wording on its own terms — **last pushed 2018-06-26.** It is kept in this record as a
candidate for *structural* rather than semantic breadth: it is a dual-arm robot with left and right
end-effector variants, a topology unlike anything pinned. The unmaintained caveat travels with it
wherever it goes, and it should never be presented as a current description.

## What this project's own loader measured

Everything above is a measurement of the **reference** tooling. The closing section below has always
said so in as many words, and the distinction turned out to matter more than the wording suggested: a
document the reference renders is not a document this project loads. So every candidate named on this
page was subsequently driven through this project's own loader, and the verdicts are recorded here so
that a later reader does not repeat the work.

**How, and where.** Each upstream was fetched at the revision its row names, extracted, and its entry
point driven through this project's command-line tool with the extracted tree as the package root. The
control throughout was the already-pinned `franka_description/robots/fr3/fr3.urdf.xacro`, driven
through the identical harness and flattened successfully, so a failure below is the document's and not
the harness's. Every verdict in this section was taken **once, locally, on Linux, with one toolchain,
in a release build** — none of them is a statement about continuous testing, and none of them should be
read as one. The pinned entry points this project actually ships are a different set and carry their
own evidence; what is recorded here is a probe of candidates, not a gate.

### The three candidates the survey selected

All three were selected because the reference evaluated a gap-list construct while rendering them. All
three refuse here, each on a different thing:

| Candidate | Entry point driven | Verdict | Refuses on |
|---|---|---|---|
| `shadow-robot/sr_common` | `sr_description/robots/sr_hand.urdf.xacro` | **refuses** | a namespaced set constructor, `${python.set('hand_e hand_g hand_c'.split())}`. Behind it: a set value kind the model does not have, the no-separator split this grammar deliberately refuses, a bracket list literal, string methods and string subscripting, and the reference's own message helper. Its bimanual sibling fails at the same site and there is no third entry point. |
| `enactic/openarm_description` | `assets/robot/openarm_v2.0/urdf/openarm_v20.urdf.xacro` | **refuses** | a bracket list comprehension. Comprehensions are named outright as future work rather than as a gap. Its v1.0 entry point refuses independently on a tuple literal, and its gripper entry point on a numeric constructor; the repository also writes a lazy-evaluation attribute on 154 properties, which this evaluator has no concept of. |
| `Kinovarobotics/ros2_kortex` | `kortex_description/robots/gen3.xacro dof:=7` | **refused when probed; loads now** | it refused on a property declaring a fallback attribute, and then on the order a block argument is expanded in. Both are behaviors this project has since brought into line with the reference, and the document is now a pinned corpus entry point that renders element for element with the reference. |

That the first two refuse is not a correction to this survey's method. The closing section states
plainly what its *evaluated* column means, and it never meant native loadability. What was missing was
the measurement, and this is it.

### The nine candidates the survey did not select

They were driven anyway, because the question they answer — how much of this ecosystem loads here
today — is worth a table and is expensive to re-derive.

| Candidate | Entry points driven | Verdict | Constructs the load exercises |
|---|---|---|---|
| `TechmanRobotInc/tmr_ros2` | five, `tm5-900` through `tm20` | **loads**, five of five | none |
| `neobotix/neo_simulation2` | four, `mp_400` through `mpo_700` | **loads**, four of four | none |
| `ROBOTIS-GIT/open_manipulator` | four, including `omy_3m` and `omy_l100` | **loads**, four of four | none |
| `DoosanRobotics/doosan-robot2` | two, `m1013` and `h2017` | **loads**, two of two | none |
| `Interbotix/interbotix_ros_manipulators` | two, `px150` and `uxarm7` | **loads**, two of two | none |
| `unitreerobotics/unitree_ros` | `go2`, a legged topology | **loads** | none |
| " | `b1` | refuses | an unset substitution argument the driver is expected to supply — a usage question, not an evaluator gap |
| `flexivrobotics/flexiv_description` | `urdf/flexiv.urdf.xacro` | **refuses** | a bracket list literal, `${robot_type_str in ['AICO1-4-V1', …]}` — its *first* blocker |
| `pal-robotics/tiago_robot` | `tiago_description/robots/tiago.urdf.xacro` | **refuses** | the same form negated, `${base_type not in ['pmb2', 'omni_base']}` — also its first blocker |
| `rai-opensource/spot_ros2` | — | not applicable | ships no URDF or xacro at all; it is a driver repository, not a description candidate |

**Six vendors load natively today with no change to this library, and every one of them exercises
nothing.** That is a settled negative result rather than an absence of data: the construct set each
load exercises was read off the load itself with the same instrument the compatibility record uses,
validated three-for-three against that record's own rows before it was trusted, and it came back empty
every time. Their expression content is arithmetic. Two of the richest spellings in the best of them —
a cylindrical inertia and a solid-cylinder inertia — are already committed rows of this project's
expression record, character for character.

These nine repositories carry ten verdicts, because one of them was driven at two entry points that
disagree. None of the ten is promoted by loading. A candidate this page records as unmaintained stays
unmaintained; a candidate recorded with a last-push date keeps it; and none of them is recorded as
carrying a construct, because none was observed to carry one.

### The correlation this exposes, which is the survey's most useful conclusion

**Everything that loads is arithmetic, and everything with semantics reaches for a form this evaluator
refuses.** The six that load exercise nothing. The four that refuse — two here, and the two selected
candidates that are out of reach — refuse on a list comprehension, a set constructor, or a bracket list
literal. The gap-list filter this survey applied was not an arbitrary narrowing: it was tracking that
correlation, and a candidate rich enough to contribute a distinct case is, in this ecosystem, a
candidate written in a form this grammar does not read.

**One form would change the answer more than any other: the bracket list literal.** It is the first
blocker for `flexivrobotics/flexiv_description` and for `pal-robotics/tiago_robot`; it is what keeps
the two-arm and mobile two-arm assemblies of the already-pinned Franka description outside the corpus;
and it is one of several things `shadow-robot/sr_common` needs. Four independent upstreams stand behind
one spelling.

## What was checked, and what was not

Stated plainly, so a later reader can tell the survey's reach from its conclusions.

- **How candidates were reached.** Code search over public repositories for the spellings each gap-list
  construct requires — an explicit-separator split, an integer subscript, a negative index, a
  keyword-argument `dict(`, a quoted literal on the left of `in`, and a merge key in an auxiliary
  document — followed by reading each hit's repository directly. A search finds what it is spelled for;
  a construct written in a spelling nobody guessed would not have been found.
- **How auxiliary documents were checked.** Every `.yaml` and `.yml` file in every candidate
  repository was parsed with the reference reader, and every key of every mapping was inspected for its
  type. That is a parse rather than a pattern match, which is why the `non-string-key` negative is worth
  something. It is also indiscriminate: it reads a repository's build configuration alongside its robot
  configuration, which is why the merge-key rows above say *which* file carried it.
- **What "evaluated" means and does not mean.** It means the reference tooling reported evaluating that
  expression while rendering that document. It does **not** mean the document loads in meios — several
  of these descriptions use comprehensions, method calls on loaded mappings and namespaced helpers this
  evaluator refuses, and any of them would need that measured before it could be pinned.
- **What was not checked.** No license was verified beyond the upstream's own declaration. No candidate
  was rendered on more than one argument set unless the row says so. No repository outside those in the
  authority table was searched, and no non-public source of any kind was consulted. No description here
  was invented, and no row records a construct that was not read in the source at the revision named.
