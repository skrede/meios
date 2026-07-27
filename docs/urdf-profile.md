# The URDF profile: what meios reads and what it refuses

This page is the contract for the URDF document meios reads. It describes the document **after
expansion** — the plain XML that remains once every macro, property, substitution and include has been
resolved. Everything a description may run before that point, and the rules that refuse the rest,
belong to [evaluation](evaluation.md); nothing on this page restates them. If you are reading to find
out what `${…}` may evaluate to, you are on the wrong page.

Each rule below says which construct it governs, what meios does when a document violates it, and the
diagnostic code the refusal carries so you can branch on it rather than on message text.

## Authority

The URDF XML specification meios follows is the ROS wiki, in the revision cited here. The live wiki
serves a proof-of-work challenge to non-browser clients, so a reader — or a build — may only be able to
reach an archived snapshot; the revision date, not the URL alone, is therefore the durable citation.

| Page | Last edited |
|---|---|
| <https://wiki.ros.org/urdf/XML/robot> | 2019-10-11 |
| <https://wiki.ros.org/urdf/XML/link> | 2022-04-19 |
| <https://wiki.ros.org/urdf/XML/joint> | 2022-06-17 |

Those three pages are the whole of the specification. They are also silent on a great deal: they state
which fields are required without saying what a parser should do when one is missing, they state some
defaults and not others, and they say nothing at all about several constructs every real description
contains.

**Where the wiki is silent, meios decides on its own authority, and records the decision on this page.**
The default answer to silence is a loud refusal rather than a guess: a document meios cannot read
without inventing a value is refused with a `file:line` diagnostic, not completed to something
plausible. Silence is never read as permission to fabricate.

## Where this profile decides on its own authority

Every rule meios enforces that the wiki does not state is recorded in this section, with the reasoning
that justifies it, so a reader can tell a specification requirement from a meios decision.

## Where this profile diverges from a stated default

Where the wiki states a default and meios deliberately does something else, the divergence and its
reason are recorded in this section rather than left for a reader to discover from behavior.

## Rules

| Element | Construct | Verdict | Diagnostic code | Rule id |
|---|---|---|---|---|
| `<origin>` | the `xyz` and `rpy` attributes | refuse | `vector_arity` | `rule:origin-xyz-arity` |

**A fixed-length numeric attribute must carry exactly its own number of components.** The wiki
describes `xyz` as "the x, y, z offset" and `rpy` as the roll, pitch and yaw angles about the fixed
axes — three components each — and says nothing whatever about what a two-component or a
four-component string is supposed to mean. There is no reading of a two-component offset that does not
invent the missing number, and no reading of a four-component one that does not discard an authored
one. Both are documents meios cannot read, so both are refused. An **absent** `<origin>`, or an absent
`xyz` or `rpy` attribute, is not a violation: the wiki states those are optional and states their
defaults, and meios keeps them.

The same reader enforces the same count on every other fixed-length numeric attribute in a
description. Those constructs get their own rows on this page as the profile is completed; until a row
exists here, treat only the row above as published.

Before this rule, meios did what the parsers it supersedes do: a two-component offset was completed
with a third zero, a four-component one was truncated to its first three, and both **silently**, with
no diagnostic at any level. A description with a typo in an offset loaded clean and placed a link
somewhere its author never wrote. That is the behavior this rule exists to end.
