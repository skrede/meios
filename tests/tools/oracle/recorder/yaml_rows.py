import re

from recorder.rendering import render, same_length, escape
from recorder.probes import FIXED_PROBES, SCALARS, UNIT_TAGS


def doubles_in(node, found):
    if isinstance(node, dict):
        for value in node.values():
            doubles_in(value, found)
    elif isinstance(node, list):
        for value in node:
            doubles_in(value, found)
    elif isinstance(node, float):
        found.append(repr(node))


def probe_set(config_dir):
    import xacro

    found = []
    xacro.init_stacks(None)
    for path in sorted(config_dir.glob("*.yaml")):
        doubles_in(xacro.load_yaml(str(path)), found)
    probes = list(FIXED_PROBES)
    return probes + [one for one in dict.fromkeys(found) if one not in probes]


def rendering_rows(tmp, config_dir):
    probes = probe_set(config_dir)
    body = "\n".join(' <p v="${%s}"/>' % escape(one) for one in probes)
    rendered = list(render(tmp, body).getElementsByTagName("p"))
    same_length("rendering", "rendered nodes", len(rendered), len(probes))
    return list(zip(probes, (node.getAttribute("v") for node in rendered)))


def degree_operands(share):
    found = []
    for path in sorted((share / "config").glob("**/*.yaml")):
        found += re.findall(r"!degrees[ \t]+(\S+)", path.read_text(encoding="utf-8"))
    return sorted(dict.fromkeys(found), key=lambda one: (float(one), one))


KINDS = {"NoneType": "null", "str": "string", "bool": "boolean", "int": "integer",
         "float": "float"}


def scalar_rows(tmp, share):
    import xacro

    sources = list(SCALARS) + ["!degrees " + one for one in degree_operands(share)]
    keys = ["k{:03d}".format(at) for at in range(len(sources))]
    document = tmp / "scalars.yaml"
    document.write_text("".join("{}: {}\n".format(k, s) for k, s in zip(keys, sources)),
                        encoding="utf-8", newline="\n")
    xacro.init_stacks(None)
    loaded = xacro.load_yaml(str(document))
    body = ' <xacro:property name="d" value="${xacro.load_yaml(\'%s\')}"/>\n' % document
    body += "\n".join(' <s v="${d[\'%s\']}"/>' % key for key in keys)
    rendered = list(render(tmp, body).getElementsByTagName("s"))
    same_length("yaml scalars", "keys", len(keys), len(sources))
    same_length("yaml scalars", "rendered nodes", len(rendered), len(sources))
    return [(source, KINDS[type(loaded[key]).__name__], node.getAttribute("v"))
            for source, key, node in zip(sources, keys, rendered)]


# The mapping wrapper upstream hands a loaded document back in defines attribute lookup to fall
# through to the document and binds subscript to the same function, so ordinary attribute lookup
# answers first and a key that is also an attribute of the wrapper never reaches the document. The
# colliding set is read off the wrapper here rather than written down: it is a property of the type
# upstream happens to derive from, and a typed-out list would be a claim about another program.
COLLISION_CONTROL = "plain"


def wrapper_names():
    import xacro

    return [one for one in dir(xacro.YamlDictWrapper) if not one.startswith("__")]


def result_category(one):
    return "method" if callable(one) else KINDS[type(one).__name__]


def collision_rows(tmp):
    import xacro

    names = wrapper_names() + [COLLISION_CONTROL]
    document = tmp / "collisions.yaml"
    document.write_text("".join("{}: {}\n".format(name, at) for at, name in enumerate(names)),
                        encoding="utf-8", newline="\n")
    xacro.init_stacks(None)
    loaded = xacro.load_yaml(str(document))
    same_length("collisions", "document keys", len(loaded), len(names))
    return [(name, result_category(getattr(loaded, name)), result_category(loaded[name]))
            for name in names]


def unit_tag_probes():
    return [(tag, operand) for tag, operands in UNIT_TAGS for operand in operands]


def unit_tag_rows(tmp):
    probes = unit_tag_probes()
    keys = ["u{:03d}".format(at) for at in range(len(probes))]
    document = tmp / "unit_tags.yaml"
    document.write_text("".join("{}: {} {}\n".format(key, tag, operand)
                                for key, (tag, operand) in zip(keys, probes)),
                        encoding="utf-8", newline="\n")
    body = ' <xacro:property name="t" value="${xacro.load_yaml(\'%s\')}"/>\n' % document
    body += "\n".join(' <u v="${t[\'%s\']}"/>' % key for key in keys)
    rendered = list(render(tmp, body).getElementsByTagName("u"))
    same_length("unit tags", "rendered nodes", len(rendered), len(probes))
    return [(tag, operand, node.getAttribute("v"))
            for (tag, operand), node in zip(probes, rendered)]

