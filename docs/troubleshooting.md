# Troubleshooting: a diagnostic code, or a message that stopped your build

This page is keyed on what you have in front of you — the code the library handed you, or the line
your configure stopped on. Each entry says what it means in your terms, what to do about it, and
which guide owns the rule behind it. Those guides state the rules; this page does not repeat them.

## Where a diagnostic comes from

meios installs no output stream of its own, so nothing is printed unless you ask. Every diagnostic is
handed to a `log_sink` you supply, and the same diagnostics are carried on the result of the load
whether it succeeded or failed. A program that injects no sink sees nothing at the moment a
diagnostic is raised and reads them off the result afterwards instead.

A `capturing_log_sink` forwards every record to an inner sink and keeps all of them; a
`capture_window` taken over that sink answers for one load only, so one sink can serve several loads
without a later one inheriting an earlier one's errors. Each record carries the code, the
`file:line:column` it was raised at, the message, and — where the operating system was involved — the
cause it reported.

**A successful load is not the same as a clean load.** A value on the success arm says meios produced
a model, not that the document was clean. Branch on the three completeness claims the result carries
rather than on the presence of a value; the reasons are in
[known limitations](known-limitations.md#diagnostics).

The programs under `examples/diagnostics/` capture diagnostics, print them, and branch on the claims,
end to end.

## Reading the diagnostics of one load

<!-- meios:snippet name=read-diagnostics tu -->

```cpp
#include <meios/urdf.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <iostream>

int main()
{
    meios::log_sink_s stream(std::cerr);
    meios::capturing_log_sink capture(stream);
    const meios::capture_window window(capture);

    meios::load("arm.urdf", meios::load_options{}, window.log());

    for(const meios::captured_diagnostic &record : window.records())
        std::cout << meios::to_string(record.loc) << ": " << meios::to_string(record.code) << ": "
                  << record.message << '\n';

    return 0;
}
```

`to_string` on the code gives you the spelling used throughout this page, so the entry you need is
the one whose row names it.

## Diagnostic codes

Search for the code you were handed. Codes that share a cause and an action share a row, and every
row names each code it covers, so grouping never hides one.

### meios could not read a file

| Code | What it means | What to do |
|---|---|---|
| `cannot_open` | A file could not be read: the document you handed `load()`, a resource an expression asked for, or an asset being copied into a bundle. The record carries the operating system's own reason as its cause. | Check the path exists and that the process may read it. A `package://` reference that reached no file is a different code — see [meios could not find an asset](#meios-could-not-find-an-asset). |

### The bytes are not a document meios reads

| Code | What it means | What to do |
|---|---|---|
| `xml_parse_error` | The XML did not parse. The message carries the parser's own description, anchored at the offset where it gave up. | Fix the markup at the reported `file:line`. |
| `non_robot_root` | The root element is something other than `<robot>`. | Hand `load()` a URDF document. An SDF or a launch file is not one. |
| `no_links` | The `<robot>` element declares no `<link>` at all. | A description with no links describes no robot; see [the profile](urdf-profile.md#rules). |

### XML the document should not contain

| Codes | What it means | What to do |
|---|---|---|
| `duplicate_attribute`, `additional_root_element`, `trailing_content`, `comment_interrupting` | The document is not well-formed in a way the XML parser tolerated but the profile does not: an attribute written twice, a second root element, text after the root, or a comment interrupting an element's text. | Fix the markup. These four are graded together by the document-validity setting — refused under the strict default, disclosed at warning level under the permissive ones. What each setting does is in [the profile](urdf-profile.md#where-this-profile-decides-on-its-own-authority). |

### Content meios does not read

| Codes | What it means | What to do |
|---|---|---|
| `unknown_element`, `unknown_attribute` | Content the profile does not describe. It is not read into the model, and the drop is named at its own `file:line` rather than passed over. It never fails a load, and it withdraws the parse claim so you can branch on it. | Nothing, if you did not need that content in the model. [The profile](urdf-profile.md#rules) states what is described. |
| `extension_ignored` | A `<gazebo>`, `<ros2_control>`, `<transmission>` or `<sensor>` block was recognized and dropped. **This is not an error**, it clears no completeness claim, and nearly every shipped description raises it. | Nothing. That the extension record type is never populated is a live gap, recorded in [known limitations](known-limitations.md#modeling-gaps). |
| `unsupported_version` | A `<robot>` carries a `version` attribute other than `1.0`. | Remove the attribute or set it to `1.0`. |

### A name or a reference that does not hold

| Codes | What it means | What to do |
|---|---|---|
| `empty_name`, `duplicate_name`, `undeclared_link`, `dangling_mimic` | An identity or a reference in the document does not hold: a name absent, empty or blank; a name a second element already carries; a `<parent>` or `<child>` naming a link the document never declares; a `<mimic>` whose target is undeclared or is the joint itself. | Fix the document. These are refused at **every** policy setting — no setting softens them, because a reference to something never declared is exactly the input that turns into plausible physics if it is read anyway. [The profile](urdf-profile.md#rules) names the rule behind each one. |

### A field meios could not read

| Codes | What it means | What to do |
|---|---|---|
| `missing_required_field`, `missing_joint_type`, `missing_geometry`, `missing_limit` | A part the profile requires is absent — a `<mass>`, a joint `type`, a `<geometry>`, a `<limit>` on a bounded joint, or one of the attributes those carry. The containing element is dropped rather than completed with a value nobody wrote. | Supply the missing part. [The profile's rule table](urdf-profile.md#rules) names the element and the attribute for each code, and states exactly where the drop stops. |
| `invalid_number`, `vector_arity`, `unknown_joint_type`, `unknown_geometry_shape`, `zero_axis`, `invalid_mass`, `invalid_inertia` | A value is present and meios refuses it: text that is not a finite number, a fixed-length vector with the wrong count, a joint type or geometry shape outside the declared set, an all-zero axis on a joint that turns about it, a negative mass, or an inertia tensor that cannot describe a rigid body. | Correct the value. The verdict and the reasoning for each are in [the profile](urdf-profile.md#rules). |

### The robot's graph

| Codes | What it means | What to do |
|---|---|---|
| `additional_root`, `no_root_cycle`, `link_on_cycle`, `multiple_parents`, `unreachable_link` | Properties of the assembled graph rather than of the document text: more than one root link, no root at all, a link lying on a cycle, a link with two parent joints, a link no path reaches. These are what the graph policy grades. | Fix the joints. Do not set the graph policy to `skip` and then branch on the topology claim — that combination leaves the claim standing on a graph that was never checked, and it is a live defect recorded in [known limitations](known-limitations.md#diagnostics). |
| `invalid_topology` | A joint names a link that is absent from the model. The load rules refuse an undeclared reference long before this point, so this can only come from a model assembled by hand outside them. No policy grades it. | The model did not come from `load()`. Fix the code that built it; see the [engine guide](engine-guide.md). |

### A material nothing defines

| Code | What it means | What to do |
|---|---|---|
| `undefined_material` | A `<visual>` names a material that neither defines a color or texture of its own nor matches any robot-level `<material>`. | Define the material at robot level, or give the visual its own. The material policy grades this one. |

### meios could not find an asset

| Code | What it means | What to do |
|---|---|---|
| `unresolved_asset` | The reference is well formed and names a location meios is allowed to reach, and there is no regular file there — a missing mesh, a directory where a file was expected, or a `package://` no configured source supplies. The missing-asset policy grades it. | Put the file where the reference names it, or register a source that holds the package. Note that silencing the diagnostic does not restore the asset: every setting of that policy withdraws the same deployment-completeness claim, so branch on the claim rather than on whether a warning appeared. If a relative mesh path written inside an *included* file will not resolve, that is a known defect and not your document — see [known limitations](known-limitations.md#sources-and-resolution) and [asset resolution](asset-resolution.md#base-context). |
| `lfs_pointer_asset` | The file resolved, and its contents are an unsmudged Git-LFS pointer rather than geometry. | Fetch the LFS objects for that checkout before the description is loaded. |
| `malformed_asset_uri` | The reference names no asset at all: a `filename` present and empty, a `<texture>` carrying no `filename`, or a `package://` URI missing its package name or its relative half. | Write both halves of the reference. The accepted spellings are in [asset resolution](asset-resolution.md#accepted-forms). |
| `asset_write_failed` | meios's own environment failed — a source holding bytes could not create its scratch directory or could not write an asset into it. A full disk, or a temporary directory it may not write. It refuses the load at every setting and is deliberately never routed through the missing-asset policy, so a lowered setting cannot turn a full disk into a merely unresolved asset. | Check free space and that the temporary directory is writable. |
| `duplicate_asset_entry` | Your program offered the same file to a source twice, and the source is built to refuse the second offer. The first bytes keep resolving, every path already handed out still names that file, and nothing was left undeployed. | This says something about your program's configuration and nothing about the description. Offer each entry once. |

### meios will not reach an asset

These three are traversal controls. The reference is intelligible and meios declines to follow it;
that is the library working, not an obstacle between you and a load. Each has a lawful way to express
the same intent.

| Code | What it means | What to do |
|---|---|---|
| `uncontained_asset` | Canonicalized, the path resolves under no configured package root and not under the input document's directory. The check is on the canonical candidate, so a climb-out spelled through a real subdirectory and a symlink leaving the root are both caught. | If the directory holding that asset is one your program means to grant, register it as a package root: authority is granted per directory and by name. There is no setting that turns containment off — the escape is configuration rather than a knob, deliberately. If the path was never meant to leave the tree, the reference itself is wrong. See [containment](asset-resolution.md#containment). |
| `unsupported_uri_scheme` | The reference carries a scheme meios does not follow — `http://`, `model://`, or any other. Only `package://` and `file://` are followed. | Express the reference as `package://` backed by a configured source, or as a path inside a root. |
| `unsupported_uri_authority` | A `file://` URI names a host. | Name a local file. Both the authority-less form and the explicit local-host form resolve; the rules are in [accepted forms](asset-resolution.md#accepted-forms). |

### An expression or a substitution did not evaluate

| Code | What it means | What to do |
|---|---|---|
| `undefined_property` | `${name}` names nothing the description bound. | Declare the property, or pass it as an argument. |
| `unresolved_arg` | `$(arg name)` names an argument with no value and no declared default. | Pass the override, or give the `<xacro:arg>` a default. |
| `unresolved_env` | `$(env VAR)` names an environment variable that is not set. | Set it, or use `$(optenv VAR fallback)`, which is the form that tolerates absence. |
| `unresolved_find` | `$(find pkg)` names a package no configured source holds. | Add a source that holds it; the source stack is described in the [evaluation guide](evaluation.md). |
| `unknown_substitution` | A `$(...)` command meios does not implement. | The commands that are implemented are listed in the [evaluation guide](evaluation.md#what-evaluates). |
| `unterminated_substitution` | A `${` or a `$(` with no closing delimiter. | Close it. |
| `expression_error` | The expression was evaluated and failed — or the evaluator **refused** it under one of its four rules, in which case the message reads `meios refused the expression '<expr>': <rule>`. A refusal is a distinct failure kind that hard-fails under every policy, and the refused span is never copied verbatim into flattened output. | For a failure, fix the expression. For a refusal, express the value without the refused construct: [what refuses, and by which rule](evaluation.md#what-refuses-and-by-which-rule) states each rule, and [what this costs](evaluation.md#what-this-costs) gives the lawful spellings for the three refusals that bite an innocent description. |
| `unsupported_expression` | A construct the core evaluator does not implement. Under a lenient evaluation policy the span is left in the output verbatim instead of failing. | Write the expression in the fixed grammar the core evaluator implements, or build with the evaluation enrichment for full parity — see the [evaluation guide](evaluation.md). If you lower the evaluation policy, do not then branch on the parse claim: that combination leaves the claim standing on a document still carrying an unevaluated expression, and it is recorded in [known limitations](known-limitations.md#diagnostics). |

### A xacro construct

| Code | What it means | What to do |
|---|---|---|
| `unresolved_include` | An `<xacro:include>` names a file that could not be read. The cause carries the operating system's reason. | Check the path, and check that the package the include reaches through is held by a configured source. |
| `xacro_parse_error` | A xacro element or one of its attributes did not parse. | Fix the markup at the reported `file:line`. |
| `xacro_structural_error` | A xacro element is used where the grammar does not allow it — a macro call that does not match its definition, a block outside the scope it belongs to. | The constructs and their shapes are in the [evaluation guide](evaluation.md). |
| `expansion_budget_exceeded` | Expansion ran past its budget, which in practice means a macro or an include that recurses. | Break the cycle. A budget is reached by recursion far more often than by size. |

### A record carrying no code

| Code | What it means | What to do |
|---|---|---|
| `unspecified` | The record reached your sink through a `log_sink` overload that carries no code. Sources and callers log this way, and one evaluator warning is left uncoded deliberately, so that a warning cannot retract a completeness claim on a load that succeeded. | The message text is all there is. How diagnostics reach a sink you wrote is in the [engine guide](engine-guide.md). |

## Messages from the build

CMake prefixes a failure with `CMake Error` and stops. A line beginning with `--` is a **status
message**: it is the build telling you what it decided, the configure is still running, and there is
nothing to fix. Both kinds are below, separately, because mistaking one for the other sends you
looking for a problem that is not there.

CMake rewraps a long message across lines, so the text you see may be broken differently from the
text quoted here. Every option named below is defined once, with its default, in
[CMake integration](cmake-integration.md#options).

### The configure stopped

| Message | What it means | What to do |
|---|---|---|
| `meios: MEIOS_REQUIRE_EVAL_PYTHON is ON but Python3 Development.Embed was not found, so meios::eval-python would have been skipped.` | You asked for the Python evaluation enrichment to be mandatory, and the embeddable interpreter is not discoverable. The interpreter is a found environment resource; it is never fetched. | Install the Python development package that provides the embeddable library, or turn `MEIOS_REQUIRE_EVAL_PYTHON` off and accept the enrichment being skipped. |
| `meios: MEIOS_REQUIRE_EVAL_PYTHON is ON but meios::eval-python did not materialize.` | The interpreter was found, and the target still was not created — which means the module's sources were not in the tree. A found interpreter says nothing about that, so a required build asks about the target rather than about discovery. | Check the checkout is complete. |
| `meios: <target> is marked NOT_EXPORTABLE with no reason. State why the module cannot join the installed export, or remove the marker.` | Reached only when editing this project's own build files. A module withheld from the installed export must record why, because the status line the build prints is that reason. | Give the marker a reason string, or remove it. |
| `Documentation snippet extraction failed:` followed by the extractor's own message | A snippet sentinel in the documentation is malformed. The extractor's message names the file, the line, and the rule that was broken — a missing name, an unknown attribute, prose between the sentinel and the fence, or a fence tagged as something other than `cpp`. | Fix the sentinel the message names. |
| `Documentation snippet gate is enabled but no snippets were extracted.` | The documentation gate is on and nothing in the corpus is marked for compilation. Reached when editing the documentation, not when consuming meios. | Restore the sentinel that went missing, or turn `MEIOS_BUILD_DOCS` off. |
| `meios_target_flatten_resource(<target>): EVAL '<value>' is not an evaluator backend. Pass one of: core, python.` | The flatten helper was passed an `EVAL` that names no backend. | Pass `core` or `python`. |
| `meios_target_flatten_resource(<target>): EVAL python needs a meios binary built with the python evaluator, and this one is not. Configure with MEIOS_EVAL_PYTHON_SUPPORT=ON, or pass EVAL core.` | You asked to flatten with the Python evaluator, and the `meios` binary this build will run was not built with it. | Do what the message says. The two backends are not interchangeable and the choice is stated at configure time on purpose; see the [resource guide](resources-guide.md#meios_target_flatten_resource). |

### The configure kept going

| Message | What it is telling you |
|---|---|
| `meios: installing meios and its CMake resource modules into the install prefix.` | meios will install. This is the default at top level. |
| `meios: not installing, because MEIOS_INSTALL is off. …` | You turned installing off. Nothing meios builds reaches your prefix, the CMake resource modules included. |
| `meios: not installing, because meios was added as a subproject and MEIOS_INSTALL is not on. …` | meios was added with `add_subdirectory` and the toggle defaults off there. Set `MEIOS_INSTALL` **before** that call; setting it afterwards is too late. On this path the toggle never reaches the cache, so this line is the only signal either way. |
| `meios: not installing, because a fetched dependency declines to install itself and so belongs to no export set: pugixml (turn PUGIXML_INSTALL on). …` | A dependency was fetched from pinned source rather than found, and it will not install itself, so meios cannot export against it. Supply a discoverable version of that dependency instead, or turn the named option on before adding meios. |
| `meios: Python3 Development.Embed not found; skipping meios::eval-python.` | The Python evaluation enrichment was requested and the embeddable interpreter is absent, so that one target is skipped. The rest of the build proceeds. This is the line that becomes a hard error if you also set `MEIOS_REQUIRE_EVAL_PYTHON`. |
| `meios: <target> builds but is not installed, because <reason>` | The named module is built and linkable in-tree, and it does not join the installed package. Its archive and its headers are withheld together, so the absence is total rather than leaving you a header with nothing behind it. What an installed package carries is in [CMake integration](cmake-integration.md#what-an-installed-package-carries). |

One warning is worth naming, because it appears in this project's own test build and looks alarming:

```
CMake Warning:
  meios: no tracked-listfile manifest could be derived from git, so the listfile-scope
  case will skip rather than scan an unknown set.
```

The listfiles this repository answers for are the ones it tracks, and it asks git rather than
walking the working copy. Where git is unavailable, the case that scans them reports the absent
manifest and skips, rather than passing on an empty scan. Nothing is wrong with your build.

## Where to go next

- [The URDF profile](urdf-profile.md) — every rule meios enforces, and the code each one carries.
- [Asset resolution](asset-resolution.md) — what a reference may name, and what meios will reach.
- [Evaluation](evaluation.md) — what a description's expressions may run, and what refuses.
- [Known limitations](known-limitations.md) — the live defects, described by the effect you will see.
- [CMake integration](cmake-integration.md) — every option, both acquisition paths, and the export.
