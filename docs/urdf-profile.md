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

**A link name must be present and must be unique.** The wiki states that a joint's name must be
unique — "the name of the joint, must be unique" on the joint page — and says nothing of the kind
about links, and nothing anywhere about what an absent or blank name means. meios requires both of a
link because everything that reads a model reaches it by name: two links sharing one, or several
sharing the empty string an absent attribute yields, makes every reference to that name ambiguous and
the choice of which one wins an implementation detail. There is no reading of a duplicate that does
not silently pick one link and discard the other.

**A robot-level `<material>` must carry a name, and no two may share one.** The wiki does not state
this either. A robot-level material exists in order to be referenced by name, so one without a name
can never be reached, and two with the same name make a reference name two different colors. Both are
refused. A material *defined inline* under a `<visual>` is unaffected: it carries its color or its
texture with it and needs no name to be read.

**A document declaring no `<link>` is refused.** The wiki never addresses whether an empty `<robot>`
is a robot. meios refuses it: a description with no links describes no body, every consumer of the
resulting model would immediately find nothing in it, and refusing at the document says so at a
`file:line` rather than leaving the consumer to discover an empty model.

**A `<material>` under a `<visual>` that carries no name and defines neither a color nor a texture is
refused.** The wiki does not mention the construct. It is neither a reference — there is no name to
resolve — nor a definition, so there is nothing meios could read from it, and accepting it silently
would leave the visual with a material the author appears to have specified and meios in fact ignored.

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
| `<link>` | the `name` attribute | refuse when absent, empty or blank | `empty_name` | `rule:link-name-required` |
| `<joint>` | the `name` attribute | refuse when absent, empty or blank | `empty_name` | `rule:joint-name-required` |
| `<link>` | a name a second link already carries | refuse | `duplicate_name` | `rule:link-name-unique` |
| `<joint>` | a name a second joint already carries | refuse | `duplicate_name` | `rule:joint-name-unique` |
| `<material>` | a name a second robot-level material already carries | refuse | `duplicate_name` | `rule:material-name-unique` |
| any | two names differing only in case or in surrounding whitespace | accept as two names | — | `rule:name-exact-bytes` |
| `<parent>`, `<child>` | a `link` naming a link the document does not declare | refuse | `undeclared_link` | `rule:link-reference-declared` |
| `<parent>`, `<child>` | the `link` attribute | refuse when absent, empty or blank | `empty_name` | `rule:link-attribute-required` |
| `<mimic>` | the `joint` attribute | refuse an undeclared target and a self-reference | `dangling_mimic` | `rule:mimic-target-declared` |
| `<robot>` | a document declaring no `<link>` | refuse | `no_links` | `rule:robot-has-links` |
| `<visual>` | a `<material>` that neither names one nor defines one | refuse | `empty_name` | `rule:visual-material-named` |

**Identity and reference integrity are refused at every policy setting.** The rules in the eleven rows
above are structural: they are properties of the document text rather than of the graph that text
describes, and no value of the document-validity policy or of the graph policy softens any of them. A
malformed identity or a reference to something that was never declared is exactly the input that turns
into plausible physics if it is read anyway, and refusing is strictly better than guessing which link
the author meant. The permissive settings govern decorative and physical detail — visuals, collisions,
materials, inertials — never identity and never references.

The graph policy therefore governs only what is genuinely a property of the assembled graph: how many
roots the links form, whether they contain a cycle, and whether a link has more than one parent joint.
Setting it permissively can no longer reopen a dangling link reference, which is what it used to do.

**Names are exact bytes.** A name is the attribute value as authored. meios never trims surrounding
whitespace, folds case, or normalises a name before comparing it, so `Base_Link` and `base_link` are
two links, and `base_link` and `base_link ` are two more. Any folding would invent an equivalence the
document never stated and would begin refusing descriptions the reference parser accepts. A name
containing internal whitespace is a legal name and is left alone. What is refused is a name that is
absent, empty, or nothing but whitespace: such an element has no identity, several of them silently
share one, and a joint cannot reference any of them.

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

The reference parser also refuses a blank name, a duplicate name and a dangling link reference, so
every identity rule on this page agrees with it. That agreement is **corroboration, not authority**.
This profile follows the wiki, and where the wiki is silent it decides for itself and records the
decision in the own-authority section above; the reference parser is an instrument for reading what
real descriptions rely on, never a specification meios defers to. Where it happens to agree, the rule
would stand without it.
