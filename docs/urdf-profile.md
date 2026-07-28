# The URDF profile: what meios reads and what it refuses

This page is the contract for the URDF document meios reads. It describes the document **after
expansion** — the plain XML that remains once every macro, property, substitution and include has been
resolved. Everything a description may run before that point, and the rules that refuse the rest,
belong to [evaluation](evaluation.md); nothing on this page restates them. If you are reading to find
out what `${…}` may evaluate to, you are on the wrong page.

The same split applies to assets. The rules below govern whether a `<mesh>` or a `<material>` element
is well formed — whether a `filename` is present, whether a `scale` has three components — and stop
there. What the string inside that `filename` may name, what a relative one is measured against, what
meios refuses to reach and how long a resolved path stays valid belong to
[asset resolution](asset-resolution.md), and are not restated here.

Each rule below says which construct it governs, what meios does when a document violates it, and the
diagnostic code the refusal carries so you can branch on it rather than on message text. The rule
table is at the end; ahead of it are the specification meios follows, the failure this profile exists
to prevent, the frame and unit conventions the numbers obey, every rule the specification does not
state, and every place this profile departs from something it does.

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

## What this profile exists to prevent

A reader that completes what a document did not say produces a robot nobody wrote, and reports
success. That failure has a shape worth naming: the defect never surfaces where it was authored, and
what reaches you is either a plausible-looking model or a complaint about something else entirely.

Two links sharing a name is the clearest illustration. Read permissively, the second declaration is
simply another link — so the model gains a body no joint attaches, and the first sign of trouble is a
topology complaint that the robot has an additional root. That complaint is true and useless: the
defect is a duplicated name several lines earlier, and nothing in the output points at it.

`tests/fixtures/urdf/profile/coercion_probe.urdf` is the other half of the same story, and it is kept
in the repository for exactly that reason. It carries an unsupported version, an unknown attribute on
`<robot>`, a two-component offset, an empty mass, a negative inertia diagonal, an unknown geometry
shape, an unknown child inside a link, a misspelled `type` attribute, a four-component offset, a zero
axis, and a mimic naming a joint that does not exist. Before the rules on this page existed, `meios
validate` printed `validation: OK (exit 0)` on it and `meios info` reported one rigid connection and
zero degrees of freedom, with no diagnostic at any level. It is now refused, and the refusal names a
defect at its own `file:line`.

A load stops at the first class of defect it refuses rather than reporting every one: identity and
reference integrity are settled before a field is read, and a field is not read out of an element
whose identity is already wrong. Repairing a document that broken is therefore iterative. Every rule
this page publishes is proven on a document of its own, one defect at a time; that one is kept whole
because what it demonstrates is the silence, not any single rule.

## Frames, units and composition

These are statements about what the numbers in a document mean, not rules a document can violate, so
none of them appears in the rule table below. They are here because a consumer that reads them wrong
places a link somewhere the author never wrote, and no diagnostic can catch that.

**Positions are in metres and angles are in radians.** The joint page states both outright: of
`<origin>`'s `xyz`, "All positions are specified in metres"; of its `rpy`, "All angles are specified
in radians" (rev. 2022-06-17). meios neither converts nor scales: an attribute reaches a consumer as
the number the document authored.

**A joint's `<origin>` is the child frame expressed in the parent.** The joint page says it plainly:
the origin "is the transform from the parent link to the child link" (rev. 2022-06-17). The joint
frame is at that transform from the parent's frame, and the child link's frame coincides with it. An
`<origin>` under a `<visual>`, a `<collision>` or an `<inertial>` is the same kind of quantity one
level down — the element's own frame expressed in the frame of the link that contains it (rev.
2022-04-19).

**`rpy` is a fixed-axis composition, roll then pitch then yaw.** The joint page, verbatim:
"Represents the rotation around fixed axis: first roll around x, then pitch around y and finally yaw
around z" (rev. 2022-06-17). Because the axes are fixed rather than carried along by the preceding
rotations, the equivalent matrix product is

```
R = Rz(yaw) * Ry(pitch) * Rx(roll)
```

applied to a column vector on the right. The reverse product is a different rotation, and reading the
sentence as a moving-axis composition produces exactly that reverse — which is why the worked values
below include a row where the two orders disagree in six of nine components.

**The inertia tensor is expressed in the inertial element's own frame.** The link page places
`<inertial><origin>` at "the pose of the inertial reference frame, relative to the link reference
frame", with its origin "at the center of mass of the link" and its axes those of the principal
frame (rev. 2022-04-19). The six components meios stores are therefore about that frame, not about
the link's frame; a consumer that wants them about the link's origin applies the parallel-axis
theorem itself, using the origin the record carries.

**meios composes nothing.** `rpy`, `transform` and `inertia` are stored as the plain numbers the
document authored, and no part of this library multiplies, converts or normalizes them — there is no
rotation conversion in the interface and none is planned for it. That is deliberate: the consumer
owns kinematics, and a conversion here would be a second implementation to keep honest. What this
page ships instead is worked evidence.

`tests/golden/urdf/rpy_reference.cases` carries a set of `rpy` triples with the rotation matrix and
the unit quaternion each one denotes under the convention above, at full double precision, one row
per line with a self-describing header. Point your own implementation at it: if your matrix matches
every row, you have the same convention meios documents and the reference parser implements. The
quaternion column follows the reference parser's `setFromRPY`, so the two representations in a row
are the same rotation and a consumer may check against whichever it computes.

## Where this profile decides on its own authority

Every rule meios enforces that the wiki does not state is recorded in this section, with the reasoning
that justifies it, so a reader can tell a specification requirement from a meios decision.

**What a violated "required" does is decided here, because the wiki only ever marks a field
required.** Nothing on the three pages says what a reader should do about a violation. meios refuses
the document under the default document-validity setting and drops the containing element under the
permissive ones; the exact shape of that drop, and where it stops, is spelled out under the rule
table below. What it never does is supply the missing value.

**How many of an element may appear is decided here too, and the decision is a gap.** The wiki says
multiple `<visual>` and multiple `<collision>` are allowed and says nothing at all about how many
`<origin>`, `<inertial>`, `<axis>`, `<limit>`, `<mimic>`, `<geometry>`, `<parent>` or `<child>` an
element may carry. meios reads the first of each and ignores the rest, with no diagnostic. That is
the one place on this page where silence is still the behavior rather than the thing being ended, and
it is stated here rather than left for you to discover: a second `<origin>` on a joint is not a
second frame, it is an element nobody will tell you was ignored.

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

**A name is exact bytes, and only an absent, empty or all-whitespace one is refused.** The wiki never
mentions whitespace or an empty attribute value. meios takes a name to be the attribute value as
authored: it never trims surrounding whitespace, folds case, or normalises before comparing, so
`Base_Link` and `base_link` are two links and `base_link` and `base_link ` are two more. Any folding
would invent an equivalence the document never stated and would begin refusing descriptions the
reference parser accepts. A name with internal whitespace is a legal name and is left alone. What is
refused is a name that is absent, empty, or nothing but whitespace: such an element has no identity,
several of them silently share one, and a joint cannot reference any of them.

**A document declaring no `<link>` is refused.** The wiki never addresses whether an empty `<robot>`
is a robot. meios refuses it: a description with no links describes no body, every consumer of the
resulting model would immediately find nothing in it, and refusing at the document says so at a
`file:line` rather than leaving the consumer to discover an empty model.

**A `<material>` under a `<visual>` that carries no name and defines neither a color nor a texture is
refused.** The wiki does not mention the construct. It is neither a reference — there is no name to
resolve — nor a definition, so there is nothing meios could read from it, and accepting it silently
would leave the visual with a material the author appears to have specified and meios in fact ignored.

**A `<mimic>` with no `multiplier` gets one, on this profile's authority rather than the wiki's.** The
joint page states that the `<mimic>` `offset` "Defaults to 0" and states no default at all for the
multiplier, so keeping one cannot be presented as following the specification. meios keeps it because
one is the only value that makes the relation the wiki documents — `value = multiplier * other +
offset` — an identity, so a `<mimic>` that names only its target still means what an author writing it
would expect; the reference parser agrees. This is the single value in this profile that is defaulted
without a citation. `rule:mimic-multiplier-default`

**A shape element the profile does not describe is not read as a shape.** The wiki gives `<geometry>`
exactly four children — `<box>`, `<cylinder>`, `<sphere>` and `<mesh>` — and says nothing about a
fifth. meios refuses one rather than falling through to a shape it invented; before this rule a
`<trapezoid>` produced a mesh with an empty filename, which is a shape the document never mentioned
and which no consumer could distinguish from an authored one.

**A numeric attribute must carry a number meios can read, and that number must be finite.** The wiki
never states what may appear in a numeric attribute. meios refuses text it cannot parse and text that
parses to an infinity or a NaN, because both reach a consumer as a coordinate, a mass or a limit and
propagate through every arithmetic that touches them.

**A fixed-length numeric attribute must carry exactly its own number of components.** The wiki
describes `xyz` as "the x, y, z offset" and `rpy` as the roll, pitch and yaw angles about the fixed
axes — three components each — and says nothing whatever about what a two-component or a
four-component string is supposed to mean. There is no reading of a two-component offset that does
not invent the missing number, and no reading of a four-component one that does not discard an
authored one. Both are documents meios cannot read, so both are refused. An **absent** `<origin>`, or
an absent `xyz` or `rpy` attribute, is not a violation: the wiki states those are optional and states
their defaults, and meios keeps them.

**The same count, and the same consequence, on `<mesh scale>` and on `<color rgba>`.** One reader
counts the components of every fixed-length numeric attribute, and the three constructs above are the
three it is asked for. The consequence is the same shape at every one of them: under the default the
document is refused, and under a permissive setting the element the attribute belongs to is dropped —
the `<visual>` or `<collision>` whose origin could not be read, the shape whose scale could not, the
`<material>` whose color could not. Nothing is completed with a value the document did not write, and
lowering the setting lowers only how loudly the refusal is said. The reader enforces the same count on
any further fixed-length numeric attribute a later revision of this profile describes; those
constructs get their own rows here, and until a row exists, treat only the three above as published.

Before this rule, meios did what the parsers it supersedes do: a two-component offset was completed
with a third zero, a four-component one was truncated to its first three, and both **silently**, with
no diagnostic at any level. A description with a typo in an offset loaded clean and placed a link
somewhere its author never wrote. A two-component `scale` was worse still: the mesh reached the
consumer scaled by zero, which is not a shape at all, and which discarded the two factors the author
did write. That is the behavior this rule exists to end.

**Every rule about what an inertial may contain is this profile's own.** The link page names `mass` and
the six tensor attributes and stops there: it never says the mass must be non-negative, never says the
tensor must describe a body that can exist, and never says what a reader should do with one that
cannot. meios refuses a negative mass, a negative moment of inertia, a tensor that is not positive
semi-definite, and a tensor that violates one of the three triangle inequalities on its diagonal. None
of those describes a rigid body, and a consumer computing a dynamics model from one gets an answer with
no physical meaning and no way to tell that from a real one.

**The tolerance is relative to the tensor's own scale, never an absolute number.** The scale is the
largest magnitude among the three diagonal components, and every comparison above admits a violation
of at most one part in a billion of that scale, raised to the order of the quantity being compared.
Real descriptions carry tensors spanning six orders of magnitude, from a small bracket to a
multi-tonne base, so a single absolute epsilon either refuses the small end outright or is smaller
than double-precision round-off at the large end. The epsilon exists only to absorb the round-off in a
determinant on a badly scaled tensor: the nearest real tensor measured sits about two parts in a
hundred from the boundary, seven orders of magnitude clear of it. It is not slack for bad data, and
widening it would not rescue a tensor that is actually wrong.

**A wholly zero tensor paired with a zero mass is accepted, deliberately.** Exact zeros on all six
components together with a mass of exactly zero is how real descriptions write a frame-only link — a
tool frame, a mounting flange, a coordinate marker with no body. The carve-out is exact rather than
near-zero, because the convention is authored zeros and a tolerance here would begin accepting tensors
that are merely small. `rule:inertia-massless-frame` A tensor whose three diagonal components are zero
while the tensor is not wholly zero gets no such reading: it is malformed, and it is refused.
`rule:inertia-degenerate-scale`

**What this profile does not check.** These rules decide a tensor's mathematical admissibility, not
its physical plausibility for the body it describes. A tensor that is semi-definite, satisfies the
triangle inequalities and is off by three orders of magnitude for the link it belongs to passes, and
so does one whose principal axes point somewhere the geometry does not support. meios has no model of
the link's shape and will not guess one; checking a tensor against its geometry is a separate job for
a consumer that owns both.

A non-finite mass or tensor component never reaches these rules: the finite-number rule above refuses
the attribute first. The arithmetic re-checks finiteness anyway, before it forms a single product,
because a determinant on components near the largest representable double overflows to an infinity —
and every tolerance derived from an infinite scale is itself infinite, so an unchecked tensor would be
accepted rather than refused.

**Four joint kinds use the `<axis>` field, and two do not.** The joint page says plainly: "Fixed and
floating joints do not use the axis field." A `revolute` or `continuous` joint turns about its axis, a
`prismatic` joint slides along it, and a `planar` joint is normal to it, so on those four an axis of
all zeros names no direction and describes a motion that cannot happen — meios refuses it. On `fixed`
and `floating` the element is not read, so an `<axis>` there is neither a violation nor a value: real
shipped descriptions carry one, and refusing it would refuse working hardware over an element the
specification says is unused. The wiki does not say what a zero axis on the other four kinds means, so
the refusal is this profile's decision; it is the only reading that does not silently accept a joint
that moves about nothing.

**On a `fixed` or `floating` joint, the axis member on the record carries no meaning.** meios's reader
fills the member for every kind — with the components the document authored if an `<axis>` was present,
and with the `(1, 0, 0)` the wiki states if it was not — so a consumer reading `joint.axis` on a fixed
joint receives a value the document never asked for and which the specification excludes. Do not read
it. Branch on the joint's kind first; the member is meaningful only on the four kinds above.

**An `<axis>` that carries no `xyz` is refused.** The joint page marks the attribute required on a
present element, and there is no third reading: leaving the axis at zero — which is what the reference
parser does — is a silently wrong axis, and substituting the absent-element default would treat writing
the element and omitting it as the same act.

**An axis is stored with exactly the components the document authored, and is never rescaled to unit
length.** The wiki's own examples are unit vectors and it says the axis is "specified in the joint
frame", but it states no normalization. meios performs none: rescaling on read would replace an
authored number with a computed one, and refusing anything not already unit-length would publish a
floating-point tolerance as a contract and begin refusing axes that are unit to fifteen digits. A
consumer that needs a unit axis normalizes it itself, from a value it can still see.

## Where this profile diverges

Two things are worth diverging from and are recorded here rather than left for a reader to discover
from behavior: a default the wiki states outright, and the parser this library supersedes.

### From a default the wiki states

**An absent `<inertial>` yields no inertial, not the zero the wiki states.** The link page marks
`<inertial>` "optional: defaults to a zero mass and zero inertia if not specified". meios instead
leaves the link's inertial absent, so a document that said nothing stays distinguishable from one that
authored a zero mass — a distinction the stated default destroys, and one a consumer computing a
dynamics model needs, because a zero-mass body and an unspecified body are not the same input. A
consumer that wants the wiki's default can apply it itself; it cannot recover the distinction meios
would have thrown away. `rule:inertial-optional`

This is the one default on this page that meios does not simply keep. Every other default the wiki
states for an element survives verbatim, and a default the wiki does not state is not invented — with
the single exception recorded in the own-authority section above, the `<mimic>` multiplier.

### From the reference parser

urdfdom is read here as an instrument for finding out what real descriptions rely on, never as a
specification. Where the two differ, the difference is deliberate.

**meios discloses; urdfdom is silent.** urdfdom ignores unknown elements and unknown attributes in
complete silence at every level — under `<robot>`, inside a `<link>`, inside a `<joint>` — and hands
back a model that looks complete. meios names each one at its own `file:line` and withdraws the
parse-completeness claim. Everything the drop rule below describes is meios choosing to say what it
did not read.

**meios preserves authoring order; urdfdom sorts.** urdfdom stores links, joints and materials in
`std::map` keyed by name, so a consumer receives them in alphabetical order and the order the
document was written in is not recoverable from the model at all. meios keeps the document's own
order in every collection it hands out. A description's authoring order carries intent — the arm
before the gripper, the base before what stands on it — and discarding it is a loss no consumer can
undo.

**urdfdom is more permissive than the wiki about a joint's link references, and meios is not.**
urdfdom's parse of `<parent>` or `<child>` with no `link` attribute only logs an informational
message; the load fails later, during the tree build, attributed to the tree rather than to the joint
that caused it. The wiki calls both `link` attributes mandatory. meios refuses at the attribute, so
the diagnostic points at the element the author has to fix.

**urdfdom is more permissive than the wiki about a coordinate-less `<axis>`, and meios is not.** An
`<axis>` element with no `xyz` leaves urdfdom's axis at all zeros — not the `(1,0,0)` the wiki states
as the default for an absent element, and not anything the document asked for. meios refuses it: the
wiki marks `xyz` required on a present element, and substituting the absent-element default would
treat writing the element and omitting it as the same act.

**urdfdom refuses a `<dynamics>` that sets neither `damping` nor `friction`; meios accepts it.** That
refusal is urdfdom's own invention — the wiki gives both attributes a stated default of zero, so an
empty `<dynamics>` is a well-formed element that says nothing. meios keeps the stated defaults and
reads it as written.

**Where the two agree on an identity rule, that agreement is corroboration and not authority.** The
reference parser also refuses a blank name, a duplicate name and a dangling link reference, and every
identity rule on this page would stand without it.

## Rules

| Element | Construct | Verdict | Diagnostic code | Rule id |
|---|---|---|---|---|
| `<origin>` | the `xyz` and `rpy` attributes | refuse | `vector_arity` | `rule:origin-xyz-arity` |
| `<mesh>` | the `scale` attribute | refuse | `vector_arity` | `rule:mesh-scale-arity` |
| `<color>` | the `rgba` attribute | refuse | `vector_arity` | `rule:material-rgba-arity` |
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
| any | a numeric attribute whose text is not a finite number | refuse | `invalid_number` | `rule:numeric-value-finite` |
| `<inertial>` | the `<mass>` child | refuse when absent | `missing_required_field` | `rule:mass-required` |
| `<mass>` | the `value` attribute | refuse when absent | `missing_required_field` | `rule:mass-value-required` |
| `<inertial>` | the `<inertia>` child | refuse when absent | `missing_required_field` | `rule:inertia-required` |
| `<inertia>` | each of the six tensor attributes | refuse when any is absent | `missing_required_field` | `rule:inertia-component-required` |
| `<joint>` | the `type` attribute | refuse when absent | `missing_joint_type` | `rule:joint-type-required` |
| `<joint>` | a `type` outside the declared six | refuse | `unknown_joint_type` | `rule:joint-type-known` |
| `<visual>`, `<collision>` | the `<geometry>` child | refuse when absent | `missing_geometry` | `rule:geometry-required` |
| `<geometry>` | a shape child | refuse when absent | `missing_geometry` | `rule:geometry-shape-required` |
| `<geometry>` | a shape child outside the declared four | refuse | `unknown_geometry_shape` | `rule:geometry-shape-known` |
| `<mesh>` | the `filename` attribute | refuse when absent | `missing_required_field` | `rule:mesh-filename-required` |
| `<box>` | the `size` attribute | refuse when absent | `missing_required_field` | `rule:box-size-required` |
| `<sphere>` | the `radius` attribute | refuse when absent | `missing_required_field` | `rule:sphere-radius-required` |
| `<cylinder>` | the `radius` and `length` attributes | refuse when either is absent | `missing_required_field` | `rule:cylinder-dimension-required` |
| `<joint>` | a `<limit>` on a bounded joint | refuse when absent | `missing_limit` | `rule:limit-required-bounded` |
| `<limit>` | the `effort` attribute | refuse when absent | `missing_required_field` | `rule:limit-effort-required` |
| `<limit>` | the `velocity` attribute | refuse when absent | `missing_required_field` | `rule:limit-velocity-required` |
| `<origin>`, `<axis>`, `<mimic>` | a default the wiki states | accept and keep the default | — | `rule:wiki-defaults-preserved` |
| `<axis>` | the `xyz` attribute on a present element | refuse when absent | `missing_required_field` | `rule:axis-xyz-required` |
| `<axis>` | an axis of all zeros on a kind that uses it | refuse | `zero_axis` | `rule:axis-nonzero-when-used` |
| `<axis>` | an element on a `fixed` or `floating` joint | accept, and read nothing from it | — | `rule:axis-not-used-on-fixed-floating` |
| `<axis>` | an axis whose length is not one | accept exactly as authored | — | `rule:axis-passthrough-non-unit` |
| `<mass>` | a `value` below zero | refuse | `invalid_mass` | `rule:mass-non-negative` |
| `<inertia>` | a tensor that is not positive semi-definite | refuse | `invalid_inertia` | `rule:inertia-semidefinite` |
| `<inertia>` | a diagonal violating one of the three triangle inequalities | refuse | `invalid_inertia` | `rule:inertia-triangle` |

**A required part that is missing drops the element that contained it — it is never completed with a
value nothing specified.** Under the strict document-validity setting each row above refuses the
document outright. Under the permissive settings the same diagnostic is emitted at warning level, or
at no level under `skip`, and the containing element is dropped: an `<inertial>` whose `<mass>` is
missing leaves the link with no inertial at all, and a `<visual>` or `<collision>` whose geometry
cannot be read is not added to the link. The accepted cost is that under a permissive setting a
consumer loses a partially-valid element rather than receiving the fields that were present. That is
the deliberate trade: a link with no inertial is a fact a consumer can see and act on, while a link
carrying a zero mass nobody wrote is indistinguishable from a real one.

The drop stops at the structural boundary. A `<link>` and a `<joint>` are never dropped, so a field of
a joint that meios cannot read — its `<origin>`, for one — is refused under the strict setting and
disclosed under the permissive ones, where the joint keeps the default the wiki states for that field.
There is no containing element to drop without changing the robot's topology, which no setting may do.

**`revolute` and `prismatic` are the two kinds that must declare a `<limit>`**, and the rule is
enforced by the library on every load rather than by a command-line pass a consumer may never run.
`continuous`, `fixed`, `floating` and `planar` are unbounded and need none. Within a `<limit>`,
`effort` and `velocity` are required and `lower` and `upper` are optional with a stated default of
zero. Until this rule moved into the library, a bounded joint with no limit loaded clean through every
entry point and was reported only by `meios validate`.

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

**Content this profile does not describe is dropped, and the drop is said out loud.** An element or an
attribute meios does not recognize is not read into the model, and a warning names it at its own
`file:line`. This is one rule at every level of the document: under `<robot>`, inside a `<link>`, and
inside a `<joint>` alike. Dropping is disclosure rather than fabrication, so it never fails a load, and
the diagnostic is emitted whatever the document-validity setting says: that setting grades the
XML-hygiene classes, and this is not one of them. What the drop does cost is the claim — the result
stops saying it was parsed whole, so a consumer can branch on that instead of scraping a log. Nothing
inside a dropped subtree is judged further: content this profile does not describe is content it has
no vocabulary to name, and reporting every node beneath an unrecognized element would say the same
thing many times.

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

**The claim answers for the document, never for the log.** Silencing a class of diagnostic does not
restore the content it reported: a document loaded with the document-validity setting at `skip`
carries the same claims it would carry at `warn`, and content dropped without a word said about it
withdraws the claim exactly as content dropped loudly does. This is the property that makes the claim
worth branching on — a claim that tracked what reached the sink would be a restatement of the setting
and would tell a consumer nothing about the document it just loaded.

An XML namespace declaration — `xmlns`, or any `xmlns:` prefix — is XML infrastructure rather than
description vocabulary and is never reported as an unknown attribute.
