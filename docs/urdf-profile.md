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

**Unknown attributes are dropped with a diagnostic that names them.** The wiki never mentions unknown
attributes on any of its three pages, so nothing about them is a specification requirement. meios names
them because the alternative is a worse diagnostic: `<joint name="shoulder" tpye="revolute">` produces,
in a parser that ignores unknown attributes, only a complaint that the joint has no type — which sends
the author looking at the wrong thing. Naming `tpye` puts the cause beside the effect.

**A `version` attribute on `<robot>` is recognized, and only one value is accepted.** The wiki does not
mention the attribute at all. meios accepts its absence and accepts the single value `1.0`; any other
value is refused. A document declaring another version asserts a format this library does not
implement, and reading it as `1.0` anyway would be a guess about what the author meant. An accepted
value is recorded on the robot record, so a description that declared `1.0` stays distinguishable from
one that said nothing, and a bundle round trip does not invent the attribute for a document that never
carried it.

## Where this profile diverges from a stated default

Where the wiki states a default and meios deliberately does something else, the divergence and its
reason are recorded in this section rather than left for a reader to discover from behavior.

## Rules

| Element | Construct | Verdict | Diagnostic code | Rule id |
|---|---|---|---|---|
| `<origin>` | the `xyz` and `rpy` attributes | refuse | `vector_arity` | `rule:origin-xyz-arity` |
| any | a child element the profile does not describe | drop | `unknown_element` | `rule:unknown-element` |
| any | an attribute the profile does not describe | drop | `unknown_attribute` | `rule:unknown-attribute` |
| any | a `<gazebo>`, `<ros2_control>`, `<transmission>` or `<sensor>` block | drop | `extension_ignored` | `rule:extension-disclosed` |
| `<robot>` | the `version` attribute | accept `1.0`, refuse anything else | `unsupported_version` | `rule:robot-version` |

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

**Content this profile does not describe is dropped, and the drop is said out loud.** An element or an
attribute meios does not recognize is not read into the model, and a warning names it at its own
`file:line`. This is one rule at every level of the document: under `<robot>`, inside a `<link>`, and
inside a `<joint>` alike. Dropping is disclosure rather than fabrication, so it never fails a load, and
the diagnostic is emitted whatever the document-validity setting says: that setting grades the
XML-hygiene classes, and this is not one of them. What the drop does cost is the claim — the result
stops saying it was parsed whole, so a consumer can branch on that instead of scraping a log. Nothing
inside a dropped
subtree is judged further: content this profile does not describe is content it has no vocabulary to
name, and reporting every node beneath an unrecognized element would say the same thing many times.

A hand-maintained list of tolerated unknown elements was deliberately not built. Any such list is a
false refusal waiting for the first description that carries something nobody thought to add to it.

**The four recognized extension blocks are disclosed under their own code and keep the parse claim
standing.** `<gazebo>`, `<ros2_control>`, `<transmission>` and `<sensor>` are dropped exactly like any
other content meios does not read, and the diagnostic still names them — but it carries
`extension_ignored` rather than `unknown_element`, and that code clears nothing. The wiki explicitly
permits this: of custom elements inside a `<link>` it says "A URDF will **ignore** these custom
elements … and your particular program can parse the XML itself to get this information." Nearly every
shipped robot description carries a simulation block, so a parse claim cleared by them would be clear
for every real robot and would tell a consumer nothing.

**What the parse-completeness claim means.** A successful load reports `parsed` when meios read
everything the document contained. It is cleared by exactly the outcomes above that drop or refuse
authored content: an unrecognized element, an unrecognized attribute, an unsupported version, a
refused fixed-length numeric attribute, and the XML-hygiene classes. It is **not** cleared by a
disclosed extension block, and it is not a statement about the robot: a document may be parsed whole
and still describe a topology meios refuses, or reference a mesh that is not on disk. Those are the
two other claims, and the three are independent facts rather than a ranking.

An XML namespace declaration — `xmlns`, or any `xmlns:` prefix — is XML infrastructure rather than
description vocabulary and is never reported as an unknown attribute.

**This profile discloses more than the parser it supersedes.** urdfdom ignores both unknown elements
and unknown attributes in complete silence, at every level, and hands back a model that looks
complete. Everything above is meios choosing to say what it did not read.
