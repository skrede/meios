# Evaluation: what a description's expressions may run

A xacro description carries executable expressions, and this page is the contract for what meios will
and will not run when it evaluates them. It is the one place that owns the boundary: the supported
grammar, the rules that refuse everything else, the ceilings that bound one load, where those rules
differ from xacro, exactly what the running evidence for that comparison covers, how the resource
helper reaches a file, what a loaded configuration is readable by, and the explicit backend for a
description that needs more than the grammar carries.

The evaluator that runs is compiled into the library. It needs no interpreter, no Python installation
and no Python packages; nothing about it is opt-in. `meios::yaml`, which is what lets an expression
read an auxiliary document, is built by default too, so the plain `load()` call in
[getting started](getting-started.md) resolves a description whose macros read a joint-limits file
with nothing installed beyond a compiler and CMake. The two libraries it needs, pugixml and yaml-cpp,
are each found on the system or fetched at configure time.

What it evaluates is a fixed, closed grammar rather than a language: arithmetic that is exact over
integers of any size up to 4300 digits and stays finite over reals, the addition of two strings,
comparison, boolean logic, membership against a mapping's keys and against a string's text,
subscripting into a mapping and into a sequence, one named string operation, a keyword-argument
mapping constructor, a dotted read of a name an include namespace holds or of a key in a document
this evaluator loaded, a fixed set of mathematics functions, and one namespaced call that reads an
auxiliary document. A construct outside that grammar
is refused with a located diagnostic naming what it met — never evaluated, and never quietly
approximated. `meios::eval-python` is a separate, opt-in backend for a trusted
description needing Python behavior the grammar does not carry; it has
[its own section](#meioseval-python-an-explicit-backend) near the end.

## What evaluates

- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `**`, and unary `+`/`-`. Integer arithmetic stays
  integer — `2 + 3` is `5`, not `5.0` — while `/` is always true division, so `4 / 2` is `2.0`.
  Every operation computes exactly on integers of any size up to 4300 digits, a result past 64 bits
  included, so `${2 ** 70}` is `1180591620717411303424`. A result with more digits crosses the integer
  ceiling, a bound of meios's own that halts the load under `expansion_budget_exceeded`, a power before
  it is computed; xacro computes such an integer, a difference
  [known limitations](known-limitations.md#expression-evaluation) lists. `/` is true division, rounded once, for integers of any size, so
  `${2 ** 70 / 3}` is `3.935305402391371e+20`. `//` and `%` round toward negative infinity, the
  remainder taking the divisor's sign, so `${-7 // 2}` is `-4` and `${7 % -2}` is `-1`. Over reals
  `//` is the whole number of times the divisor fits, counted from the exact remainder, so
  `${1 // 0.1}` is `9.0`: the real written `0.1` is a little more than one tenth. The
  `real_floor_division` agreement probe holds this against a fresh xacro render. A real remainder of
  exactly zero takes the divisor's sign, as every remainder does, so `${-4.0 % 2.0}` is `0.0` and
  `${4.0 % -2.0}` is `-0.0`; the `real_remainder_zero_sign` agreement probe holds this against a
  fresh xacro render. An integer beside a real is
  read as the nearest real, so `${99999999999999999999 * 0.5}` is `5e+19`, and one too large for a
  real refuses, as a quotient too large for a real does. Every real result must be
  finite, and a division by zero and a non-finite result are loud failures rather than a wrapped value
  or an `inf` reaching your document.
- Comparison: `<`, `<=`, `>`, `>=`, `==`, `!=`, chained the way Python chains them, so `0 < x < 1`
  reads as both comparisons rather than as a comparison against a boolean. Comparing two numbers is
  exact, so `${9007199254740993 == 9007199254740992}` is `False`, and an integer wider than 64 bits is
  compared with a real by its value: `${99999999999999999999 < 1e20}` is `True`.
- Boolean logic: `and`, `or`, `not`, short-circuiting — an operand that cannot change the result is
  never evaluated. `or` yields its first true operand, or its last when none is, and `and` its first
  false operand, or its last when none is, without converting it: `${1 or 2}` is `1`, `${0 or 2}` is
  `2`, `${2 and 3}` is `3`, `${0 and 3}` is `0` and `${'' or 'x'}` is `x`, so the idiom
  `${p and p + '_' or ''}` over `p` bound to `ab` reads `ab_`. `not` yields a boolean. A sequence or a
  mapping is true when it holds anything. The logical rows of the expression record hold these
  against xacro, beside the `native_branch_logic` cases.
- The conditional expression, `a if condition else b`. Only the branch taken is evaluated.
- Membership, against two kinds: `key in mapping`, where the key reads as text, and
  `'x' in text`, which tests substring containment.
- Subscripting a mapping, including a chain of them: `${joint_limits['A1']['lower']}`. A sequence
  takes an integer subscript on the same spelling, and a negative one counts back from the end, so
  `${names[0]}` and `${names[-1]}` are its first and last elements.
- `dict(a=1, b=2)`, which builds a mapping the rest of the grammar can read.
- A dotted read of a name an include namespace holds: `${n.w}` after an include written with
  `ns="n"`, falling through to the names of the file that wrote the include and then to the global
  names.
- A dotted read of a key in a mapping this evaluator loaded: `${config.joint_limits.wrist_3.effort}`
  reaches what the subscript chain reaches. The rules that gate it are
  [below](#what-refuses-and-by-which-rule).
- `xacro.load_yaml(spec)`, where `spec` is any expression yielding text, under the rules in
  [the resource helper](#the-resource-helper).
- Every property, argument and macro parameter the description itself has bound. `True` and `False`
  are the boolean literals, and `pi` answers `3.141592653589793` without the description binding it. A
  property may shadow a built-in name such as `pi`, with a warning.

**A name in an expression is read under its NFKC form.** Wherever an expression names something — a
bare name, a member, a namespace member, a call target, a `dict(...)` keyword and a name in `$(eval)` —
the name is read as its NFKC normalization, so `${ﬁ}` reads `fi`, `${ｐｉ}` reads `pi`, and a property
bound as `π` is read by `${π}`. A keyword is recognized on the written text, so `${ｉｆ}` is the
undefined name `if`, and a spelling of `None`, `True` or `False` outside ASCII refuses, `${Ｔrue}` with
`the constant 'True' cannot be spelled outside ASCII`. A character no identifier admits refuses at that
character, naming its code point and never quoting it: `the character U+20AC cannot stand at its
position in a name` for `€`, and `the character U+00A0 cannot stand at its position in a name` for a
no-break space inside an expression. The characters Python's `str.strip()` removes are removed from
both ends of a `${…}` body once the body's own substitutions have resolved. These are the characters
`str.isspace()` holds, among them the no-break space U+00A0 and the ideographic space U+3000, so a
`${p}` written with a no-break space beside the name reads `p`, under either evaluator. The evaluation
command's body is not stripped, so the same character at either end of `$(eval …)` refuses. A name is
stored as it is written, whether a property's, a macro parameter's or an argument's, so a macro
parameter written in decomposed form is not read by its composed spelling. The
`property_name_non_ascii_read`, `expression_reads_normalized_name`,
`expression_normalized_keyword_is_a_name`, `expression_invalid_character` and
`expression_unicode_space_at_edge` agreement probes hold these against a fresh xacro render, beside the
`native_identifier_read`, `native_identifier_fault` and `native_expression_space` cases.

**What a bound value is, before an expression ever reads it.** A property and a macro parameter each
bind a *value*, not the text the author wrote, and the text is read as a literal. A value wholly
enclosed in single quotes, two characters or more, loses the quotes and stays text, so `'abc'` binds
`abc`, `''` the empty text and `'a'b'` the text `a'b`. A value holding `_` stays text. Otherwise text
Python's `int()` reads becomes an integer, text its `float()` reads as a finite real becomes a real,
and `true`, `True`, `false` and `False` become booleans; anything else stays text. The order is what
leaves `1` an integer rather than a true, and `yes` and `no` text rather than booleans. So ` 1`, `+1`
and `١` bind `1`, `00012` binds `12`, `1.50` binds `1.5`, `1e3` binds `1000.0` and `1_0` the text
`1_0`, while `0x10`, `TRUE` and ` true ` stay as written. An integer of up to 4300 digits is an
integer of that size whether it is read by name, forwarded by a value that is one span reading it
(`c="${b}"`, a macro argument `a="${b}"`) or written inside an expression, so
`${99999999999999999999}` renders `99999999999999999999`. An integer too wide for 64 bits therefore
renders its digits, compares exactly with any number and is true wherever a truth is read; it equals
no string, it takes part in every operation as any integer does, exactly, and as a sequence index it
refuses as outside the sequence. A literal of more than 4300 digits refuses, naming that ceiling, and a
bound value of more digits stays text. An integer literal other than zero may not begin with `0`, so
`${007}` refuses under `expression_error` while `${00}` is `0` and `${007.5}` the real `7.5`; text a
property or an argument holds is read by the literal reading above, not by the expression's, so
`value="007"` binds `7`. The `integer_literal_leading_zero` agreement probe holds the refusal against
a fresh xacro render. An integer literal in an expression may carry single underscores between digits
and a `0x`, `0o` or `0b` prefix (in either case), as a Python literal may, so `${1_000}` is `1000`,
`${0x10}` is `16` and `${0_0}` is `0`, while `${0_7}` refuses as `${07}` does and an underscore
anywhere but between two digits (`${1__0}`, `${1_}`, `${0x1_}`), a prefix with no digit (`${0x}`, and
`${0or 2}`, whose `0o` is the octal prefix), a digit outside its base (`${0b2}`), a literal directly
followed by a name other than the keywords `and`, `else`, `for`, `if`, `in`, `is`, `not` and `or`
(`${1abc}`, `${1e}`; `${1if 1else 2}` still reads) and a real literal outside the range of a double
(`${1e999}`) refuse under `expression_error`, each naming its fault. A real may
carry the underscores too, so `${1_0.5}` is `10.5`. A prefixed literal too wide for 64 bits is a wide
integer, and one whose value has more than 4300 digits refuses as a longer decimal literal does. This
is the expression's reading: text a property or an argument holds keeps the literal reading above, so
`value="0x10"` stays `0x10`. An imaginary literal (`${7j}`) names a complex number, which the built-in
evaluator does not hold: it refuses as `unsupported_expression`, so a lenient policy leaves the span
as written. `meios::eval-python` evaluates it, so `${abs(3+4j)}` renders `5.0` there, while a complex
result is not written as text and refuses as an `error`. The `number_literal_forms`,
`number_literal_wide_prefixed`, `number_literal_separator_doubled`,
`number_literal_separator_leading_zero` and `number_literal_digit_outside_base` agreement probes hold
these forms against a fresh xacro render, and the `number_literal_imaginary` row of the divergence
manifest lists the imaginary literal.
Under `meios::eval-python` an integer too wide for 64 bits reaches the expression as a Python
integer, so `${b * 2}` renders `199999999999999999998`, and a property bound to `${2**70}` renders
`1180591620717411303424`. In an interpreter it starts, the backend holds the integer-to-text limit at
4300 digits whatever `PYTHONINTMAXSTRDIGITS` in the environment sets, so an integer of up to that many
digits crosses into an expression and out of it, and a longer result refuses as an `error`
(`expression_error`), where the built-in evaluator refuses one as a crossed ceiling. An interpreter the
program already runs keeps the program's own limit. A limit that cannot be held is reported once: the
process's first Python evaluation refuses as an `error` saying so, and later ones run under the
interpreter's own limit. A value `float()` reads as infinity, `1e400` or `INFINITY`,
stays text as written, because a real here is always finite; [known
limitations](known-limitations.md#expression-evaluation) lists that difference. The reading
runs wherever a value is bound: a property evaluated where it is written, a property stored for its
first read, which keeps the unquoted text so `'${x}'` stores `${x}`, that property once its first read
resolves it, a macro argument and a macro parameter's default. So
`<xacro:property name="hand" value="false"/>` followed by `${'' if hand else prefix}` takes the branch
a reader expects; a non-empty string would take the other one. The same reading is applied to
whatever an expression evaluates to, so `${'0' + '1'}` binds the integer `1`, exactly as `value="01"`
does. An argument and a boolean attribute are not read this way; both are described below. The
`property_quoted_value`, `property_padded_number`, `property_signed_number`, `property_unicode_digit`,
`macro_argument_literal_reading`, `property_wide_integer_forwarded`,
`property_wide_integer_forwarded_eager`, `macro_argument_wide_integer_forwarded`,
`integer_literal_wide`, `wide_integer_compared`, `wide_integer_condition`,
`wide_integer_compared_with_real`, `wide_integer_negated`, `wide_integer_added`,
`wide_integer_doubled`, `wide_integer_read_by_name_added`, `integer_power_beyond_64_bits`,
`wide_integer_squared`, `wide_integer_floor_division`, `wide_integer_modulo`, `wide_integer_halved`,
`wide_integer_divided`, `wide_integer_beside_real`, `wide_integer_real_power` and
`real_floored_beyond_64_bits` agreement probes hold this against a fresh xacro render, beside the
`xacro_literal_read`, `xacro_wide_integer`, `native_wide_integer`, `native_exact_compare`,
`native_big_integer` and `native_wide_real` cases.

**A property's value is evaluated at its first read, in the table that defined it.** The text written
in `value` or `default` is stored where the property is written, and the first expression that reads
the name evaluates it with the defining table current; the value, or the refusal, is cached in that
table. So a property may name one written below it; a property written in a macro body reads that
call's parameters, while one written outside the macro and read from its body does not; and a property
nothing reads is never evaluated, so a value that would fail refuses nothing. A text whose evaluation
reads its own name again, directly or through others, refuses as a circular variable definition under
`xacro_structural_error`, naming each step with the line that defined it: `the value of 'a' depends on
itself: a (robot.xacro:2) -> b (robot.xacro:3) -> a`. A property written with `lazy_eval="false"`, or
with `scope` set to `global` or `parent`, is evaluated where it is written, against the bindings above
it; `lazy_eval` is read by the boolean rule below, so `yes` refuses. Once `expand` returns, nothing
evaluates a stored text any more: a property whose text holds an expression and that the expansion
never read reads absent from the scope.

**A property's name is evaluated and checked before anything else about it.** The `name` attribute is
required, and it is evaluated as an attribute is, so `name="${p}_x"` with `p` bound to `a` binds `a_x`.
What it evaluates to must be text forming a Python identifier as Python 3.14 reads one under Unicode
16.0 — `_` or an XID_Start character first, XID_Continue characters after — that is not one of the
language's hard keywords, is its own NFKC normalization, and does not open with a double underscore. A
name failing any of these refuses under `xacro_structural_error` before `remove`, `default` or `value`
is read, and one failing any of the first three refuses with meios's own diagnostic naming it, such as
`xacro:property name 'a€' is not a name an expression can read`. So `a€`, which is not an identifier,
and `ﬁ`, whose NFKC form is `fi`, refuse, and `π` binds. The `property_name_non_ascii_not_identifier`,
`property_name_not_normalized` and `property_name_non_ascii_binds` agreement probes hold those three
against a fresh xacro render, beside the other `property_name_` and the `property_without_name`
agreement probes and the `xacro_property` cases. The identifier and normalization tables this rule and
the name reading above use are generated from Python 3.14's `unicodedata` (Unicode 16.0.0), and
regenerated and checked against that interpreter on every CI run, so a character added or retired
between Unicode versions is read here by the Unicode 16.0 tables, whichever interpreter xacro runs on.

**A condition and every boolean attribute are read by one rule.** The `value` of `xacro:if` and
`xacro:unless`, an include's `optional`, and a property's `remove` and `lazy_eval` are substituted
once, and what the substitution produced is never evaluated a second time. Its reading: `true` and
`True` are true, `false` and `False` false; text Python's `int()` reads — surrounding whitespace, a
sign, single underscores between digits, any Unicode decimal digit — is true when it is nonzero, and
so is text `float()` reads as a finite real; a typed boolean, integer or real reads by its truth, a
null is false, and a sequence or a mapping is true when it holds anything. Letter case is not folded.
Anything else refuses under `xacro_structural_error` at the element's `file:line`, naming the authored
value and what it evaluated to, as `xacro:if value="TRUE" evaluates to "TRUE", which is not a boolean`
does; so `TRUE`, `yes`, `nan`, `inf` and `${'a'}` refuse, and `xacro:if value="${'' or 'x'}"` refuses
because the `or` yields the text `x`. `1_0` and `١` take the branch, and so does `1.0`, where xacro
refuses it, a difference [known limitations](known-limitations.md#expression-evaluation) lists. An
empty `value` on `xacro:if` or `xacro:unless` refuses, as an empty `optional` does, and a missing
`value` refuses as a missing required attribute, below; an empty `remove` or `lazy_eval` reads as that
attribute's default, which the `property_flag_empty` agreement probe holds. A value that pads one span
with spaces is read by what the span yields: `value=" ${True} "` takes the branch and `value=" ${0} "`
does not. This is meios behavior; xacro reads the padded value as text and refuses the first, and
the `conditional_padded_expression` row of the divergence manifest holds that difference in both
directions. The `conditional_upper_true`, `conditional_empty`, `conditional_quoted_text`,
`conditional_without_value`, `unless_without_value`, `conditional_underscore_digits`,
`conditional_unicode_digit`, `conditional_not_a_number` and `conditional_padded_one` agreement probes
hold the rule against a fresh xacro render, beside the
`xacro_condition` and `native_boolean_value` cases.

**An argument binds the text, and the site that reads it is where that reading happens.** An argument —
a declared default, or an override a program hands `load()` through `load_options::args` — binds the
characters that were written; the conversion above is not applied to it. An override the `meios`
command line reads as `name:=value` first has the white space `str.strip()` removes taken off its
name and its value, so `a:= x ` binds `x`, while a program's override binds as written, which the
`caller_argument_as_given` agreement probe holds against a fresh xacro render. The command line splits
an override at its first `:=`, so `a:=b:=c` binds `b:=c`, and an override whose name or value is empty
once stripped, or whose name has a single leading underscore such as `_p`, binds nothing and writes a
warning saying so. Only a default that is
itself one whole expression binds that expression's value, exactly as an attribute of the same shape
does, so `default="${1+1}"` binds the integer `2`. Everything else stays as written, which is what
leaves `$(arg flag)` of a `false`-spelled default rendering `false` into a flattened document rather
than the `False` a boolean spells, and what leaves a `0.10` default its trailing zero. The conversion
happens instead where the argument is read into something that does bind a value:
`<xacro:m flag="$(arg flag)"/>` hands text to the parameter, the parameter binds it by the reading
above, and `${flag}` inside that macro is a boolean. Both implementations keep two tables — an
argument table the `$(arg n)` and `$(eval)` commands read, and a property table an expression span
reads — so an expression span cannot name an argument: `${flag}` over an argument alone refuses,
and the refusal names `$(arg flag)`. A property of an argument's name is a second binding beside the
argument, never a write to it, so a property's `default=` binds over an argument of the same name
and `$(arg n)` answers the argument whatever property is written. `$(eval)` reads each argument it
names through a conversion of its own rather than the one above — `1e3` stays text there, `1_000`
is the integer `1000`, a digit is any decimal digit Python 3.14 reads, so `١٢` is `12`, and an
argument holding one of U+001C to U+001F is text there, as the `eval_argument_separator_character`
agreement probe holds against a fresh xacro render — sees the mathematics names and the spellings
`true` and `false` before any argument, sees no property, refuses a double underscore, and ends at
its first closing parenthesis, so its body holds no call;
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

`min` and `max` take one or more arguments — zero is a fault, not an identity — order them exactly
and answer the first of equal arguments, so `${min(1, 1.0)}` is `1` and `${max(1.0, 1)}` is `1.0`.
`atan2` takes exactly two, and the rest take exactly one. `abs` of an integer is an integer; `floor` and `ceil` answer
an integer unchanged and a real as the exact integer it rounds to, of any size, so `${floor(1e30)}` is
`1000000000000000019884624838656`. All fifteen render here. `abs` is not a portable spelling: xacro's
expression globals carry the mathematics module and the two reducers and no builtins at all, so
`${abs(-3)}`, which renders `3` here, raises an undefined name under xacro. It is a reviewed
difference, and the section on differences below carries it. Any other name followed by `(` is refused
by that name, so a description reaching for a function this list does not carry is told which one it
reached for rather than that something went wrong. A name on the list called at the wrong arity is
refused the same way, by name, so `atan2(x)` reports the function rather than the count.

**A span's own text is resolved before its expression is evaluated.** `${...}` and `$(...)` both expand
any span written inside them first, and what the evaluator or the command dispatcher is handed is the
string that produced — so `${xacro.load_yaml('$(find arm_description)/cfg/limits.yaml')}` has already
become a path by the time the expression runs. That order is why the spelling a real description writes
for a package-qualified document reads at all. A command ends at its first closing parenthesis, so one
command cannot be written inside another, and a `(` or a `$` left in a command's text once its spans
are resolved refuses; only the `$(eval)` body, which an evaluator reads, may hold either. The argument
command takes exactly one name once its spans are resolved, and refuses a second word whether or not
the argument is bound: `$(arg ${a + b})` reads the argument the sum names, and a span that produces two
words refuses.

All of that nesting is charged against the expression-depth ceiling, and each shape is charged what it
costs in stack rather than one apiece: descending into the grammar is one and a nested scan is four. An
expression's own nesting counts on the same ceiling as the spans it stands inside, so an expression
inside a nested span has that much less room. So charged, every shape the ceiling admits keeps ninefold
headroom on a 512 KiB thread, which the test suite measures in an unoptimized build, and a document
nesting expression spans past it refuses at a stated position rather than running out of stack;
commands, which end at their first closing parenthesis, do not nest at all. A property evaluated at its
first read nests too: each such read is a level charged nineteen on top of the nesting of the
expression that reads it, so a chain of ten properties each reading the next renders and one of eleven
refuses with `expansion_budget_exceeded`; xacro renders a chain of one hundred and sixty-five, a
difference [known limitations](known-limitations.md#expression-evaluation) lists. A read inside
arithmetic carries that nesting as well, so a chain of `${p1 + 1}` reads renders six properties deep.

**The kinds a value can have.** An expression works over exactly seven: a null, a boolean, an
integer of any size up to 4300 digits, a real, a string, a sequence and a mapping. The first five have a scalar spelling and can be
written into a document; a sequence and a mapping have none, so one reaching a document's text is a
located, typed failure naming the kind rather than an invented serialization. Those last two reach an
expression two ways: out of an auxiliary document, and out of the grammar itself — the mapping
constructor builds a mapping and the named split builds a sequence. Which of the two an expression is
holding still matters, because only a document's mapping answers a dotted member read. A collection
the grammar builds is charged one element before each of its elements is built, so a produced
collection cannot outgrow a loaded one.

**A string is text, and the operations on it are the ones a real description evaluates.** String
literals evaluate, compare for equality against another string, and serve as mapping keys. Two strings
add to their concatenation, a string's truth value is its emptiness, `'x' in text` tests substring
containment, and `text.split('<separator>')` yields the fields between separators — an empty field
wherever two separators meet and at either end — which the subscript layer then indexes. Every other
meaning a string carries in Python stays out and refuses: repetition by an integer, ordering one string
against another, and every member spelling but the one named split. A string added to any other kind
refuses, because coercing a string to a number is how a description quietly means something other than
what it says.

The four admitted meanings compare and count by bytes, where xacro counts code points. Bytes and code
points agree over the ASCII text every measured description carries; whether any of them carries
non-ASCII text through containment, a split or a concatenation is unmeasured, so the difference is
recorded here rather than claimed absent.

## What refuses, and by which rule

A refused expression is never evaluated. Which refusals the active `eval_policy` may soften is
decided by the failure's kind, and there are exactly four:

| Kind | What produces it | Terminal |
|------|------------------|----------|
| `unsupported` | a construct outside the grammar that a fuller backend would evaluate — a comprehension, an f-string, a member of a string other than the named split, a Python constructor, a dotted read of anything but a loaded mapping's key, arithmetic on a string beyond adding two of them | no: a lenient policy may leave the span verbatim |
| `error` | a genuine fault — an undefined name, a missing key, division by zero, an integer literal of more than 4300 digits, an integer or a quotient too large for a real, a non-finite result, malformed syntax | yes, under every policy |
| `exhausted` | a crossed resource ceiling, the built-in evaluator's 4300-digit integer ceiling among them | yes, and it halts the load rather than the expression |
| `refused` | a rule of the opt-in Python backend; the built-in evaluator never produces one | yes, under every policy |

Every diagnostic carries a `file:line:column` and a `diagnostic_code`, so a caller branches on a code
rather than matching message text. The message is meios's own. Under the python evaluator, a message
the interpreter raises itself, such as its refusal of a bare name nothing binds, is passed through as
the interpreter wrote it, while the built-in evaluator writes its own. `unsupported_expression` covers
the whole first row, `undefined_property` covers both a name the scope does not bind and a key a
mapping does not hold, and `expansion_budget_exceeded` covers a crossed ceiling. The message names the
lexeme the walk stopped on, which is what tells you where in a long expression to look.

**Malformed syntax is refused before anything is evaluated.** One left-to-right walk over the token
stream alternates operands and operators, so `1 +` is reported as an expression ending where an
operand is expected and `f(1 + )` as an unexpected `)`. A closing bracket is the one token whose
legality turns on what stands before it, which is why an empty argument list is accepted and a
truncated argument is not.

**A dotted spelling reads an include namespace's names or a loaded mapping's keys, and nothing else.**
An include written with `ns="n"` reads its file into a namespace of its own, and `${n.w}` reads the
name `w` that file bound; a name the namespace does not hold falls through to the names of the file
that wrote the include and then to the global names, reading what that name reads bare: `${n.pi}` reads
the constant and `${n.sin(0)}` calls the function. The constants `e`, `tau`, `inf` and `nan` refuse
bare here as undefined names, so `${n.e}`, `${n.tau}`, `${n.inf}` and `${n.nan}` refuse as undefined
names too; xacro renders each constant under either spelling. `${n.math.pi}` refuses as the undefined
name `math`, while the bare `${math.pi}` refuses as an unsupported call target, so the two spellings
refuse differently here; xacro renders both, and the differences [below](#where-meios-and-xacro-differ)
name what holds each of these. A call through a namespace to a name xacro's global table does not hold
refuses as an undefined name, `${n.abs(-2)}` among them, though the bare `${abs(-2)}` renders here. A
namespace inside another reads through both dots, as `${a.b.c}` does. `<xacro:n.m/>` calls the macro
`m` the namespace holds. A call in that macro's body of a macro the including file defined,
`<xacro:outer/>`, refuses under `xacro_structural_error`, which the
`namespace_macro_cannot_call_outer_macro` agreement probe holds against a fresh xacro render. A
namespace has no value of its own, so `${n}` read bare refuses, naming the dotted spelling; xacro reads
its table, as [known limitations](known-limitations.md#expression-evaluation) lists. A namespace named
`math` shadows the mathematics names under that spelling: `${math.c}` reads the namespace's `c`, and
`${math.pi}` reads the constant by falling through. `xacro.load_yaml` is the one namespaced call the
grammar carries; every other namespaced spelling, `math.pi` among them where no include names its
namespace `math`, is refused by name. A dot after a value is a member read, and it resolves only for a
value that came out of a loaded auxiliary document: `${config.joint_limits.wrist_3.effort}` reads the
same key `${config['joint_limits']['wrist_3']['effort']}` reads, chains with a subscript in either
order, and survives a property, a macro argument and a nested scope. A mapping a description built
itself — what the mapping constructor below returns — has no member path at all, and neither does a
loaded sequence or a loaded scalar. Two member spellings are refused on a loaded mapping as well: a
name that spells a mapping operation (`clear`, `copy`, `fromkeys`, `get`, `items`, `keys`, `pop`,
`popitem`, `setdefault`, `update`, `values`), and any name written with a leading `__`. Both point at
the subscript spelling instead, because a description that means something here other than what it
means under xacro is worse than a description that will not load.

**The mapping constructor builds a mapping; every other constructor is refused by name.**
`dict(a=1, b=2)` takes keyword arguments and nothing else, and builds a mapping the grammar can
subscript, test membership on and nest inside another one. A positional argument, a pair-sequence
argument and a mixture of the two each refuse naming what was met instead, and a repeated keyword name
refuses rather than resolving to one of its values. What it builds is an authored mapping, so it has no
member path: `dict(a=1)['a']` reads, `dict(a=1).a` does not. `list(...)`, `set(...)` and `tuple(...)`
are named outright, so the diagnostic says which constructor was reached for instead of reporting an
unrecognized function; a set and a tuple have no representable kind here at all. Brace-literal and
bracket-literal container syntax stays refused.

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
produces (100 000) over the whole load; and, for any one expression, its nesting depth (188), its
token count (10 000), the magnitude of an exponentiation's real operands (1 000 000 000 000) and the
length in bytes of a string it produces (1 000 000). Six of them
accumulate across every expression in one load rather than resetting per expression, so a document
cannot spend a bounded budget an unbounded number of times; the other five are high-water marks instead, because what they bound is
the most any one expression reaches and not how often it is reached. A ceiling requested as zero is
refused and takes its default, so there is no spelling that means unbounded. Crossing one is
terminal and stops the load at the position that crossed it: a load continuing past its own ceiling
would report a partial answer as a whole one. Beside the eleven, an integer under the built-in
evaluator holds at most 4300 digits, a fixed bound that cannot be configured and is crossed as the
others are: an integer of more digits arising anywhere in an expression halts the load under
`expansion_budget_exceeded`, and an integer raised to a non-negative integer exponent is bounded by it
instead of by the magnitude ceiling, refused before the work. A power refused that way says it would
produce an integer of more than 4300 digits, where a result computed past the ceiling says it produced
one. Each arithmetic operation that converts an integer wider than 64 bits to work on it is charged
one evaluation step for every nine of its digits, plus one, so repeated arithmetic on long integers
spends the evaluation step ceiling; a comparison, `min` and `max`, a truth, a unary `+`, `floor` and
`ceil` of an integer and a subscript read the digits without converting them and are not charged.

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
`!millimeters`, `!foot` and `!inches` each multiply their tagged value by a fixed constant, so
`!degrees 90` reads as `1.5707963267948966` and `!inches 12` as `0.30479999999999996`. The character
set here admits a decimal literal and nothing else, so a tag written over a name, a call or a
hexadecimal literal declines rather than being approximated; xacro evaluates a tagged scalar's text as
an expression. That decline is softenable — a lenient policy leaves the span verbatim — while a tag
outside the six is a fault under every policy, named in the diagnostic, because a conversion this table
does not carry has no constant to apply.

## Where meios and xacro differ

xacro evaluates a `${}` body through Python. meios and xacro differ in both directions, and every
difference below is measured against the pinned xacro rather than reasoned about from its source.

**Tighter, by construction rather than by restriction.** There is no interpreter here and no symbol
table to widen, where xacro hands an expression to an interpreter alongside one. Comprehensions,
generator expressions, lambdas, f-strings, every string method but the one named split, the sequence
and set constructors, the object protocol and the import machinery are not restricted — they are absent
from the grammar, and an expression reaching for one is refused with a located diagnostic. The one
thing a dot can mean here is a key of a mapping this evaluator loaded; it reaches no attribute of
anything. Nothing in the grammar can name the filesystem, the network or the process: the one route to
a file is the resource helper, where C++ resolves the spec, enforces containment and reads the bytes.

**The mapping constructor takes keyword arguments, and xacro takes more.** xacro also builds a mapping
from a sequence of pairs and from a mixture of a positional argument and keyword ones;
`${dict([('a', 1)])}` refuses here and renders under xacro. Only the keyword spelling was measured and
admitted, so the wider shapes refuse rather than taking a meaning nothing measured. Both are recorded
as reviewed divergences, which means the comparison fails if either stops reproducing exactly as much
as it fails if a new one appears.

**A member read spelling a mapping operation is refused, not answered.** Here the attribute spelling
`${config.keys}` refuses and names the subscript spelling, and `${config['keys']}` renders the
document's value, which is compared against xacro on every push. xacro hands a loaded mapping back
inside a wrapper that tries ordinary attribute lookup before falling through to the document, so
`${config.keys}` renders a bound method there — text carrying the object's own address. The eleven
colliding names are measured into a committed record and the refusing set is checked against it rather
than written down. This one is recorded as a reviewed divergence in prose rather than in the manifest
the comparison reads, because the text xacro produces embeds an address and does not reproduce between
runs.

**A missing key is named; the mapping's other keys are not.** The diagnostic names the key that was
absent and the `file:line` that read it, and deliberately never lists the keys the mapping does hold: a
diagnostic must not spill the configuration it declined to read. Against a configuration nested three
mappings deep that costs you one lookup you can make yourself, and it means a message written into a
build log carries no auxiliary content.

**How an auxiliary scalar reads.** How a scalar in an auxiliary document reads is a recorded
measurement against the pinned PyYAML rather than a reading of the YAML specification, compared row for
row: `yes` and `no` are booleans while `y` and `n` are strings, `010` is the integer 8 and `1_000` is
1000, `1e5` is a string while `1.0e+5` is a real, and a key written with no value at all is a null that
spells `None`.

**A block argument's contents are evaluated at the call site and walked again where they are
inserted.** A macro's block argument is expanded where the caller wrote it, in the caller's macros and
scope; the block parameters then bind to the element children of that expanded result, in the call's
own table. Where a macro inserts a block, its contents are walked again in the inserting scope: a span
the call site left escaped, `$${x}`, is evaluated at each insertion and reads the inserting call's
parameters, and a `**` block's wrapper has its attributes evaluated again, and discarded, before its
children are inserted. A block a call hands on is seen by a macro its body calls. This evaluator used
to expand a block only where the receiving macro inserted it, which made an undefined name inside a
discarded block a load failure under xacro and nothing at all here. That difference is closed. The
change is visible on descriptions that load: a side effect written inside a block nobody inserts now
takes effect, and a block naming an unbound property now fails the load whether or not it is reached.
[Known limitations](known-limitations.md) says so in the reader's own terms. The `block_` agreement
probes hold the insertion against a fresh xacro render, among them a block inserted twice and a block
reading the inserting call's parameter.

**A property written with no value is a block.** A property with neither `value` nor `default` binds no
value: its element content is stored as a block of the property's name, in the table its `scope`
selects, and `<xacro:insert_block>` inserts that content, walked in the inserting scope, so a block
property inserted from a macro body reads that call's parameters. A block property is found before a
block parameter of the same name. An expression span does not reach a block, so `${p}` over a property
written with nothing refuses as an undefined name, inserting it inserts nothing, and a name already
bound to a value keeps that value. A removal of a name no table holds stores its content the same way.
The `property_` and `block_property_` agreement probes hold these against a fresh xacro render.

**A call that leaves a block parameter without an element refuses.** A macro call's children are
expanded, and then the call refuses for any `*` or `**` parameter left without an element, before any
default is read, so a call holding only an `<xacro:include>` of a document that binds a property, only
a property, or nothing at all refuses even when the macro never inserts the block. The refusal is
raised under `xacro_structural_error` at the call's own `file:line`, naming the first parameter left
without an element. The `include_sole_block` agreement probe and the generated macro cases xacro
refuses for a block parameter left without an element hold it, two of them over a default that would
refuse if it were read.

**A macro call binds in a fixed order.** A call's attributes bind in document order, then the call's
expanded children as its block arguments, then the defaults, and only then is a parameter still unbound
refused, all of it in the caller's scope before the macro's body is entered. A property written among a
call's children is therefore not visible to the call's attributes, which refuse it as an undefined
name, while a default reads it. A default reads a parameter the call supplied too, as the invocation's
own scope holds it, so `b:=${a}` with both supplied renders, and one reading a parameter the call left
out refuses. An attribute naming no declared parameter, a namespace declaration or a block parameter's
name included, refuses naming the attribute, and an element no block parameter takes refuses naming the
first left over; a child that expands to nothing, a property say, takes no block parameter. Where two
of these refusals meet in one call, the first by a fixed precedence is raised, and the probes below pin
it. Each is reported under `xacro_structural_error` at the call's own `file:line`, for a macro defined
in another file as well; a parameter left unbound is refused under the same code. A default that
refuses is reported at the call's own `file:line` too, under its own cause, whether or not the call
supplied its parameter, for a macro defined in another file as well. Thirteen `macro_call_` agreement
probes hold this against a fresh xacro render: an undeclared attribute, a namespace declaration and a
block parameter's name written as attributes; one and two surplus elements; an undeclared attribute
before a missing parameter, a refusing block child, an absent block and a refusing attribute, and a
surplus element before a missing parameter; an attribute reading a child's property, a default reading
one, and a child that is only a property. The `macro_default_reads_supplied_parameter` probe holds a
default's reading of a supplied parameter, beside the `xacro_macro_defaults` cases, which pin the
omitted one's refusal too. The `xacro_macro_call` cases pin the location, the included macro among
them, and `xacro_macro_call.a default that refuses while the call supplied its parameter is reported at
the call, for a macro defined in another file` the supplied parameter's refusing default.

**A macro's parameter list is read parameter by parameter, not split at every space.** A default that
opens with one `${…}` or `$(…)` span binds that span whole, a space inside it included, so
`a:=${1 + 2} b:=2` binds `a` to `3` and `a:=$(arg y) b:=2` binds it to the argument. A space may stand
on either side of the separator: `a := 1`, `a= 1` and `a :=1` each bind `1`. An empty default, `a:=`,
is no default, so a call omitting `a` refuses as missing it. A parameter whose quote never closes,
`tag:='world`, is read whole as a name with no default, so a call omitting it refuses as missing
`tag:='world`. `a:=^d` takes `a` from the enclosing scope or else `d`, and `a:=^|` alone takes it from
the enclosing scope or refuses. And text after a newline that follows a parameter is not read. The
`macro_default_empty`, `macro_default_spans_whitespace`, `macro_default_argument_command` and
`macro_param_spaced_separator` agreement probes hold the span, separator and empty shapes against a
fresh xacro render; the `xacro_macro_params` cases pin those, the unclosed quote and the newline; and
the two forward markers are carried by a measurement against the pinned xacro alone. A `params`
attribute holding only whitespace refuses under `xacro_structural_error` at the definition once the
expansion reaches it, called or not, while one in a definition the expansion never reaches is never
read; an empty `params` and a list ending in whitespace read as before. The
`macro_params_only_whitespace` and `macro_params_only_whitespace_never_reached` agreement probes hold
both against a fresh xacro render, beside the `xacro_macro_defaults` cases. Every character Python's
`str.isspace()` holds separates two parameters, not ASCII whitespace alone: the twenty-nine code
points the name reading above strips, the no-break space U+00A0, the ideographic space U+3000 and the
form feed among them, so `a:=1` and `b:=2` written with a no-break space between them bind `a` to `1`
and `b` to `2`, and a `params` attribute holding only such characters refuses at the definition as
one holding only ASCII whitespace does. Such a character inside a quoted default stays part of the
default. The `macro_params_unicode_space_between` agreement probe holds the no-break space against a
fresh xacro render, beside the `xacro_macro_params` cases. XML 1.0 admits no form feed, so xacro
cannot load a document holding one and a unit case alone pins that separator.

**A run of dollars escapes or refuses.** A run of two or more `$` before `{` or `(` writes one `$`
fewer and the opener, and lexing goes on after it: `$${x}` renders `${x}`, `$$(arg y)` renders
`$(arg y)` and `$$$${x}` renders `$$${x}`. A `$$` before anything else, or ending the text, refuses
under `expression_error`, naming the run, at the run's own column and under every evaluation policy,
inside an expression body as well, so `a$$x`, `a$$` and `${'$$'}` each refuse. In an argument's default
it refuses at the declaration, whether or not the argument is read, unless a caller's argument leaves
that default unread. The token straight after a span is lexed before the span is evaluated:
`${nope}$$x` refuses for the dollar run rather than for the undefined name, and `${nope}${x` refuses as
an unterminated span. Exactly one token is looked at, so `${nope}a$$x` refuses for the undefined name.
A following `${` token ends at its first `}`, so `${nope}${'a}'` and `${nope}${a{b}` refuse for the
undefined name, while a span's own close stays quote-aware, the difference recorded below. An include
filename is lexed the same way, so after a `$(find …)` in it, `$(find ${nope})$$x` refuses for the
dollar run; the order its other refusals take is stated with the include filename below. A single `$`
before anything but an opener stays text, as `$x` does. The `dollar_run_escapes_openers`,
`dollar_run_before_text`, `dollar_run_at_end` and `dollar_run_inside_expression` agreement probes hold
this against a fresh xacro render, with `dollar_run_in_argument_default`,
`dollar_run_in_unused_argument_default` and `dollar_run_in_redeclared_argument_default` for an
argument's default, `dollar_run_after_undefined_name`, `dollar_run_after_unset_argument`,
`dollar_run_after_division_by_zero` and `unterminated_span_after_undefined_name` for the token after a
span, and `include_dollar_run_after_find` and `include_unterminated_span_after_find` for the token
after an include's find, beside the `xacro_dollar_run`, `xacro_arg_declaration`, `xacro_lex_lookahead`
and `xacro_include_find` cases and `arg_scan.scan_args lists no argument a run of dollars
escapes`, which keeps an escaped `$(arg …)` out of the argument listing. `${'$${x}'}` renders `${x}`
here and refuses under xacro, which is the closing brace inside a string literal, recorded below; it
adds no difference of its own.

**An argument binds where the expansion first reaches its declaration.** Nothing is bound ahead of the
walk: a `$(arg n)` the expansion reaches before any declaration of `n` refuses under `unresolved_arg`
unless a caller supplied `n`, with `$(arg n) has no value here: no caller supplied it, and no
declaration the expansion has reached gives it a default`. The first declaration the expansion reaches
binds its default, unless a caller supplied the name. A later declaration of the same name binds
nothing, so a second default neither replaces the first nor rescues one that refuses, and a declaration
the expansion never reaches, in a branch not taken or a macro never called, binds nothing at all. A
declaration in an included file the expansion reaches first binds before the including document's own,
and one in a macro body binds on the first call. A declaration reads its default before it binds, so a
`$(arg a)` in the default of `a` refuses as unset. A declaration without a `default` is a required
argument: it binds nothing, and a caller supplies it or its first use refuses under `unresolved_arg`
at that use's `file:line` with the message above. xacro refuses a declaration without a default
wherever it stands, a difference [known limitations](known-limitations.md#expression-evaluation)
lists. A description whose root reads an argument above its declaration therefore needs that argument
from its caller: the Universal Robots `ur.urdf.xacro` names its robot `$(arg name)` before declaring
`name`, so it loads with `name:=` given beside `ur_type:=`; xacro needs the same. The
`argument_above_declaration`, `argument_lent_from_false_conditional`,
`argument_lent_from_uncalled_macro`, `argument_redeclared_after_refusing_default`,
`argument_redeclared_after_expression_default`, `argument_declared_in_skipped_branch`,
`argument_declared_in_uncalled_macro`, `argument_declared_in_include_first` and
`argument_default_reads_itself_before_redeclaration` agreement probes hold this against a fresh xacro
render, beside the `xacro_arg` and `xacro_arg_declaration` cases.

**Each xacro element names the attributes it requires and the ones it reads.** `xacro:include`
requires `filename` and reads `ns` and `optional`; `xacro:macro` requires `name` and reads `params`;
`xacro:property` requires `name` and reads `value`, `default`, `remove`, `scope` and `lazy_eval`;
`xacro:arg` requires `name` and reads `default` without requiring it, since a declaration without one
is a required argument; `xacro:insert_block` requires `name`; `xacro:if` and `xacro:unless` require
`value`; `xacro:call` requires `macro`; `xacro:element` requires `xacro:name`; and `xacro:attribute`
requires `name` and `value`. An element missing a required attribute refuses under
`xacro_structural_error` at its `file:line` before anything else about it is read, naming the element
and the attribute, as `<xacro:macro> needs a name attribute` does; an attribute written empty is
present, not missing. An attribute one of the first seven elements does not use warns at that element,
once per attribute in document order, as `<xacro:if> carries an attribute 'doc' it does not read` does;
a macro definition evaluates such an attribute on each call, so its warning reads `<xacro:macro>
carries an attribute 'doc' it evaluates on each call but does not use`, except one in the xacro
namespace, which it never reads, as `<xacro:macro> carries an attribute 'xacro:doc' it does not read`
says. A namespace declaration,
`xmlns:` and a prefix, never warns. The warning carries no code, so a load whose document renders keeps
its `parsed` claim. `xacro:call`,
`xacro:element` and `xacro:attribute` warn about nothing, since their other attributes are a macro's
parameters, the renamed element's own attributes, or nothing they read. An `xacro:arg` whose `name` is
written empty is a declaration like any other: its `default` is evaluated, so a default that fails
refuses, and the value is bound under the empty name, which no `$(arg)` can read; the
`argument_empty_name` agreement probe holds this against a fresh xacro render. The
`macro_without_name`, `include_without_filename`, `property_without_name`,
`conditional_without_value` and `unless_without_value` agreement probes hold the refusals against a
fresh xacro render, beside the `xacro_attribute_table` cases, one of which pairs the table with the
attribute lists the pinned xacro's source declares, so a change to either fails the build.

**A macro's name is checked where it is defined.** A macro named `call` refuses, `a macro cannot be
named 'call', which xacro:call takes`, and so does a name holding a `.`, `macro name 'a.b' holds a
'.', which separates a namespace from a name`, both under `xacro_structural_error`. The dot is looked
for in the whole name, so `xacro:a.b` refuses as dotted. A name opening with `xacro:` warns, `macro
name 'xacro:m' is defined as 'm'`, and defines the macro under the rest of the name. The
`macro_named_call`, `macro_name_with_dot` and `macro_name_with_xacro_prefix` agreement probes hold
these against a fresh xacro render, beside the `xacro_macro_redeclared` cases, which pin the warning.

**`xacro:call` calls the macro its `macro` attribute names.** `<xacro:call macro="m" a="1"/>`
evaluates `macro` as text and expands the macro it names exactly as `<xacro:m a="1"/>` would: its
other attributes and its children bind as that call's, and `macro` takes no part in the binding. A
dotted name reaches a macro an include namespace holds, as `<xacro:n.m/>` does. A name no macro
answers refuses under `xacro_structural_error` at the call's `file:line`, as `<xacro:call> names
'nope', which is not a macro in scope` does, and so does `macro="if"`, which names no macro rather
than a conditional. An empty `macro` refuses as naming no macro, and `macro="call"`, written or
evaluated, refuses as naming `xacro:call` itself, which is no macro. The `call_empty_macro`,
`macro_called_by_name_attribute` and `call_unknown_macro` agreement probes hold these against a fresh
xacro render, beside the `xacro_element_construct` cases.

**`xacro:element` is processed again under the name it evaluates.** `<xacro:element
xacro:name="${'li' + 'nk'}" name="l"/>` evaluates `xacro:name` as text and handles the same element as
if that name had been written: a plain element, a conditional, a property, an argument, a macro
definition, a macro call, an `xacro:call`, and an `xacro:element`, which refuses as renaming itself.
`xacro:name` takes no part in what the renamed element reads; its other attributes are evaluated
once, where the new name's branch evaluates them, and every diagnostic it raises carries the element's
own `file:line`. An evaluated name, here and for `xacro:attribute` below, first
loses the XML white space at either end — space, tab, carriage return and line feed, nothing else —
and when any was removed, one located `level::warn` note with no diagnostic code names the evaluated
text and the name used, as `<xacro:element> evaluates its xacro:name to 'link ' and uses 'link',
without the white space around it` does, so a load that renders with it keeps its claims. A padded
name that strips to the name of an xacro element is handled as that element, so
`xacro:name="xacro:if "` is a conditional, where xacro refuses. What is left must be a name by the Name production of XML 1.0
(fifth edition, section 2.3); one that is not refuses under `xacro_structural_error`, naming the text,
as `<xacro:element> evaluates its xacro:name to 'a><x', which is not an XML name` does, so evaluated
text cannot write markup into the output, and an empty one, a name of white space alone among them,
refuses too. An evaluated value that is not text is written as its text before that check, so
`xacro:name="${True}"` makes an element `True`, where xacro refuses a name that is not text, and the
`element_name_boolean` row of the divergence manifest holds it; `xacro:name="${1}"` refuses because
`1` is not an XML name, which is why the `element_name_not_text` probe refuses on both sides. The
warning and the refusal quote the text as evaluated, and the command line writes each on one line,
as it writes every record ([where a diagnostic comes
from](troubleshooting.md#where-a-diagnostic-comes-from)). The production admits colons anywhere, so `:a` and `a:b` pass. xacro writes such text
into its output as it is, so `a b` or `1x` there yields a document that is not well-formed XML; here
each refuses, white space inside a name included. The `element_empty_name`,
`element_named_by_attribute`, `element_renamed_to_conditional`, `element_renamed_to_macro_call`,
`element_renamed_to_property`, `element_name_not_text` and `element_padded_name` agreement probes, and
`attribute_padded_name` for `xacro:attribute`, hold these against a fresh xacro render, beside the
`xacro_element_construct` and `xacro_attribute_place` cases, which pin the shapes whose xacro output
cannot be read back, an element name led by a space and an attribute name of white space alone
among them.

**`xacro:attribute` sets an attribute on the element its content lands in.** `<xacro:attribute
name="type" value="t"/>` evaluates `name` and `value` as text and sets the attribute on the element
holding it, overwriting a value already there, whether authored or set by an earlier
`xacro:attribute`; a plain element, the top document's root, a `*` block's element and an element
`xacro:element` renamed each receive it. An evaluated value that is not text is written as its text,
so `value="${1}"` sets `1`, where xacro refuses, and so is an evaluated `name`, which must then be an
XML name as `xacro:element`'s is: `name="${True}"` sets an attribute `True`, and `name="${1}"`
refuses. Where the element holding it does not reach the output — a conditional, a macro body, an
included document's root, a `**` block's wrapper or a block property's body — the attribute is set
on the element that parent's content lands in, the enclosing
element of the conditional, the call, the include or the insertion, and one located `level::info`
note says so: `attribute 'type' placed on <link>, the element its parent's content lands in`. xacro
sets it on the node the output drops, so there the attribute is lost; [known
limitations](known-limitations.md#expression-evaluation) lists both differences. Written directly
among a macro call's arguments, or where the content lands in no output element, it sets nothing and
warns, `attribute 'a' is written where no element receives it, so it is not set`, and xacro sets
nothing there either. Inside a block passed whole through one call's `*` parameter into another call's
`**` parameter it is not set, since that insertion keeps only the block's content, and xacro loses it
the same way; one located `level::info` note names it: `attribute 'k' is not set: the block holding it
was passed whole into another call, which inserts only its content`. The
`attribute_through_passed_block` agreement probe holds that drop against a fresh xacro render. The
`attribute_empty_name`, `attribute_set_on_parent`, `attribute_under_block_element`,
`attribute_on_document_root` and `attribute_padded_name` agreement probes hold the placement on an
element the output keeps against a fresh xacro render, beside the `xacro_attribute_place` cases.

**An attribute in the xacro namespace is removed unread, the xacro namespace declaration is evaluated
and left out, and character data is kept as written.** An output element's attributes whose names
carry the `xacro:` prefix are dropped before any attribute is evaluated, so `<link xacro:foo="${nope}"
name="l"/>` renders `<link name="l"/>` and refuses nothing, and a `**` block's wrapper drops them the
same way. `xmlns:xacro` is evaluated like any other attribute of the element holding it, whether an
output element, the top document's root, a block's element or wrapper, an included root or a macro
definition, so `<link name="l" xmlns:xacro="${nope}"/>` refuses under `undefined_property`; its result
is then left out of the output and never lifted. A `<![CDATA[…]]>` section is copied into the output
as character data without substitution, so `<![CDATA[${x}]]>` keeps `${x}`, and an undefined name or
an unset argument inside one refuses nothing. The `xacro_attribute_removed`,
`xacro_attribute_never_evaluated`, `xacro_namespace_declaration_evaluated` and `cdata_kept_verbatim`
agreement probes hold these against a fresh xacro render, beside the `xacro_output_shape` cases.

**The attributes of an included root and of a macro definition are evaluated, and their namespace
declarations are lifted onto the element their content lands in.** An included document's root is
read before its content, in the scope the include reads into: every attribute other than an `xacro:`
one is evaluated, so a root `name="${p}"` refuses where only the included content defines `p` and
renders where the includer defined `p` first. A macro definition is read on each call, with the
call's parameters bound and before its body: every attribute other than an `xacro:` one and `params`,
whose defaults each call evaluates already, is evaluated, so `doc="${p}"` over a parameter `p`
renders, and `doc="${nope}"` refuses where the macro is called and nowhere else. An attribute that
fails to evaluate refuses the load under its failure's code, located in the document holding it. The
results are discarded, except that each `xmlns:` declaration other than `xmlns:xacro` is set on the
element the include's or the call's content lands in. A prefix that element already declares with the
same value is left alone. One it declares with another value keeps its value, and one located warning
names the prefix and both values: `namespace prefix 'foo' is already bound to "urn:f" on <robot>, so
its declaration as "urn:g" here is not lifted`. A declaration whose content lands directly under a
conditional or in a `**` block's content is dropped, which xacro does too, and one whose content lands
in a `*` block's element is kept there. `xacro:targetNamespace` on the top document's root is
evaluated and sets the output root's default namespace, `xmlns`, over one written there, and is itself
removed with the other `xacro:` attributes; an empty value leaves the root as written, and on an
included document's root it has no effect. The `included_root_namespace_lifted`,
`macro_namespace_lifted`, `root_target_namespace`, `included_root_attribute_evaluated`,
`included_root_attribute_before_content`, `macro_definition_attribute_evaluated` and
`macro_definition_attribute_uncalled` agreement probes hold these against a fresh xacro render, beside
the `xacro_output_shape` and `xacro_include_read` cases; a unit case built from measured xacro renders,
and no probe, holds the dropped declarations.

**An include filename is substituted whole and then read as a path.** Every include filename but a
`package://` one is substituted as text, left to right with the one-token lookahead above, and each
`$(find …)` in it writes its package's directory as the command writes it anywhere. The result is then
resolved as a path, and a path whose leading elements are a directory a find in the filename rendered
is looked up as that package's file. A path counts as under that directory only where it is rooted the
way the directory is: a drive in both or in neither, and a separator after it in both or in neither. So
`/$(find p)/urdf/c.xacro` doubles the leading separator on POSIX and is still `p`'s file here, while on
Windows it puts a separator before the drive the find renders and is looked for as an absolute
filename, as `xacro_include_target.a spelling is under a find's directory only where both are rooted
alike, a drive included` asserts for both readings. That reading is meios's alone; no xacro reading of
it is claimed. A padded or substituted package name reaches the package: `$(find p)/urdf/c.xacro`,
`$(find p )/urdf/c.xacro`, `$(find ${sp})/urdf/c.xacro` with `sp` bound to ` p` and
`$(find ${pk})/urdf/c.xacro` with `pk` bound to `p` each read `p`'s file, and so do a find an
expression produces, `${'$(find p)'}/urdf/c.xacro`, one behind an empty span,
`${''}$(find p)/urdf/c.xacro`, and a path after the find written `//` or `/./`. A find composed with
text that is not a separator does not reach its package. `$(find p)urdf/c.xacro` names a directory
`purdf` beside the package's own: with none there it refuses under `unresolved_include`; where
`purdf/c.xacro` exists it still refuses here, because the filename is then an absolute path no rendered
directory leads, which the absolute-filename entry below covers, and xacro reads that file.
`$(find p)./urdf/c.xacro` and `$(find p)urdf/../urdf/c.xacro` refuse, and so do
`x/$(find p)/urdf/c.xacro` and `$$(find p)/urdf/c.xacro`, whose run of dollars escapes the find, each
for want of the file the text names. The find command takes exactly one package name, in an include and
anywhere else: `$(find p q)/urdf/c.xacro` and an attribute's `$(find p urdf)` refuse under
`unresolved_find`, naming the second word. A package name written as a rooted path names no package:
`$(find /opt/ws/p)` refuses under `unresolved_find`, as not resolving, in an include filename and in an
attribute alike, even where that directory is a package inside a configured root, and on Windows so
does a name opening with a drive. xacro looks a package name up in the ROS package index, which holds
no such name, so against the real index it would refuse it too; that is reasoned from the index's
interface, not run. The comparison's stand-in for that index joins the name onto its root as a path and
renders it, and no compared case writes such a name. `xacro_include_find.a find naming its package by a
rooted path names no package, even a package directory inside a configured
root` asserts meios's refusal. A command's words are separated by spaces alone, so a tab is part of a
word wherever the command is written. Written as `&#9;`, since XML reads a literal tab in an attribute
as a space, `$(find&#9;p)/urdf/c.xacro`, an attribute's `$(find&#9;p)` and an attribute's `$(arg&#9;y)`
name no known command and refuse under `unknown_substitution`, and `$(find p&#9;)/urdf/c.xacro` names a
package whose name ends in a tab and refuses under `unresolved_find`. The
`include_find_tab_before_package`, `include_find_tab_after_package`, `find_command_tab_before_package`
and `arg_command_tab_before_name` agreement probes hold these against a fresh xacro render, beside
`xacro_include_spelling.a tab beside a find's package is part of a word, so the find does not reach its
package` and `xacro_command_body.a command's words are separated by a space alone, so a tab stays
inside a word`; the literal tab read as a space was measured against the pinned xacro. A path that
climbs out of the find's package, `$(find p)/../q/a.xacro`, refuses here as escaping the source root,
the containment every package-qualified include keeps; xacro reads the sibling package's file. That is
the `include_find_climbs_out_of_package` row, a reviewed divergence, so the comparison fails if it
stops reproducing exactly as much as it fails if a new one appears. A `package://` filename is split
instead, its package text taken as written, so `package://${pk}/…` refuses here; xacro refuses every
`package://` filename. The `include_find_padded_package`, `include_find_in_literal`,
`include_find_composed`, `include_find_two_words`, `find_command_second_word` and
`include_escaped_find` agreement probes hold the padded, produced, composed, two-word and escaped
spellings against a fresh xacro render. The `xacro_include_spelling` cases pin the padded, produced,
composed, two-word and climbing spellings over a directory layer, `xacro_include_find.a find package
read from a bound property renders its package's file` the bound one, and the `xacro_dollar_run`
include cases the substituted package text and the `package://` text taken as written; the composition
onto an existing `purdf` was measured against the pinned xacro, and no case pins it. The generated
include family below varies the find's spelling, its separator and the path after it.

**A find whose package only a layer that serves bytes holds routes to that layer as the filename's
first token alone.** A memory source names no paths, so such a find has no directory to write. As the
filename's first token, followed by a separator or by nothing, it routes the rest of the filename to
the source stack as that package's file: `$(find p)/urdf/a.xacro` and `$(find p)/urdf/a.xacro` read the
memory layer's file, and a memory layer stacked above a directory layer holding the same package
outranks it. Anywhere else, after text, inside a span, beside a second find or followed by text that is
not a separator, the include refuses under `unresolved_include`, naming the package and saying that no
layer naming paths holds it. The `xacro_include_bytes` cases pin these; a find beside a second find was
measured with a library program and no case pins it. xacro has no such layer, so nothing here is
compared with it.

**An include filename's refusals fall in the order the lexer meets them.** Where every configured layer
names paths, a token after a span that does not lex refuses before the span is evaluated, a span's
inner text is substituted before its command runs, and a find's package is resolved when the lexer
reaches it, so a missing package, or a find with none, refuses under `unresolved_find` before the rest
of the filename is read: `$(find nopkg)/${nope}.xacro` and `$(find )/c.xacro` refuse for the package. A
second word refuses at the find too. Only once the whole filename is substituted is it resolved, and a
miss then refuses under `unresolved_include` and a climb out of a package under
`xacro_structural_error`. A refusal's offset is counted into the whole filename, so
`$(find p)/urdf/${x` refuses as an unterminated span at offset 15, and `$(find p)/$(arg ${x)`, whose
unterminated span sits inside a command, at offset 16. Where a layer that serves bytes is configured, a
find whose package no layer naming paths holds is decided, as the first token, only once the rest of
the filename is read: `$(find nopkg)/${nope}.xacro` then refuses for the undefined name, under
`undefined_property`, and `$(find nopkg)/a.xacro` under `unresolved_include`, naming `nopkg/a.xacro`.
Anywhere else such a find refuses when the lexer reaches it, as the passage above states. The
`include_missing_package_before_undefined_name` and `include_find_without_package` agreement probes
hold the package resolved first against a fresh xacro render, beside
`xacro_include_find.the package of a leading find is resolved before the rest of the filename is
substituted`, `xacro_include_spelling.an include filename refuses at its first refusing token, at an
offset into the whole filename` and, for a layer that serves bytes,
`xacro_include_bytes.a memory layer's find of a package it does not hold refuses as an unresolved
include`; the undefined name decided first over such a layer was measured with a library program, and
no case pins it.

**An include marked `optional` whose own file cannot be read includes nothing.** `optional` is
evaluated and then read by the boolean rule above, so `true`, `1` and `${True}` mark the include
optional, `false`, `0` and an absent attribute do not, and an empty or other value refuses under
`xacro_structural_error`. An optional include whose file is not found, is a directory, or may not be
read, or whose containment beside its document cannot be decided, includes nothing and logs one
located `level::info` note naming the filename and the reason, as `optional include "missing.xacro"
was not read: it was not found; nothing was included` does; the load renders as if the include were
absent. Every other failure refuses whether the include is optional or not: a file outside the
directories that admit it, a filename climbing above its document's directory, a package no source
holds and a find naming none, a path the reader refuses to follow because a name on it is a link or a
step leads out of the directory it is read under, a file that was opened and then failed to read or to
close, a file too large to read, and a file that does not parse. A layer that serves only bytes cannot say whether it holds a package, so a missing
`package://` file only such a layer could serve refuses under `optional` too. The mark covers the
include's own file and nothing it reads: an include missing inside a file an optional include read
refuses as any include does, deliberately, since that miss is a mistake in a file that was read.
xacro also skips that nested failure and renders the document without what the included file held,
and it refuses an `optional` written as an expression; [known
limitations](known-limitations.md#expression-evaluation) lists both differences. The
`include_optional_missing_file` and `include_optional_directory` agreement probes hold the skip against
a fresh xacro render, beside the `xacro_include_read` cases, which pin the refusals and the note.

**A glob include filename refuses here; xacro expands it.** An include filename is not globbed here, so
the pattern is looked for as a file of that name and refuses under `unresolved_include`. xacro expands
an include filename holding `*`, `[` or `?` to the files it matches, in sorted order, and includes
nothing where none matches, so `$(find p)/urdf/c.xac?o` refuses here and renders the package's
`c.xacro` under xacro, and `nomatch*.xacro` refuses here and renders without the include under xacro.
The first is the `include_glob_filename` row, a reviewed divergence, so the comparison fails if it
stops reproducing exactly as much as it fails if a new one appears; the second was measured against the
pinned xacro and no row carries it.

**`$(find)` writes a package's directory as its root was configured, except that a relative root is
written absolute and a `.` element dropped.** Through a configured root that is a link the text keeps
the link, `<…>/Rl/p`, and through a `ROS_PACKAGE_PATH` entry that is a link it keeps the spelling the
crawl found the package at; containment and the read go by the directory the link leads to. A root is
made absolute and its `.` elements dropped when it is configured, so a relative root, `R` from the
directory holding it, renders `<working directory>/R/p` here and `R/p` under xacro, and `<…>/./R`
renders `<…>/R/p` here and `<…>/./R/p` under xacro. Both are reviewed divergences, the
`find_relative_root` and `find_dotted_root` rows, each probe handing meios and xacro the same root
spelling from the same directory, so the comparison fails if either stops reproducing exactly as much
as it fails if a new one appears; the `load_package_anchor`, `ros_source` and `cli_package_path` cases
pin the linked, dotted and relative spellings.

**A relative include resolves against the document that writes it, then through the configured
sources.** A relative `<xacro:include>` filename is joined onto the directory of the file being
processed (the including document, and inside a macro body the document that calls the macro), at the
path that file was named by, so a nested include reads the file beside the include that names it, a
document opened through a link anchors at the link, and a configured root holding a same-named file
does not stand in for the file beside the document. A document a package lookup served, through
`package://` or `$(find)` and a directory, bundle or ROS layer, anchors the same way, at the path the
lookup named it by, `root/package/relative`, even where that path is a link, so its includes and
auxiliary documents read the files beside that path; the file the link leads to stays the document's
identity, as the `load_package_anchor` and `ros_package_link` cases pin. A file beside the document is
admitted where its canonical target lies under the directory the document was named in, under the one
it really lives in, or under a configured package root, which under `meios::load` are
`load_options::package_roots` and for a caller of `meios::expand` the trailing `package_roots` it
passes; with none, only the document's two directories admit. A document opened through a symlinked
install tree therefore reads the siblings that link into the directory it really lives in, and a
sibling linked into a configured root is read through that root. A document a directory, bundle or ROS
layer served is read beside the path it was named by. Only where nothing is there is the sibling asked
of the layer that served the document, and not even then where the spelling holds a `..` element: such
a miss goes to the configured sources alone, so on POSIX `sub/../c.xacro` in the document, through a
linked `sub` with nothing at `a/c.xacro`, refuses. On Windows the path interfaces reduce `sub/..`
before any link is consulted, so that spelling names the `c.xacro` beside the named path, and measured
there the load succeeds instead of refusing. A relative include in a document a memory source served, a
layer that serves bytes, is asked of that layer first, with or without a `..` element, and nothing
beside the file the layer materialized decides it, so it resolves the same whatever an earlier include
made the layer write, and only what the layer lacks goes to the configured sources. The
`load_served_layer` cases pin both, the second by `load_served_layer.a byte-served document's sibling
spelled through a parent element resolves in the layer that served it whatever an earlier include
wrote, and no lower layer's file stands in`. A file read beside a document a path-naming layer served
passes its own misses to that layer only where its spelling holds no `..` element and the directory the
document was named in admitted it, so neither a file a configured root admitted through a link nor one
spelled `sub/../b.xacro` through a linked `sub` is answered from the layer, and on POSIX the second
refuses its miss. On Windows `sub/../b.xacro` names the `b.xacro` beside the named path instead, and
measured there through `$(find)` the load succeeds instead of refusing. The `load_served_layer` and
`cli_package_path` cases pin both readings, which file the Windows loads read included. Where neither
holds the file, the filename is then looked for through the configured sources, which xacro does not
consult, so a document xacro refuses for want of that file can render here. That fallback answers a
genuine miss only: a spelling through a path element that is not a directory refuses before any
fallback even where a configured root holds the name, within the platform bound stated below for the
`..` spelling. A document expanded under no name looks for nothing beside it; xacro's library entry
anchors it at the working directory. A `..` that climbs back inside the directory, `sub/../x.xacro`, is
joined as written and judged where the operating system resolves it, so on POSIX, through a linked
`sub`, it is read where the link leads if one of those admitting directories holds that, and refused as
an outside link otherwise. On Windows the path interfaces reduce `sub/..` before any link is consulted,
so the spelling names the file beside the document whatever `sub` links to, and no link is followed:
measured there, an include of it with an `x.xacro` beside the document loads where POSIX refuses it,
and one through a link into a configured root with none beside the document refuses for want of it.
Through a path element that is not a directory, a regular file `f.txt`, `f.txt/x.xacro` refuses on
every operating system, because each element before the last is checked and one that exists and is not
a directory refuses; `f.txt/../x.xacro` refuses where the operating system reports `f.txt` as not a
directory, as a POSIX system does, and on Windows the path interfaces reduce `f.txt/..` first and read
the file beside the document. On Linux both spellings refuse, the cause reporting that the element is
not a directory, as the `xacro_include_read` and `yaml_resource` cases pin, a case per spelling. The
`..` ones, `xacro_include_read.an include spelled back out of a file that is not a directory refuses on
POSIX and reads the file beside the document on Windows, and no configured source's copy stands
in` and `yaml_resource.a spelling back out of a file that is not a directory refuses on POSIX and reads
the file beside the document on Windows, and no configured root's copy stands
in`, assert that reading on Windows, where it is measured: the include and the auxiliary document read
the file beside the document with no configured root's copy standing in, and the auxiliary document's
genuine miss still reads from the root. Two shapes still differ. Through a `sub` that does not exist
the missing tail is reduced by its spelling, and the file beside the document is read; xacro refuses
it. And where `sub` is a link and nothing is at `sub/../x.xacro` as the operating system reads it, the
miss is looked for under the spelling's lexical reduction, `x.xacro`, in the configured sources, so a
copy one of them holds can render here, where xacro refuses, and a layer that served the document as
bytes is asked for that reduction first, as above; [known limitations](known-limitations.md) gives the
measured shapes. Two lookups are refused here by default: a filename climbing above that directory with
`..` refuses as climbing above the directory of the document that writes it, and a link beside the
document whose target lies under none of the directories that admit it refuses under
`uncontained_asset`. xacro reads both. Each is a reviewed divergence, so the comparison fails if it
stops reproducing exactly as much as it fails if a new one appears.

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
renders, and the comparison holds each opted-in document to a fresh xacro render beside the default
refusal of the same document. Both are `false` on `load_options::includes`, on every `meios::expand`
call that does not pass them, and on `parse_context::includes` wherever a caller building that context
for the parser leaves it out. Only the calling program sets them: a description cannot, and the `meios`
command line exposes neither. Setting one lets a description read any file the process can open, except
that a climb in a document a layer naming no paths served stays inside that document's package, so it
is a decision about how far the description is trusted.

**An absolute include filename refuses here; xacro reads it.** Here the root is removed, the leading
separators and under the Windows reading a leading drive letter and colon with the separators after it.
Under the Windows reading, a drive that the removal of `.` and `..` leaves leading, as in
`/x/../D:/a.xacro`, is removed too and what follows it is walked again, so a `..` it uncovers counts as
a climb: `/x/../D:../q/f.xacro` refuses as escaping the source root. Only these count as the root, so a
UNC spelling, `//server/share/x.xacro`, keeps `server` and `share`, and a device spelling,
`\\?\C:\x.xacro`, keeps `?` and `C:`, as ordinary elements. The rest is looked for only through the
configured sources, so an include stays inside the roots the caller configured, and where none of them
holds it the load refuses. xacro opens an include written as an absolute path where it points. This is
a reviewed divergence, so the comparison fails if it stops reproducing exactly as much as it fails if a
new one appears.

**A `package://` include filename resolves here; xacro refuses it.** Here the spelling is split into a
package and a path within it and resolved through the configured sources, as a package-qualified
include. xacro does not read `package://` in an include filename as a package reference: it joins the
spelling onto the directory of the including document as a relative path and refuses where nothing is
there. `$(find <pkg>)/...` is the spelling both read, so it is the one to write in a description meant
to load under both. This is a reviewed divergence, the `include_package_filename` row, so the
comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**`$(dirname)` renders here; xacro refuses it.** Here it renders the directory of the file it is
written in: at the top, the parent of the document path as the load was handed it, and inside an
include the canonical directory of the file the include located, the same from any working directory.
xacro refuses it wherever it is written. This is a reviewed divergence, so the comparison fails if it
stops reproducing exactly as much as it fails if a new one appears.

**The absolute value renders here; xacro refuses it.** `${abs(-3)}` is `3` here; xacro raises an
undefined name, because its expression globals are the mathematics module and the two reducers with no
builtins beside them. It is the one difference on this page that runs in that direction for a reason
belonging to the evaluator rather than to xacro's span scanner, and it is a reviewed divergence, so the
comparison fails if it stops reproducing exactly as much as it fails if a new one appears. The function
is documented, implemented and range-checked here and a pinned description already reaches it, so it is
recorded rather than removed.

**An integer of more than 4300 digits refuses here wherever an expression computes one; xacro refuses
one only where it is written as text.** `${10**5000 // 10**4990}` renders `10000000000` under xacro,
which computes the intermediate power exactly; here that power crosses the integer ceiling and the load
halts under `expansion_budget_exceeded`. The ceiling is meios's own bound on what one expression can
demand and is kept on purpose. A result of more than 4300 digits written into the document refuses
under both. The `integer_intermediate_past_digit_ceiling` row of the divergence manifest compares the
shape through both implementations, so a change to either side fails the differential.

**A self-referential alias graph refuses here; PyYAML loads it.** A document whose anchor contains an
alias to itself — `a: &x [1, *x]` — reads under PyYAML as a value that contains itself, because PyYAML
builds a mutable node and fills it in afterwards. A value here is immutable once constructed and its
collection storage is shared by copying, which cannot denote a cycle at all: the alias arrives while
its own anchor is still incomplete, and that is where it refuses. This is a reviewed divergence, so the
comparison fails if it stops reproducing exactly as much as it fails if a new one appears.

**Two string meanings and two mapping-constructor shapes refuse here; xacro renders them.** A string
repeated by an integer and one string ordered against another both render under xacro; only the
addition of two strings and equality between two were measured into this grammar, so the multiplication
operator and the ordering operators refuse a string operand rather than taking a meaning nothing
measured. The same rule governs the split: the no-argument form collapses runs of whitespace and drops
the leading and trailing fields, which is a different algorithm from the explicit-separator form
admitted here, and a second argument bounding how many separators are consumed is a second parameter
this one operation does not take. All four are reviewed divergences, and so are the two wider mapping
constructor shapes above.

**The span scanner is quote-aware and counts nesting; xacro's is neither.** xacro finds a `${}` span's
end with a pattern that stops at the first `}` wherever it stands, and three spellings differ because
of it. `${'a}b'}` renders `a}b` here and refuses under xacro as an unterminated string literal, because
a brace inside a literal is that literal's text here. `${${'1 + 2'}}` renders `3` here and refuses
under xacro, because the close finder here counts depth and the inner scan then resolves the nested
span before the outer expression runs. And `${'%.3f' % 1.2345}` refuses here and renders `1.234` under
xacro, because only arithmetic remainder was measured into this grammar and a string operand takes no
formatting meaning. All three are recorded as reviewed divergences, so the comparison fails if one
stops reproducing exactly as much as it fails if a new one appears. Two neighboring spellings are *not*
divergences, though meios and xacro refuse them for different reasons: `${ {'a': 1} }` and
`${f'{v:.3f}'}` refuse here because neither a brace literal nor a format literal is in the grammar, and
under xacro because the truncated fragment will not parse.

**Differences that predate this grammar.** `${math.pi}` is refused here as an unsupported call target
when no include names its namespace `math`; under xacro the mathematics names are also reachable
through a `math` namespace, so it renders there. Through an include namespace a name falls through to
the global names, so `pi` and a call to a function the global table lists read: `${n.pi}`, `${n.n.pi}`,
`${math.pi}` under an include whose namespace is `math`, and `${n.sin(0)}` render. The
`include_namespace_reads_mathematics_name` agreement probe holds `${n.pi}` against a fresh xacro
render, and the `xacro_namespace` cases "a read through an include namespace falls through to the
mathematics constant" and "a call through an include namespace dispatches as the bare call does" pin
meios's renders. The other constants and the `math` head differ. Under the built-in evaluator `${n.e}`,
`${n.n.e}`, `${n.tau}`, `${n.inf}` and `${n.nan}` refuse as undefined names, as the bare `${e}`,
`${tau}`, `${inf}` and `${nan}` do; xacro renders both spellings; the
`include_namespace_reads_global_constant` row holds `${n.e}` in both directions. `${n.math.pi}` refuses
as the undefined name `math`, while the bare `${math.pi}` refuses as an unsupported call target whose
message advises the bare name, so the `math` head's two spellings refuse differently here, not alike;
xacro renders both. The `xacro_namespace.a global name other than pi refuses through an include
namespace under the built-in evaluator, each constant as it refuses bare` case pins each of those
namespaced spellings and the bare constants, and the `native_expression` case "a construct outside the
measured surface refuses by name" and the `native_member` case "a call target's refusal advises the
python evaluator only for a head that evaluator binds" pin the bare `${math.pi}`. xacro's render of
each spelling neither that probe nor that row holds was measured when this was written, and nothing
carries it. Under the python evaluator `${n.e}` renders `2.718281828459045`, which the
`eval_python_scope` case "a mathematics constant other than pi renders through a namespace under the
python backend alone" pins. xacro's argument, tokenizing and message helpers (`xacro.arg`,
`xacro.tokenize`, `xacro.message` / `warning` / `error` / `fatal`) are not exposed at all: a call to
any `xacro.` helper but `xacro.load_yaml` refuses under either evaluator, and the refusal names
`xacro.load_yaml` as the only helper either exposes. The argument helper is a reviewed divergence,
since it renders under xacro; a refusal of it also names the `$(arg)` command. `map` and `filter` are
not among the functions above, and `${list(map(...))}` does appear in real descriptions, which makes it
the omission most likely to be met in practice. Every one of these is a loud refusal naming what it
met, so it is a decision rather than a silent difference in what a document means.

## What this evidence covers

Everything this page claims about agreement with xacro rests on a comparison that runs on every push,
not on a reading of xacro's source. Three things are compared for each input: a fresh render produced
by the pinned xacro at that moment, the committed record of what xacro produced when it was last
measured, and what meios produces. A fresh render disagreeing with a committed record fails the
comparison as loudly as a meios-versus-xacro disagreement does, so a re-measurement is surfaced rather
than absorbed by rewriting the record.

What is compared, exactly:

- **The pinned xacro tooling** — `xacro` 2.1.1 and PyYAML 6.0.3, and no other distribution.
- **Universal Robots ROS2 Description at tag `4.3.1`** (archive digest `3532a25c9942…`), through five
  documents, each paired by name with its own measured record and each handed `name:=ur` beside its
  variant, because the document's root reads `$(arg name)` above the declaration:
  - the `ur5e` variant of the top-level document; that same document with its safety-limit elements
    switched on; and that same document again with its mesh references forced to absolute paths;
  - the `ur3e` variant, where a boolean read out of an auxiliary document types a joint and that type
    then reaches a limit element carrying no position keys;
  - the `ur7e` variant, whose configuration the Universal Robots description ships as a verbatim copy
    of `ur5e`'s, so it renders the same document and its record is the same record. It is measured
    because the description names the variant, not because it adds a form the other four do not reach.
- **`kuka_experimental`'s `kr6r900sixx.xacro`** — the one measured document resolving `$(find)`
  across two sibling packages.
- **A second top-level document the same Universal Robots package ships**, driven with a
  distinguishing name as well as the variant. It reaches the keyword-argument mapping constructor
  through a macro-parameter default its own call overrides, and declares a property fallback where the
  main entry point declares a value.
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
- **One hundred and ten authored expression cases beside them** — the spellings no pinned description
  reaches, named here and driven through the same minimized document: an index into a sequence,
  forwards and backwards and off both ends, a key that spells a mapping operation, the keyword-argument
  mapping constructor and every argument shape it refuses, every string meaning this grammar admits or
  refuses, every arithmetic, comparison and logical spelling including the chained comparison and the
  operand `and` and `or` yield, a loaded collection's truth in a conditional expression, the dotted
  read of a loaded key, and the mathematics names called by bare name. What each of them
  measures is not left to a reader to infer: the grammar's accepted forms are enumerated in the test
  tree, and that enumeration is paired against these rows in both directions, so a form no row measures
  fails the build and a row no form names fails it too.
- **A separate recorded table of tagged scalars** — nineteen rows covering every one of the six unit
  tags, and beside it the kinds a plain scalar resolves to, both recorded from the pinned PyYAML rather
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
  kind of block parameter. Four more hold a relative include's anchor against xacro's, one of them
  driven a second time with a configured root holding a same-named file: a file beside its document, a
  nested include, an include written in a macro body, and a document opened through a link. Two more
  are loaded under a caller's include options: the climbing document with the climb admitted, and a
  document whose included file includes a link leading out of its directory, with outside links
  followed. Twenty more hold a macro call's binding against xacro's: the thirteen `macro_call_`
  documents over undeclared attributes, surplus elements, the order between refusals and a child's
  property, `macro_default_reads_supplied_parameter`, a default reading a parameter the call supplied,
  and six over the parameter list, `macro_default_empty`, `macro_default_spans_whitespace`,
  `macro_default_argument_command`, `macro_param_spaced_separator`, `macro_params_only_whitespace` and
  `macro_params_only_whitespace_never_reached`. Twelve hold a run of dollars against xacro's lexer:
  `dollar_run_escapes_openers`, `dollar_run_before_text`, `dollar_run_at_end`,
  `dollar_run_inside_expression`, `include_escaped_find`, an include whose run of dollars escapes its
  `$(find …)`, the three `dollar_run_in_` documents, a stray run in an argument's default read, unread
  and redeclared, and four over the token after a span, the three `dollar_run_after_` documents and
  `unterminated_span_after_undefined_name`. Nine more hold a `$(find …)` against xacro's reading:
  `include_dollar_run_after_find` and `include_unterminated_span_after_find` over the token after one
  in an include filename, `include_missing_package_before_undefined_name` and
  `include_find_without_package` over its package resolved before the rest of the filename,
  `include_find_padded_package`, `include_find_in_literal`, `include_find_composed` and
  `include_find_two_words` over its package name and what follows it, and `find_command_second_word`
  over the one package name it takes in an attribute. Four more hold a command's word separation
  against xacro's: `include_find_tab_before_package` and `include_find_tab_after_package` over a tab
  beside a find's package in an include filename, and `find_command_tab_before_package` and
  `arg_command_tab_before_name` over a tab after a command's name in an attribute. Seven more hold an
  argument to the declaration the expansion reaches first: the two `argument_redeclared_` documents,
  `argument_declared_in_skipped_branch`, `argument_declared_in_uncalled_macro`,
  `argument_declared_in_include_first`, `argument_default_reads_itself_before_redeclaration`, whose
  default reads its own name before a later declaration, and `argument_above_declaration`, a use the
  expansion reaches before any declaration, refused by meios and xacro alike. The last two stand alone:
  `property_forward_reference`, a property whose value reads one written below it, and
  `include_sole_block`, a call whose one child is an include binding a property, refused by meios and
  xacro alike as leaving its block parameter without an element. Together these are ninety-eight
  documents and four second drives, the hundred and two agreement probes the grammar record does not
  hold. meios's output is what the load path builds, under the caller arguments a consumer would hand
  it, written back out by the serializer the command line flattens with; the expansion alone is held to
  the same render beside it. Two refusals agree only on their cause: the diagnostic code meios gives,
  named per document, against the code `tests/refusal_causes.h` maps xacro's exception to, so a fixture
  missing from both or a refusal for an unrelated reason fails the run.

Every comparison the run owes is named in a committed inventory, and a named comparison that produces
no verdict fails the run before any pass or fail count is reported — a gate cannot go quiet by
matching nothing. Each known divergence is listed in a reviewed manifest by its exact text, in both
directions: an unlisted divergence fails the run, and a listed one that no longer reproduces fails it
too.

**What each pinned entry point is, and what it exercises, is recorded rather than described.** A third
committed record carries one row per pinned entry point: the document, its immutable source revision,
the arguments it needs, what xacro produced, what meios produced, which constructs the load actually
exercised, and any reviewed divergence. The construct column is compared against an observation taken
from loading that document, in both directions, so a description that quietly stops exercising what it
was pinned for fails as loudly as one that starts exercising something the row does not claim. All
sixteen rows record agreement — on the result and on the whole normalized rendered document — and that
empty divergence column is asserted empty rather than ignored. Two things fall straight out of the
record: exactly one pinned description resolves a duplicate key, and no Universal Robots or KUKA entry
point reads a mapping by a member name.

The comparison against a fresh xacro render runs on Linux, where the pinned xacro tooling is installed
and run — and so does the corpus tier that feeds it. What macOS and Windows fetch the corpus for is a
narrower thing: on each of the three, a consumer outside this project loads two of the pinned documents
— the `ur3e` variant and the KR6 — natively and checks them against the same recorded facts. So
agreement with xacro is established on one platform, and of the pinned documents exactly those two are
established to resolve to the same facts on all three. The rest are established on Linux.

That is the whole of it. The claim extends no further than these documents at these revisions, these
expression forms and the input space the next section states. Another vendor's authoring style, another
revision of the same description, or an expression form the closure above does not contain is covered
by nothing on this page, and [known limitations](known-limitations.md) is where the rest of what is not
yet proven is written down.

## The input space the comparison was measured over

**That every difference from xacro is either fixed or recorded holds over a stated input space, and is
not claimed beyond it.** Inside it, a disagreement fails the comparison unless a reviewed row records
it, and a recorded one that stops reproducing fails it too. A difference outside it is not claimed
absent, whoever finds it. The space has four parts, each counted on the tree this page describes:

- **The pinned corpus documents**: the sixteen entry points above, each compared whole against its
  record and a fresh xacro render, the `document` rows of
  `tests/golden/oracle/differential_inventory.cases` and the rows of `compatibility_ledger.cases`.
- **The expression cases**: one hundred and thirty-three, the twenty-three minimized and one hundred
  and ten authored cases above, which are the `expression` rows of the inventory.
- **The committed probes**: two hundred and sixty-two agreement probes, the `agreement` rows of the
  inventory, one hundred and two of them the documents the enumeration above names and one hundred and
  sixty the grammar record's, each held to a fresh xacro render. An expansion-only agreement probe
  compares a construct the robot model does not carry on the whole canonical expansion. Beside them are
  the probes whose difference the reviewed manifest `differential_divergences.cases` records,
  sixty-seven rows of it: thirteen document probes and eleven expression probes listed in
  `tests/tools/oracle/differential_probes.py`, and the grammar record's forty-three document probes, a
  group listed in `tests/tools/oracle/grammar_probes.py`. A probe meios and xacro both refuse agrees only on the same
  cause: xacro's exception class and message stem, mapped through `tests/refusal_causes.h` to the code
  meios gives for that cause, and a xacro refusal no row maps fails the run. The expression cases and
  expression probes still compare a refusal by refusal alone, because they measure the `${}` surface,
  whose evaluator reports that it failed and no code.
- **The generated set**: a committed grammar, `tests/tools/oracle/recorder/generated_families.py`, from
  which `generated.py` draws two hundred cases in each of three families from one splitmix64 sequence
  seeded `0x6d65696f73`, six hundred in all: include filenames, `$` shapes in an attribute, and macro
  parameter lists with the calls that bind them. The include family varies the text before a find, the
  find's package spelling (padded, substituted, missing, empty, two words, produced by an expression,
  held in a property or an argument, or no find at all), the separator after it, the path after that
  and the text after the path, a trailing separator among it; the span family strings together one to
  six atoms among spans, the `arg` and `find` commands, runs of dollars, braces, parentheses and plain
  text; the macro family varies a parameter's name, its form and its default, and the call's children.
  A call names the parameters its macro declares, each with odds of three in four, and one undeclared
  name with odds of one in eight, and the document reads the property `b` again after the call, so a
  parameter named `b` that shadows it must leave with the call for the two renders to agree. The pinned
  xacro's verdict for every case is recorded in `tests/golden/oracle/generated_cases.cases`, whose
  header carries the seed, the count per family and the template each family's text is written into,
  and `generated_differential` compares meios's verdict with that record on every run. A verdict is the
  cause of a refusal or, where the document rendered, the whole rendered document, read from both tools
  into the project's own canonical text (`meios::canonical_xml`: attributes sorted, insignificant
  whitespace collapsed, comments and processing instructions dropped) after the staged paths are
  normalized, so every element and attribute is compared. Two refusals agree only on the same cause:
  xacro's exception class, which the record's cause column carries beside the refusal's first line, and
  its message stem, mapped through `tests/refusal_causes.h` to the code meios gives, the same rule the
  probes follow. A xacro refusal no row maps fails the run. Nine cases disagree, and
  `tests/golden/oracle/generated_divergences.cases` records each as a reviewed row with its reason:
  five where a parent element after a find leaves the find's package, the difference the
  `include_find_climbs_out_of_package` row records; one where a find held in an argument makes the
  filename absolute, the difference the `include_absolute_path` row records; one where a separator
  after the file a find names is read here, and xacro refuses it as not a directory; and two where an
  element missing before a parent element beside the document cancels by its spelling here, one of them
  then found through the configured sources. [Known limitations](known-limitations.md) discloses all
  four.

**The grammar these parts are held to is derived from the pinned xacro's own source, and the build
pairs each of its constructs with what carries it.** `tests/golden/oracle/grammar_constructs.cases`
lists ninety-seven constructs, found by walking the pinned xacro's syntax tree rather than listed by
hand: one per element branch, attribute list, lexer token, substitution command, special form, raise
site and warning site. `grammar_constructs` pairs each with what carries it, in both directions: a
construct no pairing names fails the build, and so does a pairing whose construct the record no longer
lists or whose key its record does not hold. A carrier is an agreement probe, a corpus document, a row
of the reviewed manifest, a generated case, an expression case or a unit case, and the standing a
pairing states is checked against it; a site no document reaches is paired with that evidence instead,
and its standing stays open. A construct said to agree points only at a measurement on which both
implementations agree, or at a unit case; a recorded difference points at a manifest or generated row,
and is said to refuse here exactly where that row's column for meios reads `REFUSED`; a reviewed
difference points at a row the manifest keeps, whatever its columns read. Counting each construct by
its least settled pairing, an open difference outranking a reviewed one and either outranking
agreement, eighty-five are measured: fifty-six agree, four of them warning sites a unit case carries
alone, since the comparison compares no warning, two of which meios raises and two it does not;
thirteen have an open recorded difference on which meios renders something else; nine have open
recorded differences only where meios refuses; and seven have reviewed differences alone. The seven constructs of the `${}` expression surface and the command-line driver are paired with
the boundary rather than measured here. Five sites, four raise sites and one warning, are reached by no
document under the pinned install; `grammar::unreached_sites` pairs each with the reason. The raise for
a substitution the install cannot support is not among them: the pinned install ships no package index,
so every find reaches it there, and the `find_without_package_index` probe is rendered without the
index the oracle vendors.

**Most generated include and macro cases test agreement on a refusal, not on a rendering.** Of the two
hundred cases in each family, xacro renders seventeen include cases, eighty-four span cases and
eighteen macro cases, and refuses the rest. In the include family every non-empty text after the path
refuses, a trailing separator among it, as do a missing package, an empty find, a find given two words,
a missing file and an undefined name in the path, and of the cases that hold a find in a property or an
argument only one renders. In the macro family the one hundred and eighty-two refusals are thirty-eight
calls holding fewer children than the block parameters declared, sixty undefined names, thirty runs of
`$` that escape no opener, twenty-nine children no block parameter takes, twenty-one call attributes
naming no parameter xacro reads as declared, nineteen of them the undeclared name, and four forwarded
defaults with nothing to forward. Every case that declares `b` twice is among the refusals, so no case
xacro renders reads `b` after a call that bound it twice. A render here where xacro refuses is a silent
divergence, so agreement on a refusal is a real test, but these two families reach few verdicts on
which file an include read or what a parameter bound. The span family is the one that compares
renderings in number.

**What the generated set leaves out, and what carries it instead.** The grammar writes no glob
character into an include filename; the `include_glob_filename` document probe carries one, under its
manifest row. It writes no `package://` filename, which the `include_package_filename` row records, and
no literal absolute path to a file that exists; the absolute filenames that name one are finds held in
a property or an argument, which the `include_absolute_path` row records, and the few filenames it
starts with a separator name nothing and refuse for both. It writes none of the `eval`, `dirname`,
`env`, `optenv` and `cwd` commands. The `eval_` agreement probes compare `eval`, three of the
`command_` agreement probes compare `optenv` over an unset variable, and the `dirname_command` row
records `dirname`. The `env_variable_unset` agreement probe and the `env_command_two_words` row compare
`env`, and the `cwd_command` row compares `cwd`: meios refuses `$(cwd)` under `unknown_substitution`,
and xacro renders it as the working directory.

**What the generated comparison holds fixed, and what carries each instead.** It runs one directory
layer under `eval_policy::fail`. A layer that serves bytes has no counterpart in xacro, so no case of
it can be compared there: the `xacro_include_bytes`, `xacro_include_served` and `load_served_layer`
cases carry it, against meios's own rule. A stack of more than one layer is carried by the
`load_served_layer` and `xacro_include_precedence` cases. The lenient policies, `warn` and `skip`, are
carried by the `xacro_policy_prop` property, which holds that an expansion logging an error under
either returns no document, and by the `xacro_lex_lookahead` cases. None of these makes a claim of
agreement with xacro. It runs the core evaluator alone: the cases whose names begin `eval_python` carry
the python evaluator. Two of them, `eval_python.python evaluator matches the golden CPython
corpus` and `eval_python_unrestricted.both evaluators render every golden corpus row
identically`, compare it with the text CPython's `str` gives for each expression, as recorded in
`tests/golden/expr/arithmetic.cases`, `tests/golden/expr/functions.cases` and
`tests/golden/expr/precedence.cases`; every other case compares it with meios's own rule, and none with
a render of the pinned xacro. It passes no caller argument: the three agreement probes driven a second
time with one, `property_default_over_caller_argument`, `property_default_reads_caller_argument` and
`eval_string_caller_argument`, compare that with xacro. Its include is written only in a top-level
document no layer served, so neither served-sibling route is compared: the `xacro_include_served` and
`load_served_layer` cases carry those, against meios's own rule. Its macro family defines one macro and
calls it once, at top level: the corpus documents, each compared whole, call macros from macro bodies
and from included files, as `ur_description`'s `ur_robot`, defined in an included file, calls
`cylinder_inertial` from its body. The record holds no machine path, so the comparison runs in every
test tree on every platform; so far it has run on Linux alone.

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
root holding the file and, with no configured root, refuses, as the `load_beside_root` cases pin; xacro
reads that file. A spelling through a path element that is not a directory, `f.txt` a regular file,
refuses as it refuses there, and no configured root's copy stands in: `f.txt/x.yaml` on every operating
system, and `f.txt/../x.yaml` where the operating system reports `f.txt` as not a directory. Windows,
whose path interfaces reduce `f.txt/..` first, reads the file beside the document, with no configured
root's copy standing in, and still reads a genuine miss from the root, as measured there. The two
`yaml_resource` cases, one per spelling, pin it, and the `..` one,
`yaml_resource.a spelling back out of a file that is not a directory refuses on POSIX and reads the
file beside the document on Windows, and no configured root's copy stands
in`, asserts that reading on Windows. A link beside the document leading under none of those
directories refuses under `uncontained_asset` with no configured root's same-named copy standing in,
unless the caller sets `include_options::follow_outside_links`. A followed link's answer is final: a
link to nothing, a dangling one included, ends the lookup as unreachable, and a link naming a directory
refuses under `cannot_open`, with no configured root's copy read in either place. A `package://` spec
naming a link inside a configured root is read through the root, and one naming a link that leads out
of the root refuses, with an `uncontained_asset` record; a `$(find)` spelling reaches this reader
already substituted to a path and is judged where it lands. [Known limitations](known-limitations.md)
gives the measured shapes.

**Which document a relative spec resolves against.** The document it is written in — which is not
always the top-level one:

- Inside an included file, that included file. A spec whose text is a bare `inner-config.yaml`, bound
  in a file pulled in by `<xacro:include>`, resolves next to *that* file, not next to the description
  that included it.
- Inside a macro body, the document that **invoked** the macro, not the one that defined it. This is
  not obvious from reading the macro: a macro that loads a configuration sitting beside its own
  definition will not find it when called from elsewhere.
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

A mapping or a sequence that came back from `xacro.load_yaml` is a value like any other: it binds to a
property, passes as a macro argument, and is read by subscript. A mapping also answers a dotted read,
so `${config.joint_limits}` and `${config['joint_limits']}` reach the same key; a sequence is read by
index only, counting back from the end for a negative one, so `${links[-1]}` is the last element of a
`links:` list. Both spellings of a name that spells a mapping operation are covered by the dotted rule
above.

**What a document may say, and how it reads.** An anchor and the alias that names it denote the same
subtree, so a configuration that writes one inertia and reuses it reads the same both places. A merge
key flattens the mapping it names into the one that carries it, and a key the carrying mapping also
writes wins over the merged one. A key written twice resolves to the first position and the last value,
which is a rule a real robot description reaches: one auxiliary document in the measured corpus writes
a key twice. And a key need not be text — a document may write a number, a boolean or an empty key, and
it loads. Two such keys are the same key by one rule: the numeric kinds compare by value across their
whole range (`1` and `true` are one key), while text and a null compare only within their own kind
(`'1'` and `1` are two). What a non-text key does *not* have is a way to be read: every subscript key
is text, so such an entry loads, counts and can collide, and no expression reaches it.

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
through `exercised`. Caller arguments handed to `expand` directly are bound with `arguments().set` on
the `eval_scope`, where `$(arg n)` reads them, while the scope's own `set` binds a property, which an
expression reads. A spelling outside the vocabulary resolves to nothing, and nothing is charged
against the record: it is an observation, not a twelfth ceiling. The other arm is an
`expansion_error` carrying the failing location, a `diagnostic_code`, a message and — where a refused
system operation caused it — that operation's native cause. A terminal failure is reported exactly
once through the sink you supplied and returned on the error arm, so you neither log it again nor
risk seeing it twice. Warnings, the environment-read note and every other nonterminal diagnostic keep
travelling that same sink, unaffected. A span the active policy declines to resolve is a **success**
whose span is left verbatim; [what refuses, and by which rule](#what-refuses-and-by-which-rule) is
what decides which failures are terminal at all. `meios::substitute` publishes the same two arms over
the same record.

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
Python behavior the grammar above does not carry — a comprehension, an f-string, a string method other
than the named split, a method call on a loaded mapping, string repetition or ordering, or a unit tag
written over something other than a numeric literal: it registers the same six tags and evaluates the
tagged text as an expression. It drives a *found* (never fetched) CPython, 3.11 or later. The
interpreter it starts puts no program or working directory on its module path and drops every
empty element of `PYTHONPATH` (a leading, trailing or doubled separator), which Python would
read as the working directory. Every directory `PYTHONPATH` names stays on the module path as
Python keeps it, and a relative one, `.` included, is read against the working directory, so
a module file beside where the program was started is imported only when `PYTHONPATH` names
that directory. The interpreter installs none of Python's signal handlers, so SIGINT keeps
the disposition the program gave it. Starting it leaves the program's locale as the program set it
and exports no locale variable into the environment, and on Windows it leaves the standard streams
in the mode the program gave them. It runs in Python's UTF-8 mode, so text Python decodes or encodes
without naming an encoding is UTF-8 whatever the locale. A program that already runs Python keeps its
interpreter: the backend uses it as it finds it, module path, integer digit limit and signal handling
included, never finalizes it, and takes the interpreter lock for each evaluation, so the program must
not hold that lock on another thread while a load runs. An interpreter that cannot start makes every
Python evaluation of the process refuse as an `error` naming the failed start, and the start is not
tried again. Its default class, `meios::python_evaluator`, is restricted: it evaluates a documented
subset and refuses anything outside it with a `file:line` diagnostic naming one of four rules —
`dunder-identifier`, `format-traversal`, `non-allowlisted-builtin`, `uncontained-yaml-path`. Its
sibling `meios::unrestricted_python_evaluator` applies none of those rules and evaluates whatever
CPython evaluates, `${__import__('os').getcwd()}` included.

**Under both classes a name is read when Python evaluates it**, at any depth of the expression: the
expression is evaluated over a mapping that reads the scope as its globals, so a name inside a lambda,
a generator expression or a comprehension is read the way one at the top level is. A property is
evaluated only if the expression reads it: `${'x' if c else b}` with `c` true never evaluates `b`,
whatever `b`'s own text would do. A name is looked up in the scope first, then among the mathematics
names and the two spellings of the resource helper, then among the builtins, so a property shadows a
mathematics name of its spelling: a property `pi` written `3` reads `3`, as it does under the built-in
evaluator. The restricted class's guard asks the scope about the name Python reads, after Python
normalizes it, so a property shadows a withheld builtin under any spelling Python normalizes to its
name: with a property `open` bound, `${ｏｐｅｎ}` renders it, which the `eval_python_scope` case "a
property shadows the withheld builtin its name normalizes to, as the guard reads the name Python reads"
pins. A property whose own text refuses is never replaced by the mathematics name or builtin of its
spelling; the read refuses, carrying the one record that text's evaluation logged. An include namespace
is read through a dot, `${n.w}`, falling through to the names of the file that wrote the include and
then to the names a bare read reaches, as under the built-in evaluator, so `${n.pi}` reads the constant
and `${n.sin(0)}` calls the function. Those names are this backend's, so `${n.e}` renders
`2.718281828459045`, and `${n.abs(-2)}` renders here as the bare call does, where xacro's global table
holds no `abs` and refuses it; a member no name answers raises `NameError: nothing in scope binds 'w'`,
which suggests no argument command, as under the built-in evaluator. A property's text evaluated at its
first read under this backend is charged against the expression-depth ceiling as the built-in evaluator
charges it, and a circular definition refuses the same way. Each read this backend makes carries a
nesting of nine, for the interpreter's frames that stay on the stack beneath the evaluation the read
starts, so each level is charged that beside the nineteen a deferred read costs: a chain of seven
properties each reading the next inside arithmetic renders under it, where six do under the built-in
evaluator, and one longer refuses with `expansion_budget_exceeded`. Measured on a 512 KiB thread with
the ceiling lifted, the interpreter's own parser refuses such a chain of forty-nine properties, before
the thread runs out. The `eval_python_scope` cases carry all of this, and run each namespace document
under the built-in evaluator as well.

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
directly on arbitrary bytes, asserting that no input makes it invoke undefined behavior; a loud failure
result is an acceptable outcome there and a sanitizer trap is not. That is a genuine property of a
parser that must not crash on hostile-looking input, and it says nothing about whether the grammar
admits the right forms. The proof of *that* is the table-driven suites and the comparison above:
`tests/golden/oracle/expressions.cases` states every measured expression, its xacro rendering and its
verdict, and `tests/unit/native_expression_test.cpp` and
`tests/integration/native_differential_test.cpp` run that table through the evaluator seam and through
a full document expansion.
