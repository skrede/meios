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
`${'0' + '1'}` binds the integer `1`, exactly as `value="01"` does. A property's value is evaluated
where the property is written, against the bindings above it, where the reference stores the text and
evaluates it when the name is read, so a property whose value fails refuses here even if nothing
reads it, and one naming a property written below it refuses here and renders there;
[known limitations](known-limitations.md#expression-evaluation) records the measurement.

**An argument binds the text, and the site that reads it is where that reading happens.** An
argument — a declared default, or an override a caller hands the load — binds the characters that
were written; the conversion above is not applied to it. Only a default that is itself one whole
expression binds that expression's value, exactly as an attribute of the same shape does, so
`default="${1+1}"` binds the integer `2`. Everything else stays as written, which is what leaves
`$(arg flag)` of a `false`-spelled default rendering `false` into a flattened document rather than
the `False` a boolean spells, and what leaves a `0.10` default its trailing zero. The conversion
happens instead where the argument is read into something that does bind a value:
`<xacro:m flag="$(arg flag)"/>` hands text to the parameter, the parameter binds it by the reading
above, and `${flag}` inside that macro is a boolean. Both implementations keep two tables — an
argument table the `$(arg n)` and `$(eval)` commands read, and a property table an expression span
reads — so an expression span cannot name an argument: `${flag}` over an argument alone refuses, and
the refusal names `$(arg flag)`. A property of an argument's name is a second binding beside the
argument, never a write to it, so a property's `default=` binds over an argument of the same name and
`$(arg n)` answers the argument whatever property is written. `$(eval)` reads each argument it names
through the reference's own conversion rather than the one above — `1e3` stays text there, `1_000` is
the integer `1000`, and a digit is any decimal digit Python 3.14 reads, so `١٢` is `12` — sees the
mathematics names and the spellings `true` and `false` before any argument, sees no property, refuses a
double underscore, and ends at its first closing parenthesis, so its body holds no call;
[known limitations](known-limitations.md#expression-evaluation) records what was measured.

**A macro call's arguments are evaluated against the caller.** All of them are read before any is
bound, so one argument never sees a sibling's new value: a call passing both
`ee_id="${ee_id}_white"` and `inertials="${...ee_id...}"` reads the caller's `ee_id` in the second,
not the one the first just wrote. An inherited `^` default reads the caller too — its properties
and parameters, never an argument, so a bare `^` whose name only an argument binds refuses, naming
`$(arg n)`, and `^|fallback` takes its fallback.

Mathematics is a fixed set of functions, called by bare name and never through a module:

```
abs   acos   asin   atan   atan2   ceil   cos   degrees
floor   max   min   radians   sin   sqrt   tan
```

`min` and `max` take one or more arguments — zero is a fault, not an identity — `atan2` takes exactly
two, and the rest take exactly one. `abs` of an integer is an integer; `floor` and `ceil` yield
integers whatever they are handed. Fourteen of the fifteen names render identically to the reference.
`abs` is the exception and is not a compatible spelling: the reference's expression globals carry the
mathematics module and the two reducers and no builtins at all, so `${abs(-3)}` renders `3` here and
raises an undefined name there. It is a reviewed divergence, and the section on divergences below
carries it. Any other name followed by `(` is refused by that name, so a
description reaching for a function this list does not carry is told which one it reached for rather
than that something went wrong. A name on the list called at the wrong arity is refused the same way,
by name, so `atan2(x)` reports the function rather than the count.

**A span's own text is resolved before its expression is evaluated.** `${...}` and `$(...)` both
expand any span written inside them first, and what the evaluator or the command dispatcher is handed
is the string that produced — so `${xacro.load_yaml('$(find arm_description)/cfg/limits.yaml')}` has
already become a path by the time the expression runs. That is the order the reference resolves a
substitution in, and it is why the spelling a real description writes for a package-qualified
document reads at all. A command ends at its first closing parenthesis, as the reference's pattern
ends it, so one command cannot be written inside another, and a `(` or a `$` left in a command's text
once its spans are resolved refuses, as the reference's command scanner refuses it; only the
`$(eval)` body, which an evaluator reads, may hold either. The argument command takes exactly one
name once its spans are resolved, and refuses a second word whether or not the argument is bound, as
the reference does: `$(arg ${a + b})` reads the argument the sum names, and a span that produces two
words refuses.

All of that nesting is charged against the expression-depth ceiling, and each shape is charged what it
costs in stack rather than one apiece: descending into the grammar is one and a nested scan is three,
weighted from the depth at which each shape was measured to exhaust a 512 KiB thread. So the one
ceiling leaves both shapes the same headroom, and a document nesting
expression spans past it refuses at a stated position rather than running out of stack; commands,
which end at their first closing parenthesis, do not nest at all.

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

**A block argument's contents are evaluated at the call site, on both sides.** A macro's block
argument is expanded once, where the caller wrote it, in the caller's macros and scope; the block
parameters then bind to the element children of that expanded result. This evaluator used to expand a
block only where the receiving macro inserted it, which made an undefined name inside a discarded
block a load failure upstream and nothing at all here. That difference is closed, and the two sides now
agree on the failure as well as on the rendering. The change is visible on descriptions that load: a
side effect written inside a block nobody inserts now takes effect, and a block naming an unbound
property now fails the load whether or not it is reached. [Known limitations](known-limitations.md)
says so in the reader's own terms.

**A block parameter a call leaves without an element renders here and refuses there.** The reference
expands a macro call's children and then refuses the call with "Not enough blocks" for any `*` or `**`
parameter left without an element, so a call holding only an `<xacro:include>` of a document that binds
a property, only a property, or nothing at all refuses there even when the macro never inserts the
block. Here a block parameter binds what the expanded call holds, and only inserting a block nobody
supplied refuses, which it does on both sides. The include shape is a reviewed divergence, so the
comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**A macro call binds in the reference's order, and refuses where the reference refuses.** The reference
binds a call's attributes in document order, then takes the call's expanded children as its block
arguments, then the defaults, and only then refuses a parameter still unbound; the same order is taken
here, all of it in the caller's scope before the macro's body is entered. A property written among a
call's children is therefore not visible to the call's attributes, which refuse it as an undefined name
on both sides, while a default reads it. A default reads a parameter the call supplied too, as the
invocation's own scope holds it, so `b:=${a}` with both supplied renders, and one reading a parameter
the call left out refuses. An attribute naming no declared parameter, a namespace declaration or a
block parameter's name included, refuses with the reference's "Invalid parameter", and an element no
block parameter takes refuses with "Unused block", naming the first left over; a child that expands to
nothing, a property say, takes no block parameter. Where two of these refusals meet in one call, the
one the reference raises first is raised here. Each is reported under `xacro_structural_error` at the
call's own `file:line`, for a macro defined in another file as well; a parameter left unbound is
refused under the same code in this side's own words. A default that refuses is reported at the call's
own `file:line` too, under its own cause, whether or not the call supplied its parameter, for a macro
defined in another file as well. Thirteen `macro_call_` agreement probes hold this against a fresh
upstream render: an undeclared attribute, a namespace declaration and a block parameter's name written
as attributes; one and two surplus elements; an undeclared attribute before a missing parameter, a
refusing block child, an absent block and a refusing attribute, and a surplus element before a missing
parameter; an attribute reading a child's property, a default reading one, and a child that is only a
property. The `macro_default_reads_supplied_parameter` probe holds a default's reading of a supplied
parameter, beside the `xacro_macro_defaults` cases, which pin the omitted one's refusal too. The
`xacro_macro_call` cases pin the location, the included macro among them, and `xacro_macro_call.a
default that refuses while the call supplied its parameter is reported at the call, for a macro defined
in another file` the supplied parameter's refusing default.

**A macro's parameter list reads as the reference's pattern reads it.** The reference splits `params`
with its `re_macro_arg` pattern, and the scanner here follows that pattern rather than splitting at
every space. A default that opens with one `${…}` or `$(…)` span binds that span whole, a space inside
it included, so `a:=${1 + 2} b:=2` binds `a` to `3` and `a:=$(arg y) b:=2` binds it to the argument. A
space may stand on either side of the separator: `a := 1`, `a= 1` and `a :=1` each bind `1`. An empty
default, `a:=`, is no default, so a call omitting `a` refuses as missing it. A parameter whose quote
never closes, `tag:='world`, is read whole as a name with no default, so a call omitting it refuses as
missing `tag:='world`, as the reference refuses it. `a:=^d` takes `a` from the enclosing scope or else
`d`, and `a:=^|` alone takes it from the enclosing scope or refuses. And text after a newline that
follows a parameter is not read, because the pattern's last group stops at a newline. The
`macro_default_empty`, `macro_default_spans_whitespace`, `macro_default_argument_command` and
`macro_param_spaced_separator` agreement probes hold the span, separator and empty shapes against a
fresh upstream render; the `xacro_macro_params` cases pin those, the unclosed quote and the newline;
and the two forward markers are carried by a measurement against the pinned reference alone. A `params`
attribute holding only whitespace refuses under `xacro_structural_error` at the definition once the
expansion reaches it, called or not, as the reference refuses it with "list index out of range", while
one in a definition the expansion never reaches is never read; an empty `params` and a list ending in
whitespace read as before. The `macro_params_only_whitespace` and
`macro_params_only_whitespace_never_reached` agreement probes hold both against a fresh upstream
render, beside the `xacro_macro_defaults` cases. One separator still differs: a space outside ASCII,
which [known limitations](known-limitations.md) records.

**A run of dollars escapes or refuses as the reference's lexer has it.** A run of two or more `$`
before `{` or `(` writes one `$` fewer and the opener, and lexing goes on after it: `$${x}` renders
`${x}`, `$$(arg y)` renders `$(arg y)` and `$$$${x}` renders `$$${x}`. A `$$` before anything else, or
ending the text, refuses with the reference's "invalid expression" under `expression_error`, at the
run's own column and under every evaluation policy, inside an expression body as well, so `a$$x`, `a$$`
and `${'$$'}` each refuse on both sides. In an argument's default it refuses at the declaration,
whether or not the argument is read, unless a caller's argument leaves that default unread. The token
straight after a span is lexed before the span is evaluated, because the reference's lexer reads one
token ahead: `${nope}$$x` refuses with "invalid expression: $$x" on both sides rather than for the
undefined name, and `${nope}${x` refuses as an unterminated span here and with "invalid expression:
${x" there. Exactly one token is looked at, so `${nope}a$$x` refuses for the undefined name on both
sides. A following `${` token ends at its first `}`, as the reference's pattern has it, so
`${nope}${'a}'` and `${nope}${a{b}` refuse for the undefined name on both sides, while a span's own
close stays quote-aware, the difference recorded below. In an include filename the token after a
leading `$(find …)` is lexed before that command's package text is evaluated, and the package is
resolved before the rest of the filename is read, as the reference evaluates the find first:
`$(find ${nope})$$x` refuses with "invalid expression" on both sides, and `$(find nopkg)/${nope}.xacro`
and `$(find )/c.xacro` refuse for the package on both sides, under `unresolved_find` here. A single `$`
before anything but an opener stays text, as `$x` does on both sides. The
`dollar_run_escapes_openers`, `dollar_run_before_text`, `dollar_run_at_end` and
`dollar_run_inside_expression` agreement probes hold this against a fresh upstream render, with
`dollar_run_in_argument_default`, `dollar_run_in_unused_argument_default` and
`dollar_run_in_redeclared_argument_default` for an argument's default,
`dollar_run_after_undefined_name`, `dollar_run_after_unset_argument`,
`dollar_run_after_division_by_zero` and `unterminated_span_after_undefined_name` for the token after a
span, `include_dollar_run_after_find` and `include_unterminated_span_after_find` for the token after an
include's find, and `include_missing_package_before_undefined_name` and `include_find_without_package`
for its package resolved first, beside the `xacro_dollar_run`, `xacro_arg_declaration`,
`xacro_lex_lookahead` and `xacro_include_find` cases and
`arg_scan.scan_args lists no argument a run of dollars escapes`, which keeps an escaped `$(arg …)` out
of the argument listing. `${'$${x}'}` renders `${x}` here and refuses there, which is the closing brace
inside a string literal, recorded below; it adds no difference of its own.

**An argument's default is read at the first declaration the expansion reaches, as the reference reads
it.** A later declaration of the same name binds nothing, so a second default neither replaces the
first nor rescues one that refuses, and a declaration the expansion never reaches, in a branch not
taken or a macro never called, binds nothing where a reached declaration follows it. A declaration in
an included file the expansion reaches first binds before the including document's own, and one in a
macro body binds on the first call. The declaration the expansion reaches reads its default with no
lent value bound, so a `$(arg a)` in it refuses as unset, as the reference refuses it. The
`argument_redeclared_after_refusing_default`, `argument_redeclared_after_expression_default`,
`argument_declared_in_skipped_branch`, `argument_declared_in_uncalled_macro`,
`argument_declared_in_include_first` and `argument_default_reads_itself_before_redeclaration`
agreement probes hold this against a fresh upstream render, beside the `xacro_arg_declaration` cases. What a use above every
reached declaration reads, and what a declaration the expansion never reaches still lends where none is
reached, [known limitations](known-limitations.md) records.

**An include filename is a package lookup only where it begins with `$(find …)`.** That command's
package text is substituted before the lookup, so `$(find ${pk})/urdf/a.xacro` with `pk` bound to `p`
reads the package `p`'s file on both sides. A `$(find` anywhere else is part of a relative spelling:
`$$(find p)/urdf/a.xacro`, whose run of dollars escapes it, and `sub/$(find p)/urdf/a.xacro` each
refuse under `unresolved_include`, as the reference refuses them for want of the file the spelling
names. A `package://` filename's package text is taken as written, so `package://${pk}/…` refuses here,
and the reference refuses every `package://` filename. The `include_escaped_find` agreement probe holds
the escaped spelling against a fresh upstream render, and the three `xacro_dollar_run` include cases
pin the leading find, its substituted package text and the `package://` text taken as written.

**`$(find)` writes a package's directory as its root was configured, except that a relative root is
written absolute and a `.` element dropped.** Through a configured root that is a link the text keeps
the link, `<…>/Rl/p`, as the reference's does, and through a `ROS_PACKAGE_PATH` entry that is a link it
keeps the spelling the crawl found the package at; containment and the read go by the directory the
link leads to. A root is made absolute and its `.` elements dropped when it is configured, so a
relative root, `R` from the directory holding it, renders `<working directory>/R/p` here and `R/p`
there, and `<…>/./R` renders `<…>/R/p` here and `<…>/./R/p` there. Both are reviewed divergences, the
`find_relative_root` and `find_dotted_root` rows, each probe handing both sides the same root spelling
from the same directory, so the comparison fails if either stops reproducing exactly as much as it
fails if a new one appears; the `load_package_anchor`, `ros_source` and `cli_package_path` cases pin
the linked, dotted and relative spellings.

**A relative include resolves against the document that writes it, then through the configured
sources.** The reference joins a relative `<xacro:include>` filename onto the directory of the file
being processed (the including document, and inside a macro body the document that calls the macro), at
the path that file was named by, and consults nothing else. The same anchor is taken here, so a nested
include reads the file beside the include that names it, a document opened through a link anchors at
the link, and a configured root holding a same-named file does not stand in for the file beside the
document. A document a package lookup served, through `package://` or `$(find)` and a directory, bundle
or ROS layer, anchors the same way, at the path the lookup named it by, `root/package/relative`, even
where that path is a link, so its includes and auxiliary documents read the files beside that path, as
the reference reads them through `$(find)`; the file the link leads to stays the document's identity,
as the `load_package_anchor` and `ros_package_link` cases pin. A file beside the document is admitted
where its canonical target lies under the directory the document was named in, under the one it really
lives in, or under a configured package root, which under `meios::load` are
`load_options::package_roots` and for a caller of `meios::expand` the trailing `package_roots` it
passes; with none, only the document's two directories admit. A document opened through a symlinked
install tree therefore reads the siblings that link into the directory it really lives in, and a
sibling linked into a configured root is read through that root. A document a directory, bundle or ROS
layer served is read beside the path it was named by. Only where nothing is there is the sibling asked
of the layer that served the document, and not even then where the spelling holds a `..` element: such
a miss goes to the configured sources alone, so `sub/../c.xacro` in the document, through a linked
`sub` with nothing at `a/c.xacro`, refuses as the reference refuses it. A relative include in a
document a memory source served, a layer that serves bytes, is asked of that layer first, with or
without a `..` element, and nothing beside the file the layer materialized decides it, so it resolves
the same whatever an earlier include made the layer write, and only what the layer lacks goes to the
configured sources. The `load_served_layer` cases pin both, the second by
`load_served_layer.a byte-served document's sibling spelled through a parent element resolves in the
layer that served it whatever an earlier include wrote, and no lower layer's file stands in`. A file
read beside a document a path-naming layer served passes its own misses to that layer only where its
spelling holds no `..` element and the directory the document was named in admitted it, so neither a
file a configured root admitted through a link nor one spelled `sub/../b.xacro` through a linked `sub`
is answered from the layer, and the second refuses its miss as the reference refuses it, as the
`load_served_layer` and `cli_package_path` cases pin. Where neither holds the file, the filename is
then looked for through the configured sources, which the reference does not consult, so a document the
reference refuses for want of that file can render here. That fallback answers a genuine miss only: a
spelling through a path element that is not a directory refuses before any fallback even where a
configured root holds the name, as the reference refuses it, within the platform bound stated below for
the `..` spelling. A document expanded under no name looks for nothing beside it, where the reference's
library entry anchors it at the working directory. A `..` that climbs back inside the directory,
`sub/../x.xacro`, is joined as written and judged where the operating system resolves it, as the
reference reads it, so through a linked `sub` it is read where the link leads if one of those admitting
directories holds that, and refused as an outside link otherwise. Through a path element that is not a
directory, a regular file `f.txt`, `f.txt/x.xacro` refuses on every operating system, because each
element before the last is checked and one that exists and is not a directory refuses;
`f.txt/../x.xacro` refuses where the operating system reports `f.txt` as not a directory, as a POSIX
system does, and on Windows the path interfaces are assumed to reduce `f.txt/..` first and read the
file beside the document, as the reference would there. On Linux both sides refuse both spellings with
"Not a directory", as the `xacro_include_read` and `yaml_resource` cases pin, a case per spelling. The
`..` ones, `xacro_include_read.an include spelled back out of a file that is not a directory refuses on
POSIX and reads the file beside the document on Windows, and no configured source's copy stands in` and
`yaml_resource.a spelling back out of a file that is not a directory refuses on POSIX and reads the
file beside the document on Windows, and no configured root's copy stands in`, assert the assumed
reading on Windows, so a Windows run of the suite decides it; none has run, and it is unmeasured. Two
shapes still differ. Through a `sub` that does not exist the missing tail is reduced by its spelling,
and the file beside the document is read where the reference refuses. And where `sub` is a link and
nothing is at `sub/../x.xacro` as the operating system reads it, the miss is looked for under the
spelling's lexical reduction, `x.xacro`, in the configured sources, so a copy one of them holds can
render where the reference refuses, and a layer that served the document as bytes is asked for that
reduction first, as above; [known limitations](known-limitations.md) gives the measured shapes. Two lookups the reference
makes are refused here by default: a filename climbing above that directory with `..` refuses as
climbing above the directory of the document that writes it, and a link beside the document whose
target lies under none of the directories that admit it refuses under `uncontained_asset`. The
reference reads both. Each is a reviewed divergence, so the comparison fails if it stops reproducing
exactly as much as it fails if a new one appears.

**Two caller options lift those two refusals, and both are off by default.** `meios::include_options`
carries `climb_above_document`, which admits a relative include whose spelling leaves its document's
directory, joined as written and read where the operating system resolves it. For a document served by
a layer whose type names no paths, a memory source, such a climb is answered by that layer inside the
document's package and refused under `xacro_structural_error` out of it; a document a directory, bundle
or ROS layer served reads the climb where the operating system resolves it, through a beside link into
another configured root as well, as the `load_served_layer` cases pin for a directory and a bundle
source and [known limitations](known-limitations.md) measures for the ROS layer. Which of the two
applies is read from the layer's type, and the layer is asked nothing to decide it. It carries
`follow_outside_links` too, which takes a link beside the document wherever it leads, for an include,
an auxiliary document and a mesh alike. With it set, any link on the spelling's way makes the followed
answer final, a link that stays inside the document's directory included: where nothing is at its
target, a dangling link included, or an auxiliary document's target is a directory, the load refuses,
and no configured root's same-named copy stands in, which the `load_beside_root` cases pin for a miss
through a link that stays inside, where with the option unset the root's copy renders. With the option
unset, a dangling link is an absent file beside the document and falls through to the lookups after a
miss, as [known limitations](known-limitations.md) states. A lexical climb-out and an absolute path
landing outside stay refused with the option set. With one set, the document its refusal stopped
renders as the reference renders it, and the comparison holds each opted-in document to a fresh
upstream render beside the default refusal of the same document. Both are `false` on
`load_options::includes`, on every `meios::expand` call that does not pass them, and on
`parse_context::includes` wherever a caller building that context for the parser leaves it out. Only
the calling program sets them: a description cannot, and the `meios` command line exposes neither.
Setting one lets a description read any file the process can open, except that a climb in a document a
layer naming no paths served stays inside that document's package, so it is a decision about how far
the description is trusted, not about parity.

**An absolute include filename refuses here and is read there.** The reference opens an include
written as an absolute path where it points. Here the leading separator is dropped and the rest is
looked for only through the configured sources, so an include stays inside the roots the caller
configured, and where none of them holds it the load refuses. This is a reviewed divergence, so the
comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**A `package://` include filename resolves here and is refused there.** The reference does not read
`package://` in an include filename as a package reference: it joins the spelling onto the directory
of the including document as a relative path and refuses where nothing is there. Here the spelling is
split into a package and a path within it and resolved through the configured sources, as a
package-qualified include. `$(find <pkg>)/...` is the spelling both read, so it is the one to write in
a description meant to load under both. This is a reviewed divergence, the `include_package_filename`
row, so the comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**`$(dirname)` renders here and refuses there.** The reference never hands its directory command a
file and refuses it wherever it is written. Here it renders the directory of the file it is written
in: at the top, the parent of the document path as the load was handed it, and inside an include the
canonical directory of the file the include located, the same from any working directory. This is a
reviewed divergence, so the comparison fails if it stops reproducing exactly as much as it fails if a
new one appears.

**The absolute value renders here and refuses there.** `${abs(-3)}` is `3` here; upstream raises an
undefined name, because its expression globals are the mathematics module and the two reducers with no
builtins beside them. It is the one difference on this page that runs in that direction for a reason
belonging to the evaluator rather than to the reference's span scanner, and it is a reviewed
divergence, so the comparison fails if it stops reproducing exactly as much as it fails if a new one
appears. The function is documented, implemented and range-checked here and a pinned description
already reaches it, so it is recorded rather than removed.

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
/ `error` / `fatal`) are not exposed at all: a call to any of the reference's helpers but
`xacro.load_yaml` refuses under either evaluator, and the refusal names `xacro.load_yaml` as the only
helper either exposes. The argument helper is a reviewed divergence, since it renders upstream; a
refusal of it also names the `$(arg)` command. `map` and `filter` are not among the functions above,
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
- **A second top-level document the same Universal Robots package ships**, driven with a
  distinguishing name as well as the variant. It reaches the keyword-argument mapping constructor
  through a macro-parameter default its own call overrides, and declares a property fallback where
  the main entry point declares a value.
- **KUKA LBR Med 14 R820 at tag `v2.5.0`** (archive digest `edb596d3e2b7…`), whose macro reads a
  joint-limits document through `xacro.load_yaml` and computes every joint's limits out of it.
- **Franka Robotics' description at tag `2.8.1`** (archive digest `4adcc45f83fd…`), through six of
  the eight entry points it ships, each paired by name with its own measured record. It is the only
  measured description that reads a loaded mapping by a dotted member name, and the only one that
  passes such a mapping through a macro parameter. All eight were rendered and compared: no two are
  alike, so none of the six stands in for another, and the two that are absent build their arm list
  with a bracket literal and then slice it — neither spelling is read here.
- **The Kinova `gen3` arm at its seven-joint variant**, whose initial-position mapping is a
  macro-parameter default the caller overrides — which is why it exercises the mapping constructor at
  all.
- **A two-file gantry written in this repository rather than pinned from a vendor**, because no
  surveyed vendor writes a merge key into a file a description loads. Its joint-limit table factors a
  repeated block behind an anchor and writes each axis's range and vector as a short sequence, so one
  document witnesses a merge key, a document sequence and an index counted back from the end. It is
  pinned by a digest of its two files, recomputed at configure, so an edit to either fails the build
  rather than silently invalidating a measurement.
- **Twenty-three minimized expression cases** — one for each non-trivial expression form in the
  closure of the Universal Robots document: the joint-limit arithmetic, the inertia arithmetic, the
  string comparison, the membership test, the subscript chain, `pi`, and the four auxiliary-document
  loads. Each is driven through a document seeding exactly the names it reads, so a failing case
  names one form.
- **One hundred and four authored expression cases beside them** — the spellings no pinned
  description reaches, named here and driven through the same minimized document: an index into a
  sequence, forwards and backwards and off both ends, a key the reference's own mapping wrapper
  answers itself, the keyword-argument mapping constructor and every argument shape it refuses, every
  string meaning this grammar admits or refuses, every arithmetic, comparison and logical spelling
  including the chained comparison, the dotted read of a loaded key, and the mathematics names called
  by bare name. What each of them measures is not left to a reader to infer: the grammar's accepted
  forms are enumerated in the test tree, and that enumeration is paired against these rows in both
  directions, so a form no row measures fails the build and a row no form names fails it too.
- **A separate recorded table of tagged scalars** — nineteen rows covering every one of the six unit
  tags, and beside it the kinds a plain scalar resolves to, both recorded from the pinned reader rather
  than read off the YAML specification.
- **Thirty-two minimized binding documents**, three of them driven a second time with a caller
  argument, each compared whole: a property's default and value over an argument of the same name,
  before and after its declaration and from a macro's global write, a macro parameter beside an
  argument, a property default under a parameter, the vendor idiom of a property defaulting to its own
  argument, the reads of an argument other than through `$(arg n)`, and ten over `$(eval)` — an
  argument beside a property of its name, text in a sum and in a conditional, the conversion over ASCII
  and over other decimal digits, the names that shadow an argument, and the four refusals — and seven
  over the other commands: a command written inside one, a parenthesis or a dollar sign left in one's
  text, and an argument name an expression span supplies. Four of them hold the argument command to one
  name: a second word written over an unset and over a bound argument, and a span-built name with and
  without a space. Beside them sit six documents over a block argument the macro never inserts: a
  property, a global write, an emitting call and an include inside it, and an unbound name under each
  kind of block parameter. Four more hold a relative include to the reference's anchor, one of them
  driven a second time with a configured root holding a same-named file: a file beside its document, a
  nested include, an include written in a macro body, and a document opened through a link. Two more
  are loaded under a caller's include options: the climbing document with the climb admitted, and a
  document whose included file includes a link leading out of its directory, with outside links
  followed. Twenty more hold a macro call to the reference's binding: the thirteen `macro_call_`
  documents over undeclared attributes, surplus elements, the order between refusals and a child's
  property, `macro_default_reads_supplied_parameter`, a default reading a parameter the call supplied,
  and six over the parameter list, `macro_default_empty`, `macro_default_spans_whitespace`,
  `macro_default_argument_command`, `macro_param_spaced_separator`, `macro_params_only_whitespace` and
  `macro_params_only_whitespace_never_reached`. Twelve hold a run of dollars to the reference's lexer:
  `dollar_run_escapes_openers`, `dollar_run_before_text`, `dollar_run_at_end`,
  `dollar_run_inside_expression`, `include_escaped_find`, an include whose run of dollars escapes its
  `$(find …)`, the three `dollar_run_in_` documents, a stray run in an argument's default read, unread
  and redeclared, and four over the token after a span, the three `dollar_run_after_` documents and
  `unterminated_span_after_undefined_name`. Four more hold an include filename's leading `$(find …)` to
  the reference's order: `include_dollar_run_after_find` and `include_unterminated_span_after_find`
  over the token after it, and `include_missing_package_before_undefined_name` and
  `include_find_without_package` over its package resolved before the rest of the filename. Six more
  hold an argument's default to the declaration the expansion reaches first: the two
  `argument_redeclared_` documents, `argument_declared_in_skipped_branch`,
  `argument_declared_in_uncalled_macro`, `argument_declared_in_include_first` and
  `argument_default_reads_itself_before_redeclaration`, whose default reads its own name before a
  later declaration. The meios side is what the load path builds, under the caller
  arguments a consumer would hand it, written back out by the serializer the command line flattens
  with; the expansion alone is held to the same render beside it. Two refusals agree only on their
  cause: upstream's own wording and this side's diagnostic code, both named per document, so a fixture
  missing from both sides or a refusal for an unrelated reason fails the run.

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
not claim. All sixteen rows record agreement — on the result and on the whole normalized rendered
document — and that empty divergence column is asserted empty rather than ignored. Two things fall
straight out of the record: exactly one pinned description resolves a duplicate key, and no Universal
Robots or KUKA entry point reads a mapping by a member name.

The comparison against a fresh upstream render runs on Linux, where the pinned upstream tooling is
installed and run — and so does the corpus tier that feeds it. What macOS and Windows fetch the corpus
for is a narrower thing: on each of the three, a consumer outside this project loads two of the pinned
documents — the `ur3e` variant and the KR6 — natively and checks them against the same recorded facts.
So agreement with upstream is established on one platform, and of the pinned documents exactly those
two are established to resolve to the same facts on all three. The rest are established on Linux.

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
| anything else | probed beside the calling document, admitted under the directory it was named in, the one it really lives in or a configured containment root, then probed against each configured containment root | — |

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
hand the resolver different shapes for the same authored form. A resource one root excludes and another
holds the place for is reported as missing rather than as escaping, since the second root answered the
containment question the first one raised. The probe beside the calling document is admitted under the
directory the document was named in, the one it really lives in, or a configured root, by the rule an
include beside its document is held to. That rule holds for a spelling that stays inside the document's
directory as written: an include's leading `..` meets the climb gate first, while an auxiliary
document's is admitted wherever it lands under one of those directories, so it loads under a configured
root holding the file and, with no configured root, refuses where the reference reads it, as the
`load_beside_root` cases pin. A spelling through a path element that is not a directory, `f.txt` a
regular file, refuses as it refuses there, and no configured root's copy stands in: `f.txt/x.yaml` on
every operating system, and `f.txt/../x.yaml` where the operating system reports `f.txt` as not a
directory, with Windows, whose path interfaces are assumed to reduce `f.txt/..` first and read the file
beside the document. The two `yaml_resource` cases, one per spelling, pin it, and the `..` one,
`yaml_resource.a spelling back out of a file that is not a directory refuses on POSIX and reads the
file beside the document on Windows, and no configured root's copy stands in`, asserts the assumed
reading on Windows, so a Windows run of the suite decides it; none has run, and it is unmeasured. A
link beside the document leading under none of those directories refuses under `uncontained_asset` with
no configured root's same-named copy standing in, unless the caller sets
`include_options::follow_outside_links`. A followed link's answer is final: a link to nothing, a
dangling one included, ends the lookup as unreachable, and a link naming a directory refuses under
`cannot_open`, with no configured root's copy read in either place. A `package://` spec naming a link
inside a configured root is read through the root, and one naming a link that leads out of the root
refuses, with an `uncontained_asset` record; a `$(find)` spelling reaches this reader already
substituted to a path and is judged where it lands. [Known limitations](known-limitations.md) gives the
measured shapes.

**Which document a relative spec resolves against.** The document it is written in — which is not
always the top-level one:

- Inside an included file, that included file. A spec whose text is a bare `inner-config.yaml`, bound
  in a file pulled in by `<xacro:include>`, resolves next to *that* file, not next to the description
  that included it.
- Inside a macro body, the document that **invoked** the macro, not the one that defined it. This
  matches the compatibility target and is not obvious from reading the macro: a macro that loads a
  configuration sitting beside its own definition will not find it when called from elsewhere.
- A document served as bytes by a source layer does have a directory: such a layer materializes the
  bytes into a scratch area it owns and hands back the path of the file it wrote, and a relative spec
  written in that document is probed beside that path. The layer writes an entry into that area only
  the first time the entry is looked up, and this reader does not ask the layer, so a sibling the
  layer holds but has not yet written is not found: measured on a layer holding `a.xacro` and
  `c.yaml` side by side, a relative `c.yaml` in the served `a.xacro` refuses as unreachable until
  `c.yaml` has itself been looked up, and resolves after. The scratch tree lives for as long as the
  source does. Source kind does not distinguish the two package spec forms: they split into the same
  package/relative pair and read the returned file's text.

A relative `<xacro:include>` filename follows the anchor of the document that writes it, at the path
that document was named by, and where nothing is there it falls back to the configured sources. A
document a source layer served differs here from the reader above: its relative include is read beside
the path it was named by, or for a memory source beside the file that source materialized, and, where
nothing is there, asked of the layer that served it, so it resolves whether or not that layer has
written the sibling yet.

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
vocabulary of fifteen, six the auxiliary-document reader marks and nine the expression evaluator
marks, each at the point the construct actually resolves rather than declared anywhere, and read
through `exercised`. Caller arguments handed to `expand` directly are bound with
`eval_scope::bind_argument`, where `$(arg n)` reads them; `set` binds a property, which an expression
reads. A spelling outside the vocabulary resolves to nothing, and nothing is charged
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
