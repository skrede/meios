# Evaluation: what a description's expressions may run

A xacro description carries executable expressions, and this page is the contract for what meios will
and will not run when it evaluates them. It is the one place that owns the boundary: the supported
grammar, the rules that refuse everything else, the ceilings that bound one load, where those rules
diverge from the compatibility target, exactly what the running evidence for that comparison covers,
how the resource helper reaches a file, what a loaded configuration is readable by, and the explicit
backend for a description that needs more than the grammar carries.

The evaluator that runs is compiled into the library. It needs no interpreter, no Python installation
and no Python packages; nothing about it is opt-in. `meios::yaml`, which is what lets an expression
read an auxiliary document, is built by default too, so the plain `load()` call in
[getting started](getting-started.md) resolves a description whose macros read a joint-limits file
with nothing installed beyond a compiler and CMake. The two libraries it needs, pugixml and yaml-cpp,
are each found on the system or fetched at configure time.

What it evaluates is a fixed, closed grammar rather than a language: range-checked arithmetic over
integers and reals, comparison, boolean logic, membership and subscripting into a mapping, a fixed
set of mathematics functions, and one namespaced call that reads an auxiliary document. A construct
outside that grammar is refused with a located diagnostic naming what it met — never evaluated, and
never quietly approximated. `meios::eval-python` is a separate, opt-in backend for a trusted
description needing Python behavior the grammar does not carry; it has
[its own section](#meioseval-python-an-explicit-backend) near the end.

## What evaluates

- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `**`, and unary `+`/`-`. Integer arithmetic stays
  integer — `2 + 3` is `5`, not `5.0` — while `/` is always true division, so `4 / 2` is `2.0`.
  Every integer operation is range-checked and every real result must be finite, so a crossed range,
  a division by zero and a non-finite result are all loud failures rather than a wrapped value or an
  `inf` reaching your document.
- Comparison: `<`, `<=`, `>`, `>=`, `==`, `!=`, chained the way Python chains them, so `0 < x < 1`
  reads as both comparisons rather than as a comparison against a boolean.
- Boolean logic: `and`, `or`, `not`, short-circuiting — an operand that cannot change the result is
  never evaluated.
- The conditional expression, `a if condition else b`. Only the branch taken is evaluated.
- Membership: `key in mapping`, where the key reads as text.
- Subscripting a mapping, including a chain of them: `${joint_limits['A1']['lower']}`.
- `xacro.load_yaml(name)`, under the rules in [the resource helper](#the-resource-helper).
- Every property, argument and macro parameter the description itself has bound. `True` and `False`
  are the boolean literals, and `pi` answers `3.141592653589793` without the description binding it.

Mathematics is a fixed set of functions, called by bare name and never through a module:

```
abs   acos   asin   atan   atan2   ceil   cos   degrees
floor   max   min   radians   sin   sqrt   tan
```

`min` and `max` take one or more arguments — zero is a fault, not an identity — `atan2` takes exactly
two, and the rest take exactly one. `abs` of an integer is an integer; `floor` and `ceil` yield
integers whatever they are handed. Any other name followed by `(` is refused by that name, so a
description reaching for a function this list does not carry is told which one it reached for rather
than that something went wrong. A name on the list called at the wrong arity is refused the same way,
by name, so `atan2(x)` reports the function rather than the count.

**The kinds a value can have.** An expression works over exactly seven: a null, a boolean, an
integer, a real, a string, a sequence and a mapping. The first five have a scalar spelling and can be
written into a document; a sequence and a mapping have none, so one reaching a document's text is a
located, typed failure naming the kind rather than an invented serialization. Those last two arrive
only from an auxiliary document — nothing in the grammar constructs one.

**A string is text, and only text.** String literals evaluate, compare for equality against another
string, and serve as mapping keys. They carry no arithmetic and no truth value here: `'mesh.' + name`
and `not name` are refused rather than concatenated or tested for emptiness, and so is ordering one
string against another. The measured surface contains no such comparison, and coercing a string to a
number is how a description quietly means something other than what it says.

## What refuses, and by which rule

A refused expression is never evaluated. Which refusals the active `eval_policy` may soften is
decided by the failure's kind, and there are exactly four:

| Kind | What produces it | Terminal |
|------|------------------|----------|
| `unsupported` | a construct outside the grammar that a fuller backend would evaluate — a comprehension, an f-string, a string method, a Python constructor, a dotted read, arithmetic on a string | no: a lenient policy may leave the span verbatim |
| `error` | a genuine fault — an undefined name, a missing key, division by zero, a crossed integer range, a non-finite result, malformed syntax | yes, under every policy |
| `exhausted` | a crossed resource ceiling | yes, and it halts the load rather than the expression |
| `refused` | a rule of the opt-in Python backend; the built-in evaluator never produces one | yes, under every policy |

Every diagnostic carries a `file:line:column` and a `diagnostic_code`, so a caller branches on a code
rather than matching message text. `unsupported_expression` covers the whole first row,
`undefined_property` covers both a name the scope does not bind and a key a mapping does not hold,
and `expansion_budget_exceeded` covers a crossed ceiling. The message names the lexeme the walk
stopped on, which is what tells you where in a long expression to look.

**Malformed syntax is refused before anything is evaluated.** One left-to-right walk over the token
stream alternates operands and operators, so `1 +` is reported as an expression ending where an
operand is expected and `f(1 + )` as an unexpected `)`. A closing bracket is the one token whose
legality turns on what stands before it, which is why an empty argument list is accepted and a
truncated argument is not.

**A dotted spelling is refused, with one exception.** `xacro.load_yaml` is the one namespaced call
the grammar carries. Every other dotted spelling — `math.pi`, a mapping read as `config.limits`, a
method on a value — is refused, because a description that means something here other than what it
means upstream is worse than a description that will not load.

**The Python constructors are refused by name.** `dict(...)`, `list(...)`, `set(...)` and
`tuple(...)` are named outright, so the diagnostic says which constructor was reached for instead of
reporting an unrecognized function.

**Membership and subscripting both require a mapping.** `list[0]` and `x in list` are refused, and so
is a subscript key that is not text. An auxiliary document's sequence therefore crosses a property
boundary and reaches a macro intact, but is not indexable here. The two refusals are not the same
kind: `x in list` is `unsupported`, so a lenient policy may leave the span verbatim, while `list[0]`
is an `error` and terminal under every policy — as is a key that is not text, on either construct.

**Six ceilings bound the evaluator.** They are the auxiliary document's node count (100 000) and
nesting depth (64), and the bytes read (8 000 000), tokens lexed (1 000 000), evaluation steps
(10 000 000) and expression nesting depth (256). Four of them accumulate across every expression in
one load rather than resetting per expression, so a document cannot spend a bounded budget an
unbounded number of times; the two depth axes are high-water marks instead, because reaching a
nesting level is what they bound and not how often it is reached. A ceiling requested as zero is
refused and takes its default, so there is no spelling that means unbounded. Crossing one is terminal
and stops the load at the position that crossed it: a load continuing past its own ceiling would
report a partial answer as a whole one.

Expansion carries two more that are not the evaluator's: a work count (1 000 000) over nodes
visited, macro instantiations and substitutions, and an emitted-node count (100 000). They bound a
shallow-but-wide macro fan-out that no expression ceiling would catch, and they report under the same
`expansion_budget_exceeded` code — so a crossed ceiling naming a work limit is one of these two
rather than one of the six. None of the eight is reachable through `load()`; that entry point always
runs them at their defaults, and only the `meios::xacro` seam takes different ones.

## What this costs

The refusals above are correct for the grammar and still cost a real description something. You
should meet them here rather than in a document that mysteriously stopped loading.

**A filename composed by concatenation refuses.** `${'mesh.' + suffix}` is refused, because a string
has no arithmetic meaning here. The trade was taken deliberately: the alternative is deciding what
`'2' + 2` means, and every such decision is a place a description can mean something other than what
it says. Text is composed by writing it — a substitution span beside literal characters,
`filename="meshes/${name}.stl"`, is not an expression and is unaffected.

**A list is readable but not indexable.** A configuration whose value is a sequence crosses a
property boundary and reaches a macro whole, and `${limits['A1']['range'][0]}` is still refused. A
description that indexes a sequence needs the explicit backend — and until it gets one the refusal is
terminal, so no policy will carry the load past it by leaving the span verbatim.

**`xacro.load_yaml` takes a bound name, not a literal.** `${xacro.load_yaml('config/limits.yaml')}`
refuses; the spec must be bound to a property, an argument or a macro parameter first, and
`${xacro.load_yaml(limits_file)}` then reads it. That is how every measured description already
writes it, so the cost is narrow — but it is a refusal, not a coercion, and a document written the
other way will say so at the call rather than silently reading nothing.

**One unit tag converts.** `!degrees` is converted, and only when its text is a finite numeric
literal. Every other tag declines, which is a softenable refusal rather than a fault, so a lenient
policy leaves the span verbatim instead of failing the load.

## Where this diverges from the compatibility target

The reference is xacro's own evaluation of a `${}` body through Python. The two differ in both
directions, and every difference below is measured against a pinned upstream rather than reasoned
about from its source.

**Tighter, by construction rather than by restriction.** The reference hands an expression to an
interpreter alongside a symbol table; there is no interpreter here and no table to widen.
Comprehensions, generator expressions, lambdas, f-strings, string methods, the constructors,
attribute access and the import machinery are not restricted — they are absent from the grammar, and
an expression reaching for one is refused with a located diagnostic. Nothing in the grammar can name
the filesystem, the network or the process: the one route to a file is the resource helper, where C++
resolves the spec, enforces containment and reads the bytes.

**`and` and `or` yield a boolean, not the deciding operand.** `${1 or 2}` renders `True` here and `1`
upstream; `${2 and 3}` renders `True` here and `3`. This is measured and deliberately unchanged — the
grammar's logical operators produce booleans, and the measured descriptions use them in conditions
rather than for their operand. It is recorded as a reviewed divergence, which means the comparison
fails if it stops reproducing exactly as much as it fails if a new one appears.

**A missing key is named; the mapping's other keys are not.** The reference raises a bare key error.
Here the diagnostic names the key that was absent and the `file:line` that read it, and deliberately
never lists the keys the mapping does hold: a diagnostic must not spill the configuration it declined
to read. Against a configuration nested three mappings deep that costs you one lookup you can make
yourself, and it means a message written into a build log carries no auxiliary content.

**Auxiliary scalars resolve to the same kinds.** How a scalar in an auxiliary document reads is a
recorded measurement against the pinned reader rather than a reading of the YAML specification, and
it agrees row for row: `yes` and `no` are booleans while `y` and `n` are strings, `010` is the
integer 8 and `1_000` is 1000, `1e5` is a string while `1.0e+5` is a real, and a key written with no
value at all is a null that spells `None`.

**Parity gaps that predate this grammar.** The mathematics names are also reachable through a `math`
namespace upstream, so `${math.pi}` works there and is refused here as a dotted read. The reference's
argument, tokenizing and message helpers (`xacro.arg`, `xacro.tokenize`, `xacro.message` / `warning`
/ `error` / `fatal`) are not exposed at all. `map` and `filter` are not among the functions above,
and `${list(map(...))}` does appear in real descriptions, which makes it the omission most likely to
be met in practice. Every one of these is a loud refusal naming what it met, so it is a decision
rather than a silent difference in what a document means.

## What this evidence covers

Everything this page claims about agreement with the compatibility target rests on a comparison that
runs on every push, not on a reading of upstream's source. Three things are compared for each input:
a fresh render produced by the pinned upstream at that moment, the committed record of what upstream
produced when it was last measured, and what meios produces. A fresh render disagreeing with a
committed record fails the comparison as loudly as a meios-versus-upstream disagreement does, so a
re-measurement is surfaced rather than absorbed by rewriting the record.

What is compared, exactly:

- **The pinned upstream tooling** — `xacro` 2.1.1 and PyYAML 6.0.3, and no other distribution.
- **Universal Robots ROS2 Description at tag `4.3.1`** (archive digest `3532a25c9942…`), through five
  documents, each paired by name with its own measured record:
  - the `ur5e` variant of the top-level document; that same document with its safety-limit elements
    switched on; and that same document again with its mesh references forced to absolute paths;
  - the `ur3e` variant, where a boolean read out of an auxiliary document types a joint and that type
    then reaches a limit element carrying no position keys;
  - the `ur7e` variant, whose configuration upstream ships as a verbatim copy of `ur5e`'s, so it
    renders the same document and its record is the same record. It is measured because upstream
    names the variant, not because it adds a form the other four do not reach.
- **`kuka_experimental`'s `kr6r900sixx.xacro`** — the one measured document resolving `$(find)`
  across two sibling packages.
- **KUKA LBR Med 14 R820 at tag `v2.5.0`** (archive digest `edb596d3e2b7…`), whose macro reads a
  joint-limits document through `xacro.load_yaml` and computes every joint's limits out of it.
- **Twenty-three minimized expression cases** — one for each non-trivial expression form in the
  closure of the Universal Robots document: the joint-limit arithmetic, the inertia arithmetic, the
  string comparison, the membership test, the subscript chain, `pi`, and the four auxiliary-document
  loads. Each is driven through a document seeding exactly the names it reads, so a failing case
  names one form. That closure is what bounds the set: a grammar form the Universal Robots document
  never writes — `//`, `%`, `not`, a chained comparison, the conditional expression, most of the
  mathematics names — is accepted by the evaluator and measured against upstream by nothing here.

Every comparison the run owes is named in a committed inventory, and a named comparison that produces
no verdict fails the run before any pass or fail count is reported — a gate cannot go quiet by
matching nothing. Each known divergence is listed in a reviewed manifest by its exact text, in both
directions: an unlisted divergence fails the run, and a listed one that no longer reproduces fails it
too.

The comparison against a fresh upstream render runs on Linux, where the pinned upstream tooling is
installed and run. The documents themselves are not confined to it: every push builds the library and
runs its suite on Linux, macOS and Windows, and on each of the three a consumer outside this project
loads the pinned Universal Robots and KR6 documents natively and checks them against the same
recorded facts. Agreement with upstream is established on one platform; that these documents resolve
to the same facts is established on all three.

That is the whole of it. The claim extends no further than these documents at these revisions and
these expression forms. Another vendor's authoring style, another revision of the same upstream, or
an expression form the closure above does not contain is covered by nothing on this page, and
[known limitations](known-limitations.md) is where the rest of what is not yet proven is written
down.

## What this is not

**This is not a sandbox, and it does not need to be one.** There is no interpreter to escape and no
authority to withdraw: the grammar has no import machinery, no attribute access, no `open` and no way
to name one. Those are absent rather than withheld, so there is no denylist to keep current as a
future language grows a name, and no reach to close. The one thing an expression touches outside
itself is an auxiliary text document, and C++ resolves, contains and reads that before the evaluator
sees a byte of it.

That does not make a hostile description harmless, and nothing here should be read as containment. A
description you did not write still chooses which files the helper is asked for, what arithmetic
runs, and how deep and how long it runs for. Containment is what bounds the first of those and the
ceilings above are what bound the last, and neither is a reason to load a description you would not
read first.

## The resource helper

`xacro.load_yaml(spec)` is the only way an expression reaches a file, and it does not read one
itself: `spec` names a property, argument or macro parameter whose text is the specification, C++
resolves that text, enforces containment and reads the bytes, and the parser is handed *text*. An
evaluation scope with no resource loader installed refuses every spec, and a build without
`meios::yaml` reports that it resolves no auxiliary document format at all: the capability is absent
by declaration, its absence is a checked refusal, and there is no fallback read anywhere.

Three spec forms are accepted:

| Form | Resolution | Refused by shape |
|------|------------|------------------|
| `package://<pkg>/<rel>` | through the source stack, the same resolution a mesh or an include takes | no divider at all, an empty package half, an empty relative half, and a relative half opening on a further separator, under `malformed_asset_uri` |
| `$(find <pkg>)/<rel>` | the same — both spellings split to a package/relative pair and take one lookup | the same four under the same code; this row is the spec form, never the bare `$(find <pkg>)` substitution command |
| anything else | probed against the calling document's directory, then against each configured containment root | — |

A spelling refused by shape is decided before the source stack is asked, so no lookup, no containment
decision and no filesystem call runs for it — the same order the asset grammar keeps.

The two package spellings agree on which four shapes they refuse but not on what divides them: the
`$(find …)` splitter treats a backslash as a divider and as a further separator, while the
`package://` splitter recognizes only a forward slash. So `$(find pkg)\cfg.yaml` splits and resolves
where `package://pkg\cfg.yaml` is malformed. Write the separator as `/` in both and the distinction
never arises.

Containment is judged on the **resolved candidate**, never on the authored spelling. An absolute path
that canonicalizes inside a root is accepted; one that does not is refused under `uncontained_asset`,
and so are a `../` escape and a symlink inside a root whose real target leaves it. The rule has to be
"lands inside a root" rather than "does not look absolute", because the two substitution entry points
hand the resolver different shapes for the same authored form. A resource one root excludes and
another holds the place for is reported as missing rather than as escaping, since the second root
answered the containment question the first one raised.

**Which document a relative spec resolves against.** The document it is written in — which is not
always the top-level one:

- Inside an included file, that included file. A spec whose text is a bare `inner-config.yaml`, bound
  in a file pulled in by `<xacro:include>`, resolves next to *that* file, not next to the description
  that included it.
- Inside a macro body, the document that **invoked** the macro, not the one that defined it. This
  matches the compatibility target and is not obvious from reading the macro: a macro that loads a
  configuration sitting beside its own definition will not find it when called from elsewhere.
- A document served as bytes by a source layer does have a directory: such a layer materializes the
  bytes into a scratch area it owns and hands back the path of the file it wrote, so a relative spec
  written in that document resolves beside it exactly as it would beside a document read off disk.
  The scratch tree lives for as long as the source does. Source kind does not distinguish the two
  package spec forms: they split into the same package/relative pair and read the returned file's
  text.

**Windows may over-refuse, and it is unmeasured.** Containment canonicalizes both paths and then
compares them lexically, with no case folding and no expansion of short (8.3) path names of its own.
Whether a candidate that differs from its root only in case, or that arrives in short form, therefore
compares unequal and is refused depends entirely on how much the standard library's
`weakly_canonical` already normalized: an implementation that resolves the existing prefix through
the filesystem repairs both spellings before the comparison sees them, and one that does not leaves
them to compare unequal. No test in this project exercises either, on any platform.

What the direction guarantees is the part that matters: the comparison can only refuse a path it
should have accepted, never accept one it should have refused. It fails closed. Treat this as the
first thing to suspect behind an unexplained containment refusal on Windows, and as a claim this page
has not earned the right to state either way.

## Reading a loaded configuration

A mapping or a sequence that came back from `xacro.load_yaml` is a value like any other: it binds to
a property, passes as a macro argument, and is read by subscript. What it does not do is answer a
dotted read — `${config.joint_limits}` is refused under the dotted rule above, and
`${config['joint_limits']}` is how the same value is read here.

**What crosses a property boundary.** A property binds text, so a mapping or a sequence crossing a
`xacro:property` boundary is carried across inside a text encoding and restored on the other side.
The third property out reads exactly as the first does, and there is no hop limit to exceed:

```xml
<xacro:arg name="limits" default="$(find my_description)/config/joint_limits.yaml"/>
<xacro:property name="limits_file" value="$(arg limits)"/>
<xacro:property name="config" value="${xacro.load_yaml(limits_file)}"/>
<xacro:property name="joint_limits" value="${config['joint_limits']}"/>

<joint name="shoulder_pan" type="revolute">
  <parent link="base_link"/>
  <child link="shoulder_link"/>
  <axis xyz="0 0 1"/>
  <limit lower="${joint_limits['shoulder_pan']['min']}"
         upper="${joint_limits['shoulder_pan']['max']}"
         effort="${joint_limits['shoulder_pan']['effort']}"
         velocity="${joint_limits['shoulder_pan']['velocity']}"/>
</joint>
```

**What does not carry.** A container an author wrote literally in a value is text and stays text: a
property whose value is `{'a': 1}` binds that string, and subscripting it is refused rather than
answered. Nothing in the grammar constructs a mapping or a sequence, so there is no rebuilt-container
case to describe — a container's origin is always an auxiliary document.

**A control character an author writes is erased.** A container crosses a property boundary inside a
text encoding delimited by control characters the XML 1.0 charset clause forbids outright, so one can
never appear in a legitimate document. An author who writes one anyway — as a raw byte, or as the
character-reference spelling of the same code point — gains nothing by it: the character is erased
from authored text before that text becomes a property value, an argument default, a macro parameter
or a caller-supplied argument. The reachability an auxiliary load grants therefore cannot be forged
from a description, and the same erase runs at the output seams, so no flattened document carries one
either.

**A missing key fails where it is read.** `${config['nope']}` names the key and the position that
read it, and does not expand to an empty string. It does not name the keys the mapping does hold, for
the reason given above.

## What an expansion hands back

Expansion is a result, never a flag beside a document. `meios::expand` returns either an `expansion`
carrying the expanded document and nothing else, or an `expansion_error` carrying the failing
location, a `diagnostic_code`, a message and — where a refused system operation caused it — that
operation's native cause. A terminal failure is reported exactly once through the sink you supplied
and returned on the error arm, so you neither log it again nor risk seeing it twice. Warnings, the
environment-read note and every other nonterminal diagnostic keep travelling that same sink,
unaffected. A span the active policy declines to resolve is a **success** whose span is left
verbatim; [what refuses, and by which rule](#what-refuses-and-by-which-rule) is what decides which
failures are terminal at all. `meios::substitute` publishes the same two arms over the same record.

<!-- meios:snippet name=expansion-result tu -->
```cpp
#include <meios/io.h>
#include <meios/xacro.h>

#include <iostream>

int main()
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    meios::log_sink log;

    const meios::expected<meios::expansion, meios::expansion_error> expanded =
        meios::expand("<robot name=\"r\"/>", scope, sources, "robot.urdf.xacro",
                      meios::expansion_limits{}, log);
    if (!expanded)
    {
        const meios::expansion_error &err = expanded.error();
        std::cout << meios::to_string(err.loc) << " (" << meios::to_string(err.code) << ") "
                  << err.message << '\n';
        return 1;
    }
    std::cout << expanded->document.size() << " bytes\n";
}
```

## meios::eval-python: an explicit backend

`meios::eval-python` is a separate module, off by default, for a description you trust that needs
Python behavior the grammar above does not carry — a comprehension, an f-string, a string method, a
dotted read, arithmetic on a string, or a wider set of unit tags, whose tagged text it evaluates as
an expression the way the reference does. It drives a *found* (never fetched) CPython. Its default
class, `meios::python_evaluator`, is restricted: it evaluates a documented subset and refuses
anything outside it with a `file:line` diagnostic naming one of four rules — `dunder-identifier`,
`format-traversal`, `non-allowlisted-builtin`, `uncontained-yaml-path`. Its sibling
`meios::unrestricted_python_evaluator` applies none of those rules and evaluates whatever CPython
evaluates, `${__import__('os').getcwd()}` included.

The unrestricted class is reachable only from C++. The command-line tool never constructs it and
carries no flag for it, so arbitrary code execution is never one flag away from an ad-hoc invocation
on a file somebody sent you; choosing it is a compile-time act, an include line and a construction
site, both visible in review. What you accept by choosing it is that every expression in every
description you load — including ones pulled in by an include, from a package you did not write —
runs with your process's authority. Choose it for descriptions you would be willing to run as a
script. Both classes share one resolution and containment implementation with each other and with
the built-in evaluator, so the helper's containment holds under all three; what the unrestricted
class widens is the expression itself, which is enough to reach the filesystem directly, `open`
included.

Resource exhaustion is unbounded under this backend, where the built-in evaluator's six ceilings do
not apply: `${10**10**10}` and `${[0]*10**12}` are refused by nothing there and will burn processor
time and memory. A real bound needs a per-expression watchdog against an embedded interpreter holding
the interpreter lock, portable across all three supported platforms, and there is none.

The enrichment embeds that interpreter, and an embedded interpreter binds a binary to one of them.
Everything that links it — including the command-line tool, which an install prefix carries — is
compiled against a single CPython minor version and loads that version's runtime and no other, so a
binary built here does not run on a machine carrying a different one. There is no negotiation at
startup and no fallback to the built-in evaluator: the failure is at load, before any description is
read. Build against the interpreter you have.

Neither class reaches an installed package. The module's embedded-interpreter link edge cannot be
re-resolved from an install tree, so its archive and its headers are withheld together and
`meios_eval-python_FOUND` is false in every package installed from this project; build it and link it
in-tree and it works.

## Explicit non-goals

**Duration is not one of the ceilings.** The step ceiling bounds how much work an expression may do,
not how long that work takes, and none of the six adapts to the machine it runs on. There is no
per-expression timeout, and a load whose expressions all stay inside their budgets has no
wall-clock guarantee of any kind.

**Nothing here validates an auxiliary document against a schema.** `xacro.load_yaml` hands back
whatever the document holds. A key that is absent, or present carrying a string where the description
arithmetic wants a number, surfaces at the expression that reads it and not at the load that parsed
it — which is why the ceilings bound the parse and the diagnostics name the reader.

**The fuzzing is a robustness check, not a proof of this boundary.** A harness drives this evaluator
directly on arbitrary bytes, asserting that no input makes it invoke undefined behavior; a loud
failure result is an acceptable outcome there and a sanitizer trap is not. That is a genuine property
of a parser that must not crash on hostile-looking input, and it says nothing about whether the
grammar admits the right forms. The proof of *that* is the table-driven suites and the comparison
above: `tests/golden/oracle/expressions.cases` states every measured expression, its upstream
rendering and its verdict, and `tests/unit/native_expression_test.cpp` and
`tests/integration/native_differential_test.cpp` run that table through the evaluator seam and
through a full document expansion.
