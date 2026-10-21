from collections import namedtuple

from recorder.rendering import escape

# A family's slots are listed in the order its text column carries their values.
family = namedtuple("family", "name slots template case")

# The slots that land in an attribute value and are written escaped; every other slot is markup.
ESCAPED = ("TEXT", "SPEC")

ROBOT = '<robot name="g" xmlns:xacro="http://www.ros.org/wiki/xacro">%s</robot>'
PROPERTIES = ('<xacro:property name="x" value="vx"/><xacro:property name="e" value=""/>'
              '<xacro:property name="pk" value="p"/><xacro:property name="sp" value=" p"/>')

INCLUDE_PREFIXES = (("", 5), ("${e}", 1), ("x/", 1), ("./", 1), ("$$", 1), ("$", 1))
INCLUDE_HEADS = ("$(find p)", "$(find  p)", "$(find p )", "$(find ${pk})", "$(find ${sp})",
                 "$(find q)", "$(find nopkg)", "$(find )", "$(find p q)", "${'$(find p)'}", "")
INCLUDE_SEPARATORS = ("/", "//", "", "/./", "./")
INCLUDE_TAILS = ("urdf/c.xacro", "c.xacro", "urdf/sub/d.xacro", "urdf/sub/../c.xacro",
                 "urdf/../urdf/c.xacro", "../q/a.xacro", "a.xacro", "missing.xacro",
                 "urdf/${nope}.xacro", "${x}.xacro")
INCLUDE_SUFFIXES = (("", 6), ("$$x", 1), ("${x", 1), ("$(x", 1), ("$", 1))


def include_case(draw):
    return (draw.weighted(INCLUDE_PREFIXES) + draw.pick(INCLUDE_HEADS)
            + draw.pick(INCLUDE_SEPARATORS) + draw.pick(INCLUDE_TAILS)
            + draw.weighted(INCLUDE_SUFFIXES))


SPAN_ATOMS = ("a", "/", ".", " ", "$", "$$", "$$$", "{", "}", "(", ")", "${x}", "${nope}", "${e}",
              "${'a'}", "$(arg y)", "$(arg nope)", "$(arg  y)", "$(arg y z)", "$(arg ${x})",
              "${'$(arg y)'}", "$(find p)", "$(find  p)", "$(find p urdf)", "$(find nopkg)")


def span_case(draw):
    return "".join(draw.pick(SPAN_ATOMS) for _ in range(draw.between(1, 6)))


MACRO_NAMES = ("a", "b", "c")
MACRO_FORMS = ("{n}", "{n}:={d}", "{n}={d}", "{n}:=^", "{n}:=^|{d}", "*{n}", "**{n}")
MACRO_DEFAULTS = ("1", "${x}", "${1 + 2}", "$(arg y)", "'q r'", "${cp}", "$$x", "")
CALL_NAMES = ("a", "b", "c", "d")
CALL_VALUES = ("1", "${x}", "$(arg y)", "${nope}", "$$x", "z w", "${cp}")
CALL_CHILDREN = ("", "<visual/>", "<visual/><collision/>", '<xacro:property name="cp" value="c"/>')


def parameter(draw):
    name, form = draw.pick(MACRO_NAMES), draw.pick(MACRO_FORMS)
    default = draw.pick(MACRO_DEFAULTS) if "{d}" in form else ""
    written = form.format(n=name, d=default)
    if form.startswith("*"):
        return written, '<xacro:insert_block name="%s"/>' % name
    return written, '<link name="${%s}"/>' % name


def macro_case(draw):
    specification = [parameter(draw) for _ in range(draw.between(1, 3))]
    spec = specification[0][0] + "".join(draw.pick((" ", "  ")) + written
                                         for written, _ in specification[1:])
    names = draw.distinct(CALL_NAMES, draw.between(0, 3))
    attributes = " ".join('%s="%s"' % (name, escape(draw.pick(CALL_VALUES))) for name in names)
    body = "".join(element for _, element in specification)
    return "\t".join((spec, attributes, draw.pick(CALL_CHILDREN), body))


# Drawn in this order from one sequence, so a family appended later leaves every earlier family's
# cases as they were.
FAMILIES = (
    family("include", ("TEXT",),
           ROBOT % (PROPERTIES + '<xacro:include filename="@TEXT@"/>'), include_case),
    family("span", ("TEXT",),
           ROBOT % (PROPERTIES + '<xacro:arg name="y" default="Y"/><link name="@TEXT@"/>'),
           span_case),
    family("macro", ("SPEC", "ATTRS", "CHILDREN", "BODY"),
           ROBOT % ('<xacro:property name="x" value="vx"/><xacro:property name="b" value="ob"/>'
                    '<xacro:arg name="y" default="Y"/><xacro:macro name="m" params="@SPEC@">'
                    '@BODY@</xacro:macro><xacro:m @ATTRS@>@CHILDREN@</xacro:m>'), macro_case),
)
