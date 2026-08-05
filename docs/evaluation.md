# Evaluation: what a description's expressions may run

A xacro description carries executable expressions, and this page is the contract for what meios will
and will not run when it evaluates them. It is the one place that owns the boundary: the supported
subset, the literal rules that refuse everything else, where those rules diverge from the
compatibility target, how the resource helper resolves a file, the unrestricted backend and what
choosing it means, and the two things this boundary explicitly does not do.

The built-in **core evaluator** is always present, needs nothing outside the C++ standard library, and
evaluates a fixed numeric and boolean grammar. It cannot reach the filesystem, the network or the
process at all, and nothing below applies to it.

The **`meios::eval-python` enrichment** drives a found (never fetched) CPython. Its default class,
`meios::python_evaluator`, is restricted: it evaluates the subset described here, and refuses anything
outside it with a `file:line` diagnostic naming the rule that refused it. Its sibling
`meios::unrestricted_python_evaluator` evaluates whatever CPython evaluates; it has its own section
near the end.

## What evaluates

- Arithmetic, comparison and boolean operators over numbers and strings, including `**`, `//` and the
  `%` operator in both its numeric and its string-formatting sense.
- The mathematics module: every public name of Python's `math`, bound directly as a bare name, so
  `sqrt(2)`, `radians(180)` and `pi` all work. The module object itself is never bound, so the
  `math.<name>` spelling is not available and `${math.pi}` fails with a `NameError`.
- List, dict and set comprehensions, generator expressions, and lambdas.
- String operations — concatenation, slicing, `join`, `split`, the case and strip methods — with the
  single exception of the two formatting methods named in the next section.
- Subscripting and indexing, including into a mapping returned by the resource helper.
- Interpolated (f-) strings, with format specs and the `!s` conversion.
- `load_yaml` / `xacro.load_yaml`, under the rules in [The resource helper](#the-resource-helper).
- Every property, argument and macro parameter the description itself has bound.

Twenty builtin names are reachable, and no others:

```
abs   all   any   bool   dict   enumerate   float   int   len   list
max   min   range   round   set   sorted   str   sum   tuple   zip
```

`pow` also evaluates, and it is worth a sentence because the rule that admits it is derived rather
than curated. The withheld set is computed as `dir(builtins)` minus the twenty names above minus the
public `math` names. `pow` is the only name those last two share, so it is subtracted back out and
reaches a description as `math.pow` — which returns a float, so `${pow(2,3)}` renders `8.0`, not `8`.
Nothing here is a hand-maintained denylist: widening what is reachable means adding a name to the
allowlist, and a future CPython that grows a new builtin withholds it without anyone editing a list.

## What refuses, and by which rule

A refused expression is never evaluated. The refusal is a distinct failure kind — not the
"unsupported construct" kind a lenient evaluation policy may soften — so it hard-fails under every
policy, and the refused span is never copied verbatim into the flattened output. Every refusal
diagnostic names one of exactly four rules:

| Rule | Refuses |
|------|---------|
| `dunder-identifier` | an identifier beginning with a double underscore, anywhere in the expression |
| `format-traversal` | the string formatting primitives, by spelling |
| `non-allowlisted-builtin` | a builtin name outside the twenty above, and the `!r` / `!a` conversion fields |
| `uncontained-yaml-path` | a resource spec that does not resolve inside a containment root |

The expression is parsed with `ast.parse(expr, mode='eval')` — which builds a tree and does not
execute anything — and the rules are applied to that tree in the order above.

**The identifier test, literally.** Every `Name` identifier, every `Attribute` name, every keyword
argument name and every function parameter name in the tree is tested with `startswith('__')`. It is a
leading-double-underscore test, not a contains-a-dunder test. The walk descends into lambda bodies and
comprehension bodies, and it never inspects the contents of a string constant — which is why
`'a literal containing __import__'` evaluates, and why the formatting rule below has to exist. This
one test closes an attribute chain that names no builtin at all: `load_yaml.__globals__` reaches a
live module's namespace without the allowlist ever being consulted.

**The withheld-name rule.** Any `Name` loaded in the expression that is in the withheld set and is
*not* bound by the description's own scope is refused. The scope exception matters: a description
property genuinely named `type` is answered from the scope and not refused, because the rule is about
what the interpreter would supply, not about spelling. This rule is what closes an attribute computed
at run time — `getattr(x, '_' + '_class__')` builds its attribute name out of two string constants,
where no name-based check can possibly see it, and only the absence of `getattr` itself closes it.

The same rule covers the conversion fields of an interpolated string. `f'{x!r}'` and `f'{x!a}'` refuse:
the conversion field is a call to `repr` and to `ascii` respectively, both withheld, and it names no
identifier the walk would otherwise see, so the field is read as that builtin's name. `f'{x!s}'`
evaluates, because the string type it calls is on the allowlist.

That clause reaches the `!r` and `!a` conversion fields and nothing further. The `%` operator's
conversions reach the same two builtins — `'%r' % x` calls `repr` — and are **deliberately not
covered**. Percent formatting performs no attribute traversal: it walks no attributes, and its mapping
form subscripts only a mapping the caller already supplied. There is no reach to close, so the
restriction is not widened to cover it. Widening a restriction to match the shape of an overstated
claim is the wrong repair; the claim is what gets made precise.

**The traversal rule.** `format` and `format_map` are refused outright — as an attribute, and as a bare
name loaded from the scope. The formatting mini-language performs attribute access and subscripting on
text written *inside a string constant*, which the walk deliberately never inspects, so the traversal
is invisible to the identifier test: `'{0.__globals__}'.format(f)` interpolates a function's entire
defining namespace while the tree shows not one dunder identifier. Reaching the method through the
allowlisted string type instead — `str.format('{0.__self__}', len)` — is the same reach and is refused
by the same rule. Because the traversal cannot be seen, the primitive itself is what had to go.

**Why the withheld-name rule cannot be replaced by curating the namespace.** Every allowlisted builtin
carries an attribute leading back to the module that defines it — `len.__self__` *is* the real
`builtins` module — so the reach exists no matter which names are bound. Removing the primitives that
compute or traverse attributes is the only thing that closes it, which is why `getattr`, `type`, `vars`
and `dir` are absent rather than merely unbound.

**Why the check walks a syntax tree.** The obvious alternative is to inspect the name table of a
compiled code object, which is what the compatibility target does. A nested code object escapes it:
`(lambda: __import__('os').getcwd())()` compiles to a top-level object whose name table is empty,
because the lambda body is a code object of its own. The tree walk descends into it and refuses.

## What this costs

Three refusals are deliberately false. You should meet them here rather than in a description that
mysteriously stopped working.

**Every use of the string formatting method refuses, including an innocent one.** `'{}'.format(2)` is
refused under `format-traversal`, because the primitive is refused by spelling and not by what it is
asked to reach. The trade was taken because a xacro description composes text with substitution spans
and concatenation and has no need of the method; the conversion field calling the string type
(`f'{x!s}'`) and a numeric format spec (`f'{v:.3f}'`) both still evaluate, so the formatting a
description actually does is untouched.

**A property named `format` refuses once it enters a composed expression — but not on its own.** This
one has two halves, and reading only the first will mislead you. With
`<xacro:property name="format" value="stl"/>` in the description:

- `${format}` expands to `stl`, with no diagnostic at all. The substitution layer answers a lone
  identifier bound to a string straight from the scope and never calls the evaluator.
- `${'mesh.' + format}` refuses under `format-traversal`. The moment that property joins a composed
  expression the short-circuit no longer applies, the evaluator sees the bare name, and the spelling
  refusal fires.

So the cost is real but narrower than "a property named `format` is broken": it bites at the first
concatenation, not at the reference. The bare-name branch is deliberately not exempted by what the
scope binds — it is a spelling refusal like the identifier test, not a namespace lookup, and making it
conditional would read as a lookup and invite a future exemption.

## Where this diverges from the compatibility target

The reference is xacro's `safe_eval`. Its boundary is treated here as a floor rather than a target, so
the two differ in both directions.

**Tighter.** `safe_eval` empties `__builtins__` but exposes a wide symbol table beside it, including
`type` and `vars` — the two classic roots of an attribute walk back into the interpreter. meios
withholds both, along with `getattr`, `dir`, `globals`, `locals`, `open`, `eval`, `exec`, `compile`,
`input` and `__import__`. The reference also applies its double-underscore test to a compiled code
object's name table, which a nested code object escapes; the tree walk does not.

**Omissions that can bite.** `map` and `filter` are exposed by the reference and are not among the
twenty names above. `${list(map(...))}` does appear in real descriptions, so that is the omission most
likely to be met in practice. Any such name produces a loud `non-allowlisted-builtin` refusal, which
makes it a decision: a name a real description needs gets added to the allowlist deliberately, and is
never passed through silently.

**Parity gaps that predate this restriction.** Dotted access into a loaded configuration is not
supported — the reference wraps loaded yaml so `${cfg.wheel.radius}` works, meios returns plain
mappings, so `${cfg['wheel']['radius']}` is the spelling. The mathematics names carry the same shape of
gap: the reference spreads them into a `math` namespace beside the bare ones, so `${math.pi}` works
there, while meios binds only the bare names and the dotted spelling raises a `NameError` rather than a
refusal. The reference's argument, tokenizing and message helpers (`xacro.arg`, `xacro.tokenize`,
`xacro.message` / `warning` / `error` / `fatal`) are not exposed at all. A unit-tagged yaml value is
read as a literal here, where the reference evaluates it as an expression.

## What this is not

This is a hardening pass. It is **not a sandbox** and it is **not a jail**.

An embedded interpreter evaluating author-supplied expressions inside your process is not a security
boundary you would put across a network, and nothing here proves what a description *cannot* do. What
the restriction does is reduce what a description can reach: the import machinery, the filesystem, the
process, and the attribute graph that leads back to them are not reachable through the supported
subset, and an expression that tries is refused loudly instead of executed. Read that as raising the
cost of a hostile description, not as containment. If you are about to load a description you would
not read first, this restriction is not the thing that makes that safe.

## The resource helper

`load_yaml(spec)` — also spelled `xacro.load_yaml(spec)` — is the only way an expression reaches a
file, and it does not read one itself. C++ resolves the spec, enforces containment and reads the bytes;
the interpreter is handed *text* and parses it. Under the restricted evaluator no `open` exists in any
namespace an expression can reach, so the helper is the only route to a file there; under the
unrestricted evaluator `open` is an ordinary reachable builtin, so containment governs `load_yaml` and
not what an expression reads by other means. An evaluation scope with no resource loader installed
refuses every spec: the capability is absent by default, its absence is a checked refusal, and there is
no fallback read anywhere.

Three spec forms are accepted:

| Form | Resolution | Refused by shape |
|------|------------|------------------|
| `package://<pkg>/<rel>` | `source_stack::locate`, the same resolution a mesh or an include takes | an empty package half, an empty relative half, and a relative half opening on a further separator, under `malformed_asset_uri` |
| `$(find <pkg>)/<rel>` | identical — both spellings split to the same package/relative pair and take one `locate` call | the same three under the same code; this row is the spec form, never the bare `$(find <pkg>)` substitution command |
| anything else | probed against the calling document's directory, then against each configured containment root | — |

A spelling refused by shape is decided before the source stack is asked, so no lookup, no containment
decision and no filesystem call runs for it — the same order the asset grammar keeps.

Containment is judged on the **resolved candidate**, never on the authored spelling. An absolute path
that canonicalizes inside a root is accepted; one that does not is refused under
`uncontained-yaml-path`, and so are a `../` escape and a symlink inside a root whose real target leaves
it. The rule has to be "lands inside a root" rather than "does not look absolute", because the two
substitution entry points hand the resolver different shapes for the same authored form.

**Which document a relative spec resolves against.** The document it is written in — which is not
always the top-level one:

- Inside an included file, that included file. A bare `'inner-config.yaml'` written in a file pulled in
  by `<xacro:include>` resolves next to *that* file, not next to the description that included it.
- Inside a macro body, the document that **invoked** the macro, not the one that defined it. This
  matches the compatibility target and is not obvious from reading the macro: a macro that loads a
  configuration sitting beside its own definition will not find it when called from elsewhere.
- A document served as bytes by a source layer does have a directory: such a layer materializes the
  bytes into a scratch area it owns and hands back the path of the file it wrote, so a relative spec
  written in that document resolves beside it exactly as it would beside a document read off disk.
  The scratch tree lives for as long as the source does. Source kind does not distinguish the two
  package spec forms: `package://<pkg>/<rel>` and `$(find <pkg>)/<rel>` split into the same
  package/relative pair, take the same `locate` call and read the returned file's text.

**Unit tags.** `!radians`, `!degrees`, `!meters`, `!millimeters`, `!foot` and `!inches` are registered
as constructors on PyYAML's `SafeLoader` the first time a configuration is parsed, and stay registered
for the life of the process. The registration is idempotent — re-registering a tag replaces it with an
identical constructor — and it touches `SafeLoader` and no other loader class, so a sibling loader in
your own embedded Python is unaffected.

**Windows over-refuses.** Containment compares canonicalized paths without case folding and without
expanding short (8.3) path names. On Windows a candidate whose spelling differs from its root's in
case, or which arrives in short form, therefore compares unequal to that root and is refused even
though it genuinely sits inside it. This fails closed — it is over-refusal, not a bypass — but it is a
real functional limit on one of the three supported platforms, and it is the first thing to suspect
behind an unexplained `uncontained-yaml-path` there.

## The unrestricted evaluator

`meios::unrestricted_python_evaluator`, declared in `meios/eval/unrestricted_python_evaluator.h`, is a
supported and documented backend that applies none of the expression rules above.
`${__import__('os').getcwd()}` evaluates under it.

It is reachable only from C++. The command-line tool never constructs it and carries no flag for it —
`--eval python` means the restricted class and nothing else — so arbitrary code execution is never one
flag away from an ad-hoc invocation on a file somebody sent you. Choosing it is a compile-time act: an
include line and a construction site, both visible in review.

The two classes share **one** resolution and containment implementation. The unrestricted class refuses
`load_yaml('/etc/passwd')` under the same `uncontained-yaml-path` rule, resolves `package://` the same
way, and reads byte-backed sources the same way. The expression sandbox is the only difference between
them, and what it widens is the expression itself — which is enough to reach the filesystem directly,
`open` included. The helper's resolution and containment are identical under both, so an unrestricted
expression is bounded where it goes through `load_yaml` and unbounded where it does not.

What you accept by choosing it: every expression in every description you load — including ones pulled
in by an include, from a package you did not write — runs with your process's authority. Choose it for
descriptions you would be willing to run as a script.

## What an expansion hands back

Expansion is a result, never a flag beside a document. `meios::expand` returns either an `expansion`
carrying the expanded document and nothing else, or an `expansion_error` carrying the failing
location, a `diagnostic_code`, a message and — where a refused system operation caused it — that
operation's native cause. A terminal failure is reported exactly once through the sink you supplied
and returned on the error arm, so you neither log it again nor risk seeing it twice. Warnings, the
environment-read note and every other nonterminal diagnostic keep travelling that same sink,
unaffected. A span the active policy declines to resolve is a **success** whose span is left
verbatim; [What refuses, and by which rule](#what-refuses-and-by-which-rule) is what decides which
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

## Explicit non-goals

**Resource exhaustion is outside this boundary.** `${10**10**10}` and `${[0]*10**12}` are refused by
nothing: they name no withheld builtin, traverse no attribute and touch no file. They will burn
processor time and memory. The boundary is about *authority* — the filesystem, the network, the
process — and not about *availability*. A real bound needs a per-expression watchdog against an
embedded interpreter holding the interpreter lock, portable across all three supported platforms;
there is none today, and its absence is deliberate rather than overlooked.

**The expression fuzzing does not cover this boundary.** The fuzz corpus is seeded with the refused
expressions from the boundary's own table, but the harness those seeds feed drives the built-in core
evaluator — which carries none of this exposure — and the fuzz targets do not link the Python
enrichment at all. That is a genuine robustness check on a parser that must not crash on
hostile-looking input, and it is not a check of the trust boundary. Nothing currently fuzzes the
boundary. Its proof is the table-driven suite: `tests/golden/expr/abuse.cases` states every expression,
its verdict and the rule that produces it, and `tests/unit/eval_python_abuse_test.cpp` runs the table
through the evaluator seam and through a full document expansion.
