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
integers and reals, the addition of two strings, comparison, boolean logic, membership against a
mapping's keys and against a string's text, subscripting into a mapping and into a sequence, one
named string operation, a keyword-argument mapping constructor, a dotted read of a key in a document
this evaluator loaded, a fixed set of mathematics functions, and one namespaced call that reads an
auxiliary document. A construct
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
- Membership, against two kinds: `key in mapping`, where the key reads as text, and
  `'x' in text`, which tests substring containment.
- Subscripting a mapping, including a chain of them: `${joint_limits['A1']['lower']}`. A sequence
  takes an integer subscript on the same spelling, and a negative one counts back from the end, so
  `${names[0]}` and `${names[-1]}` are its first and last elements.
- `dict(a=1, b=2)`, which builds a mapping the rest of the grammar can read.
- A dotted read of a key in a mapping this evaluator loaded: `${config.joint_limits.wrist_3.effort}`
  reaches what the subscript chain reaches. The rules that gate it are
  [below](#what-refuses-and-by-which-rule).
- `xacro.load_yaml(spec)`, where `spec` is any expression yielding text, under the rules in
  [the resource helper](#the-resource-helper).
- Every property, argument and macro parameter the description itself has bound. `True` and `False`
  are the boolean literals, and `pi` answers `3.141592653589793` without the description binding it.

**What a bound value is, before an expression ever reads it.** A property and a macro parameter each
bind a *value*, not the text the author wrote. The text is read as an integer, then as a real, then
as one of `true`, `True`, `false` and `False`, and it stays a string only if none of those fit — the
order is what leaves `1` an integer rather than a true, and `yes` and `no` strings rather than
booleans. So `<xacro:property name="hand" value="false"/>`
followed by `${'' if hand else prefix}` takes the branch a reader expects; a non-empty string would
take the other one. The same reading is applied to whatever an expression evaluates to, so
`${'0' + '1'}` binds the integer `1`, exactly as `value="01"` does.

**An argument binds the text, and the site that reads it is where that reading happens.** An
argument — a declared default, or an override a caller hands the load — binds the characters that
were written; the conversion above is not applied to it. Only a default that is itself one whole
expression binds that expression's value, exactly as an attribute of the same shape does, so
`default="${1+1}"` binds the integer `2`. Everything else stays as written, which is what leaves
`$(arg flag)` of a `false`-spelled default rendering `false` into a flattened document rather than
the `False` a boolean spells, and what leaves a `0.10` default its trailing zero. The conversion
happens instead where the argument is read into something that does bind a value:
`<xacro:m flag="$(arg flag)"/>` hands text to the parameter, the parameter binds it by the reading
above, and `${flag}` inside that macro is a boolean. The reference draws the same line, keeping two
tables where this evaluator has one — an argument table its `$(arg n)` command reads, and a property
table its expressions read. That one table is why an expression here can name an argument at all,
and what it names is text; [known limitations](known-limitations.md) records that read.

**A macro call's arguments are evaluated against the caller.** All of them are read before any is
bound, so one argument never sees a sibling's new value: a call passing both
`ee_id="${ee_id}_white"` and `inertials="${...ee_id...}"` reads the caller's `ee_id` in the second,
not the one the first just wrote. An inherited `^` default reads the caller too.

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

**A span's own text is resolved before its expression is evaluated.** `${...}` and `$(...)` both
expand any span written inside them first, and what the evaluator or the command dispatcher is handed
is the string that produced — so `${xacro.load_yaml('$(find arm_description)/cfg/limits.yaml')}` has
already become a path by the time the expression runs. That is the order the reference resolves a
substitution in, and it is why the spelling a real description writes for a package-qualified
document reads at all. An argument command's default is the one exception: `$(arg name default)` is
dispatched unscanned, so a default the bound argument never uses is never evaluated — and when it is
used, it resolves through the enclosing substitution rather than beside it, so it is charged to the
same load.

All of that nesting is charged against the expression-depth ceiling, and each shape is charged what it
costs in stack rather than one apiece: descending into the grammar is one, a nested scan is three and a
nested substitution is eleven, weighted from the depth at which each shape was measured to exhaust a
512 KiB thread. So the one ceiling leaves every shape the same headroom, and a document nesting spans
past it — through a `${}`, through a `$(…)`, or through an argument default that names an argument of
its own — refuses at a stated position rather than running out of stack.

**The kinds a value can have.** An expression works over exactly seven: a null, a boolean, an
integer, a real, a string, a sequence and a mapping. The first five have a scalar spelling and can be
written into a document; a sequence and a mapping have none, so one reaching a document's text is a
located, typed failure naming the kind rather than an invented serialization. Those last two reach an
expression two ways: out of an auxiliary document, and out of the grammar itself — the mapping
constructor builds a mapping and the named split builds a sequence. Which of the two an expression is
holding still matters, because only a document's mapping answers a dotted member read. A collection
the grammar builds is charged one element before each of its elements is built, so a produced
collection cannot outgrow a loaded one.

**A string is text, and the operations on it are the ones a real description evaluates.** String
literals evaluate, compare for equality against another string, and serve as mapping keys. Two
strings add to their concatenation, a string's truth value is its emptiness, `'x' in text` tests
substring containment, and `text.split('<separator>')` yields the fields between separators — an
empty field wherever two separators meet and at either end — which the subscript layer then indexes.
Every other meaning a string carries in Python stays out and refuses: repetition by an integer,
ordering one string against another, and every member spelling but the one named split. A string
added to any other kind refuses the way the reference's own type error refuses it, because coercing a
string to a number is how a description quietly means something other than what it says.

The four admitted meanings compare and count by bytes where the reference counts code points. The two
agree over the ASCII text every measured description carries; whether any of them carries non-ASCII
text through containment, a split or a concatenation is unmeasured, so the difference is recorded
here rather than claimed absent.

## What refuses, and by which rule

A refused expression is never evaluated. Which refusals the active `eval_policy` may soften is
decided by the failure's kind, and there are exactly four:

| Kind | What produces it | Terminal |
|------|------------------|----------|
| `unsupported` | a construct outside the grammar that a fuller backend would evaluate — a comprehension, an f-string, a member of a string other than the named split, a Python constructor, a dotted read of anything but a loaded mapping's key, arithmetic on a string beyond adding two of them | no: a lenient policy may leave the span verbatim |
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

**A dotted spelling reads a loaded mapping and nothing else.** `xacro.load_yaml` is the one
namespaced call the grammar carries; every other namespaced spelling, `math.pi` among them, is
refused by name. A dot after a value is a member read, and it resolves only for a value that came
out of a loaded auxiliary document: `${config.joint_limits.wrist_3.effort}` reads the same key
`${config['joint_limits']['wrist_3']['effort']}` reads, chains with a subscript in either order, and
survives a property, a macro argument and a nested scope. A mapping a description built itself —
what the mapping constructor below returns — has no member path at all, and neither does a loaded
sequence or a loaded scalar. Two member spellings
are refused on a loaded mapping as well: a name the reference's own mapping wrapper answers before
it consults the document (`clear`, `copy`, `fromkeys`, `get`, `items`, `keys`, `pop`, `popitem`,
`setdefault`, `update`, `values`), and any name written with a leading `__`. Both point at the
subscript spelling instead, because a description that means something here other than what it
means upstream is worse than a description that will not load.

**The mapping constructor builds a mapping; every other constructor is refused by name.**
`dict(a=1, b=2)` takes keyword arguments and nothing else, and builds a mapping the grammar can
subscript, test membership on and nest inside another one. A positional argument, a pair-sequence
argument and a mixture of the two each refuse naming what was met instead, and a repeated keyword
name refuses the way the reference refuses it rather than resolving to one of its values. What it
builds is an authored mapping, so it has no member path: `dict(a=1)['a']` reads, `dict(a=1).a` does
not. `list(...)`, `set(...)` and `tuple(...)` are named outright, so the diagnostic says which
constructor was reached for instead of reporting an unrecognized function; a set and a tuple have no
representable kind here at all. Brace-literal and bracket-literal container syntax stays refused —
the reference's own substitution scanner closes the span at the first `}`, so a brace literal never
reaches its interpreter either.

**Subscripting reads a mapping by a text key and a sequence by an integer index; membership tests a
mapping and a string, and refuses a sequence.** `list[0]` and `list[-1]` read, and an index outside
the sequence is named as the index it was rather than folded to an end. What refuses on a subscript
is the key whose kind the container cannot take: text into a sequence, and anything but text into a
mapping — including a mapping key an auxiliary document wrote as a number or a boolean, which loads
but which no subscript spelling reaches. `x in list` refuses too, and the refusals are not the same
kind: `x in list` is `unsupported`, so a lenient policy may leave the span verbatim, while a
wrong-kind subscript key is an `error` and terminal under every policy.

**Eleven ceilings bound the evaluator.** They are the auxiliary document's node count (100 000),
nesting depth (64) and alias expansion (1 000 000); the bytes produced (8 000 000), tokens lexed
(1 000 000), evaluation steps (10 000 000) and elements built into a collection the grammar
produces (100 000) over the whole load; and, for any one expression, its nesting depth (256), its
token count (10 000), the magnitude of an exponentiation's operands (1 000 000 000 000) and the
length in bytes of a string it produces (1 000 000). Six of them accumulate across every expression
in one load rather than resetting per expression, so a document cannot spend a bounded budget an
unbounded number of times; the other five are high-water marks instead, because what they bound is
the most any one expression reaches and not how often it is reached. A ceiling requested as zero is
refused and takes its default, so there is no spelling that means unbounded. Crossing one is
terminal and stops the load at the position that crossed it: a load continuing past its own ceiling
would report a partial answer as a whole one.

Expansion carries three more that are not the evaluator's: a work count (1 000 000) over nodes
visited, macro instantiations and substitutions, an emitted-node count (100 000), and the depth of
the walk itself (128). The first two bound a shallow-but-wide macro fan-out that no expression
ceiling would catch; the third bounds the opposite shape, a macro that instantiates itself or a
document nested without end, which takes the native stack some five hundred levels in and long
before either volume count accrues — so its number is set from the depth at which that stack was
measured to give out rather than chosen. All three report under the same
`expansion_budget_exceeded` code — so a crossed ceiling naming a work, output-node or
expansion-depth limit is one of these three rather than one of the eleven. These three do not take
the zero rule the eleven do: a zero there is a ceiling already crossed, not an absent one. None of
the fourteen is reachable through `load()`; that entry point always runs them at their defaults,
and only the `meios::xacro` seam takes different ones.

## What this costs

The refusals above are correct for the grammar and still cost a real description something. You
should meet them here rather than in a document that mysteriously stopped loading.

**A filename composed from a string and a number refuses.** `${'mesh.' + suffix}` reads when
`suffix` is text and refuses when it is a number, because deciding what `'2' + 2` means is a place a
description can mean something other than what it says. Text is also composed by writing it — a
substitution span beside literal characters, `filename="meshes/${name}.stl"`, is not an expression
and is unaffected.

**A string carries one named operation, not a method surface.** `text.split('/')` reads;
`text.upper()`, `text.strip()`, `text.split()` with no separator and `text.split(' ', 1)` with a
count all refuse, and no member spelling on a string yields a value that can be called. A description
needing any of them needs the explicit backend.

**`xacro.load_yaml` reads one argument, and it must yield text.** The argument is an ordinary
expression, so a bound name, a quoted literal and a composition of the two all name a document:
`${xacro.load_yaml('config/' + variant + '.yaml')}` reads. What costs you something is an argument
yielding anything else — the diagnostic names the kind it produced rather than probing for a
filename, so a number or a collection reaching the call says so at the call instead of composing a
path nobody wrote.

**Six unit tags convert, and only over a numeric literal.** `!radians`, `!degrees`, `!meters`,
`!millimeters`, `!foot` and `!inches` each multiply their tagged value by the reference's own
constant, so `!degrees 90` reads as `1.5707963267948966` and `!inches 12` as `0.30479999999999996`.
The reference evaluates a tagged scalar's text as an expression; here the character set admits a
decimal literal and nothing else, so a tag written over a name, a call or a hexadecimal literal
declines rather than being approximated. That decline is softenable — a lenient policy leaves the
span verbatim — while a tag outside the six is a fault under every policy, named in the diagnostic,
because a conversion this table does not carry has no constant to apply.

## Where this diverges from the compatibility target

The reference is xacro's own evaluation of a `${}` body through Python. The two differ in both
directions, and every difference below is measured against a pinned upstream rather than reasoned
about from its source.

**Tighter, by construction rather than by restriction.** The reference hands an expression to an
interpreter alongside a symbol table; there is no interpreter here and no table to widen.
Comprehensions, generator expressions, lambdas, f-strings, every string method but the one named
split, the sequence and set constructors, the object protocol and the import machinery are not
restricted — they are absent from the grammar, and an expression reaching for one is refused with a
located diagnostic. The one thing a dot can mean here is a key of a mapping this evaluator loaded;
it reaches no attribute of anything. Nothing in the grammar can name
the filesystem, the network or the process: the one route to a file is the resource helper, where C++
resolves the spec, enforces containment and reads the bytes.

**`and` and `or` yield a boolean, not the deciding operand.** `${1 or 2}` renders `True` here and `1`
upstream; `${2 and 3}` renders `True` here and `3`. This is measured and deliberately unchanged — the
grammar's logical operators produce booleans, and the measured descriptions use them in conditions
rather than for their operand. It is recorded as a reviewed divergence, which means the comparison
fails if it stops reproducing exactly as much as it fails if a new one appears.

**The mapping constructor takes keyword arguments where the reference takes more.** The reference
also builds a mapping from a sequence of pairs and from a mixture of a positional argument and
keyword ones; `${dict([('a', 1)])}` renders there and refuses here. Only the keyword spelling was
measured and admitted, so the wider shapes refuse rather than taking a meaning nothing measured.
Both are recorded as reviewed divergences, which means the comparison fails if either stops
reproducing exactly as much as it fails if a new one appears.

**A member name the reference's mapping wrapper answers itself is refused, not answered.** The
reference hands a loaded mapping back inside a wrapper that tries ordinary attribute lookup before
falling through to the document, so `${config.keys}` renders a bound method there — text carrying
the object's own address — while `${config['keys']}` renders the document's value. The eleven
colliding names are measured into a committed record and the refusing set is checked against it
rather than written down. Here the attribute spelling refuses and names the subscript spelling; the
subscript spelling agrees with the reference and is compared on every push. This one is recorded as
a reviewed divergence in prose rather than in the manifest the comparison reads, because the text
the reference produces embeds an address and does not reproduce between runs.

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

**A block argument's contents are evaluated at the call site there and at the insertion site here.**
Where a macro receives a block and then drops it, the reference has already evaluated every expression
inside that block; here nothing inside it is evaluated at all. The rendered documents agree, because
the work is discarded on both sides, and the corpus comparison covers a pinned description that takes
exactly this path. What does not agree is the failure: an undefined name inside a discarded block is a
load failure there and passes unnoticed here. This one is measured but not yet in the reviewed manifest,
and [known limitations](known-limitations.md) is where a reader meets it.

**A self-referential alias graph loads there and refuses here.** A document whose anchor contains an
alias to itself — `a: &x [1, *x]` — reads upstream as a value that contains itself, because its reader
builds a mutable node and fills it in afterwards. A value here is immutable once constructed and its
collection storage is shared by copying, which cannot denote a cycle at all: the alias arrives while
its own anchor is still incomplete, and that is where it refuses. This is a reviewed divergence, so
the comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**Two string meanings and two mapping-constructor shapes refuse here and render there.** A string
repeated by an integer and one string ordered against another both render upstream; only the addition
of two strings and equality between two were measured into this grammar, so the multiplication
operator and the ordering operators refuse a string operand rather than taking a meaning nothing
measured. The same rule governs the split: the no-argument form collapses runs of whitespace and
drops the leading and trailing fields, which is a different algorithm from the explicit-separator form
admitted here, and a second argument bounding how many separators are consumed is a second parameter
this one operation does not take. All four are reviewed divergences, and so are the two wider mapping
constructor shapes above.

**The span scanner is quote-aware and counts nesting where the reference's is neither.** The
reference finds a `${}` span's end with a pattern that stops at the first `}` wherever it stands, and
three spellings differ because of it. `${'a}b'}` renders `a}b` here and refuses there as an
unterminated string literal, because a brace inside a literal is that literal's text here.
`${${'1 + 2'}}` renders `3` here and refuses there, because the close finder here counts depth and
the inner scan then resolves the nested span before the outer expression runs. And
`${'%.3f' % 1.2345}` renders `1.234` there and refuses here, because only arithmetic remainder was
measured into this grammar and a string operand takes no formatting meaning. All three are recorded
as reviewed divergences, so the comparison fails if one stops reproducing exactly as much as it fails
if a new one appears. Two neighboring spellings are *not* divergences, though they refuse for
different reasons on each side: `${ {'a': 1} }` and `${f'{v:.3f}'}` refuse on both, there because the
truncated fragment will not parse and here because neither a brace literal nor a format literal is in
the grammar.

**Parity gaps that predate this grammar.** The mathematics names are also reachable through a `math`
namespace upstream, so `${math.pi}` works there and is refused here as an unsupported call target. The reference's
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
- **Franka Robotics' description at tag `2.8.1`** (archive digest `4adcc45f83fd…`), through six of
  the eight entry points it ships, each paired by name with its own measured record. It is the only
  measured description that reads a loaded mapping by a dotted member name, and the only one that
  passes such a mapping through a macro parameter. All eight were rendered and compared: no two are
  alike, so none of the six stands in for another, and the two that are absent build their arm list
  with a bracket literal and then slice it — neither spelling is read here.
- **Twenty-three minimized expression cases** — one for each non-trivial expression form in the
  closure of the Universal Robots document: the joint-limit arithmetic, the inertia arithmetic, the
  string comparison, the membership test, the subscript chain, `pi`, and the four auxiliary-document
  loads. Each is driven through a document seeding exactly the names it reads, so a failing case
  names one form.
- **Fifty-one authored expression cases beside them** — the spellings no pinned description reaches,
  named here and driven through the same minimized document: an index into a sequence, forwards and
  backwards and off both ends, a key the reference's own mapping wrapper answers itself, the
  keyword-argument mapping constructor and every argument shape it refuses, and every string meaning
  this grammar admits or refuses. A chained comparison and most of the mathematics names are still
  written by no measured description and are measured against upstream by nothing here.
- **A separate recorded table of tagged scalars** — nineteen rows covering every one of the six unit
  tags, and beside it the kinds a plain scalar resolves to, both recorded from the pinned reader rather
  than read off the YAML specification.

Every comparison the run owes is named in a committed inventory, and a named comparison that produces
no verdict fails the run before any pass or fail count is reported — a gate cannot go quiet by
matching nothing. Each known divergence is listed in a reviewed manifest by its exact text, in both
directions: an unlisted divergence fails the run, and a listed one that no longer reproduces fails it
too.

**What each pinned entry point is, and what it exercises, is recorded rather than described.** A third
committed record carries one row per pinned entry point: the document, its immutable source revision,
the arguments it needs, what upstream produced, what meios produced, which constructs the load
actually exercised, and any reviewed divergence. The construct column is compared against an
observation taken from loading that document, in both directions, so a description that quietly stops
exercising what it was pinned for fails as loudly as one that starts exercising something the row does
not claim. All thirteen rows record agreement — on the result and on the whole normalized rendered
document — and that empty divergence column is asserted empty rather than ignored. Two things fall
straight out of the record: exactly one pinned description resolves a duplicate key, and no Universal
Robots or KUKA entry point reads a mapping by a member name.

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
authority to withdraw: the grammar has no import machinery, no object protocol, no `open` and no way
to name one — a dot after a value reaches a loaded document's key and nothing else. Those are absent
rather than withheld, so there is no denylist to keep current as a future language grows a name, and
no reach to close. The one thing an expression touches outside
itself is an auxiliary text document, and C++ resolves, contains and reads that before the evaluator
sees a byte of it.

That does not make a hostile description harmless, and nothing here should be read as containment. A
description you did not write still chooses which files the helper is asked for, what arithmetic
runs, and how deep and how long it runs for. Containment is what bounds the first of those and the
ceilings above are what bound the last, and neither is a reason to load a description you would not
read first.

## The resource helper

`xacro.load_yaml(spec)` is the only way an expression reaches a file, and it does not read one
itself: `spec` is any expression yielding text and that text is the specification, C++ resolves it,
enforces containment and reads the bytes, and the parser is handed *text*. Widening the argument from
a bare name to an expression added no resolution of its own — whatever the expression composes still
arrives at exactly the one loader below. An
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
a property, passes as a macro argument, and is read by subscript. A mapping also answers a dotted
read, so `${config.joint_limits}` and `${config['joint_limits']}` reach the same key; a sequence is
read by index only, counting back from the end for a negative one, so `${links[-1]}` is the last
element of a `links:` list. Both spellings of a name the reference's mapping wrapper answers itself
are covered by the dotted rule above.

**What a document may say, and how it reads.** An anchor and the alias that names it denote the same
subtree, so a configuration that writes one inertia and reuses it reads the same both places. A merge
key flattens the mapping it names into the one that carries it, and a key the carrying mapping also
writes wins over the merged one. A key written twice resolves to the first position and the last
value, which is a rule a real robot description reaches: one auxiliary document in the measured
corpus writes a key twice. And a key need not be text — a document may write a number, a boolean or
an empty key, and it loads. Two such keys are the same key when the reference's own comparison would
say so, which means the numeric kinds compare by value across their whole range (`1` and `true` are
one key) while text and a null compare only within their own kind (`'1'` and `1` are two). What a
non-text key does *not* have is a way to be read: every subscript key is text, so such an entry loads,
counts and can collide, and no expression reaches it.

**What crosses a property boundary.** An attribute that is exactly one `${…}` binds the value that
span evaluated to, with its kind and its origin intact, so a mapping or a sequence crosses a
`xacro:property`, a macro argument and a nested scope without being rendered and re-read. The third
property out reads exactly as the first does — dotted reads included — and there is no hop limit to
exceed. An attribute mixing a span with literal text is the other path, and it renders: a collection
has no scalar spelling, so a collection reaching mixed text is a typed failure naming the kind rather
than an invented serialization.

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
answered. A container the grammar itself built — a `dict(…)` or the sequence a split yields — crosses
a property boundary the same way a loaded one does and reads the same on the other side, with one
difference: it never gains a member path. `${dict(a=1)['a']}` reads after any number of hops, and
`${dict(a=1).a}` refuses after none, because what the dotted rule gates on is which document a value
came out of and an authored container came out of none.

**A control character an author writes is erased.** Under the opt-in backend a container does cross a
property boundary as text, delimited by two control characters the XML 1.0 charset clause forbids
outright, so neither can appear in a legitimate document. An author who writes one anyway — as a raw
byte, or as the
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
carrying the expanded document beside a record of which constructs that one load exercised — a closed
vocabulary of fourteen, six the auxiliary-document reader marks and eight the expression evaluator
marks, each at the point the construct actually resolves rather than declared anywhere, and read
through `exercised`. A spelling outside the vocabulary resolves to nothing, and nothing is charged
against the record: it is an observation, not a twelfth ceiling. The other arm is an
`expansion_error` carrying the failing location, a `diagnostic_code`, a message and — where a
refused system operation caused it — that operation's native cause. A terminal failure is reported
exactly once through the sink you supplied and returned on the error arm, so you neither log it
again nor risk seeing it twice. Warnings, the environment-read note and every other nonterminal
diagnostic keep travelling that same sink, unaffected. A span the active policy declines to resolve
is a **success** whose span is left verbatim;
[what refuses, and by which rule](#what-refuses-and-by-which-rule) is what decides which failures
are terminal at all. `meios::substitute` publishes the same two arms over the same record.

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
Python behavior the grammar above does not carry — a comprehension, an f-string, a string method
other than the named split, a method call on a loaded mapping, string repetition or ordering, or a
unit tag written over something other than a numeric literal: it registers the same six tags and
evaluates the tagged text as an expression, the way the reference does. It drives a *found* (never
fetched) CPython. Its default
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

Resource exhaustion is unbounded under this backend, where the built-in evaluator's eleven ceilings do
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
not how long that work takes, and none of the eleven adapts to the machine it runs on. There is no
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
