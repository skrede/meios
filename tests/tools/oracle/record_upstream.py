# Two layers in one file: the host layer builds a pinned virtual environment and re-executes this
# same file inside it, because upstream xacro must never be importable from the interpreter a
# developer happens to run.

import os
import re
import sys
import hashlib
import argparse
import tempfile
import subprocess
import importlib.metadata
from pathlib import Path
from collections import namedtuple

PINS = (("xacro", "2.1.1"), ("pyyaml", "6.0.3"))
CORPUS_PIN = "4.3.1"
LBR_PIN = "2.5.0"
FRANKA_PIN = "2.8.1"
KORTEX_PIN = "0.2.5"
MODULES = {"xacro": "xacro", "pyyaml": "yaml"}
TOOLING = ("pip", "setuptools", "wheel")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
DOC = ('<?xml version="1.0"?>\n<robot name="probe" xmlns:xacro="http://wiki.ros.org/xacro">\n'
       '%s\n</robot>\n')
FIXED_PROBES = ("-0.0005", "-0.0001", "0.0004", "0.001", "0.0001", "0.00001", "1e15", "1e16",
                "1e17", "180.0", "0.0", "-0.0")
SCALARS = ("true", "false", "yes", "no", "y", "n", "'true'", '"1"', "1e5", "1.5e3", "1.0e+5",
           "010", "1_000", "42", "3.5", "", "~", "null", "hello")
SEED_YAML = ("mesh_files:\n  base:\n    visual:\n      mesh:\n        package: ur_description\n"
             "        path: meshes/base.dae\njoint_limits:\n  shoulder_pan:\n    min: -6.28\n")
# A second document, kept apart from the seed above so adding a sequence does not change what the
# expressions reading the whole seed document already render.
SEQUENCE_YAML = "bounds:\n  - -6.28\n  - 0.0\n  - 6.28\n"
# A third, kept apart for the same reason: two keys the mapping wrapper answers itself and one it
# does not, so the subscript spelling of each can be rendered beside the other.
COLLISION_YAML = "keys: 5\nvalues: 6\nplain: 7\n"
# Upstream registers exactly these six tags and converts each by multiplying the tagged text by
# its constant. An operand of 1 makes a row carry the constant itself; the wider operands separate
# the constants from one another and pin the multiplication order.
UNIT_TAGS = (("!radians", ("1", "0.5", "-2")),
             ("!degrees", ("1", "45", "90", "-180.0")),
             ("!meters", ("1", "2", "-0.75")),
             ("!millimeters", ("1", "1500", "-25.4")),
             ("!foot", ("1", "3", "-0.5")),
             ("!inches", ("1", "12", "-6")))
SEEDS = {"safety_pos_margin": "0.15", "mass": "3.7", "radius": "0.06", "length": "0.12",
         "wrist_3_joint_type": "continuous", "name": "base", "type": "visual",
         "sec_mesh_files": "${xacro.load_yaml(seed_file)['mesh_files']}",
         "sec_bounds": "${xacro.load_yaml(sequence_file)['bounds']}",
         "sec_collisions": "${xacro.load_yaml(collision_file)}"}
# Authored rather than found in the closure: no pinned description subscripts a sequence, so the
# spellings are named here and upstream is asked what each one means.
SEQUENCE_PROBES = ("sec_bounds[0]", "sec_bounds[-1]", "sec_bounds[3]", "sec_bounds[-4]",
                   "sec_bounds['0']")
# The subscript spelling of a name the mapping wrapper answers itself, beside a name it does not.
# Only this spelling is probed: the attribute spelling of a colliding name renders an object
# address and does not reproduce between runs.
COLLISION_PROBES = ("sec_collisions['keys']", "sec_collisions['values']",
                    "sec_collisions['plain']")
# The keyword-argument mapping constructor, driven through a minimized document rather than read
# out of a description: the two pinned files carrying the form are macro fragments, and loading a
# fragment as a document turns every refusal into a false one. The refusing probes are the ones
# whose refusal upstream names something an independent implementation can also name -- a repeated
# keyword, an absent key, an argument owing its value.
CONSTRUCTOR_PROBES = ("dict(a=1, b=2)", "dict(a=1, b=2)['a']", "dict()",
                      "dict(radius=radius, length=length)['radius']", "dict(a=dict(b=2)['b'])",
                      "dict(mesh=sec_mesh_files['base'])", "'a' in dict(a=1)",
                      "dict()['missing']", "dict(a=1, a=2)", "dict(a=)")
# The four string meanings a real vendor description evaluates on its default render, driven
# through a minimized document because the pinned files carrying them are macro fragments: two
# strings added, a string's truth value, substring containment, and the one named split. The
# refusing probes are the ones whose refusal upstream names something an independent
# implementation can also name -- an operand mismatch, a needle that is not text, an index past
# the last field, an empty separator, and a separator that is not text.
STRING_PROBES = ("'x' + 'y'", "'' + 'y'", "'x' + ''", "'' + ''", "name + '_' + type",
                 "'x' + 1", "1 + 'x'", "'x' - 'y'", "'x' / 'y'", "'x' // 'y'", "'x' % 'y'",
                 "'x' ** 'y'",
                 "not ''", "not 'a'", "'' if '' else name + type + '_'",
                 "name + '_' if name else ''", "'' if name else name + type + '_'",
                 "'x' in 'axb'", "'' in 'abc'", "'z' in 'abc'", "1 in 'abc'",
                 "'a b c'.split(' ')", "'a b c'.split(' ')[0]", "'a b c'.split(' ')[-1]",
                 "'a  b'.split(' ')", "' a '.split(' ')", "''.split(' ')", "'abc'.split('x')",
                 "'ab'.split('b')", "'a b c'.split(' ')[3]", "'abc'.split('')",
                 "'a b'.split(1)",
                 "sec_mesh_files['base']['visual']['mesh']['path'].split('/')[-1]")
# Every top-level document the pinned Franka tree ships whose stem names its own directory and
# whose renders this project reproduces. All eight were rendered and compared: no two are alike, so
# each of these carries its own measurement rather than standing in for a sibling. The two absent
# ones build an arm list with a bracket literal and slice it, which this grammar does not read.
FRANKA_DOCUMENTS = ("fer", "fp3", "fr3", "fr3v2", "fr3v2_1", "tmrv0_2")

# One entry point of the pinned corpus: the share its description resolves against, its path under
# the root that share names, the arguments upstream is driven with, the arguments the corpus record
# passes, the fetched upstream whose content digest pins its revision, and the record its measured
# facts are written to. The two argument fields differ because upstream needs every argument named
# outright while the corpus record passes only the ones that select a variant.
entry_point = namedtuple("entry_point", "share document mappings arguments upstream record")

UR = {"ur_type": "ur5e", "name": "ur"}
# The one entry point this project wrote rather than pinned. It is in this repository instead of a
# fetched tree, so it resolves from the repository rather than through the package index, and the
# two files that are the whole of it are named here as the listfile names them.
AUTHORED = "corpus_merged"
AUTHORED_FILES = ("gantry.urdf.xacro", "joint_limits.yaml")
AUTHORED_DIR = REPO / "tests" / "fixtures" / "urdf" / AUTHORED
# The fresh-render orchestrator beside this file drives these same entry points; it reads this
# table rather than carrying its own, because two lists of the same descriptions drift apart.
CORPUS_DOCUMENTS = (
    entry_point("ur_description", "urdf/ur.urdf.xacro", UR, "ur_type=ur5e", "ur_description",
                "ur5e_facts.cases"),
    entry_point("ur_description", "urdf/ur.urdf.xacro", dict(UR, ur_type="ur3e"), "ur_type=ur3e",
                "ur_description", "ur3e_facts.cases"),
    entry_point("ur_description", "urdf/ur.urdf.xacro", dict(UR, ur_type="ur7e"), "ur_type=ur7e",
                "ur_description", "ur7e_facts.cases"),
    entry_point("ur_description", "urdf/ur.urdf.xacro", dict(UR, safety_limits="true"),
                "safety_limits=true ur_type=ur5e", "ur_description", "ur5e_safety_facts.cases"),
    entry_point("ur_description", "urdf/ur.urdf.xacro", dict(UR, force_abs_paths="true"),
                "force_abs_paths=true ur_type=ur5e", "ur_description",
                "ur5e_abs_paths_facts.cases"),
    entry_point("kuka_kr6_support", "kuka_kr6_support/urdf/kr6r900sixx.xacro", {}, "",
                "kuka_experimental", "kr6_facts.cases"),
    entry_point("lbr_med14_r820_description", "urdf/lbr_med14_r820.urdf.xacro", {}, "",
                "lbr_med14_r820_description", "lbr_med14_r820_facts.cases"),
) + tuple(entry_point("franka_description", "robots/{}/{}.urdf.xacro".format(one, one), {}, "",
                      "franka_description", "{}_facts.cases".format(one))
          for one in FRANKA_DOCUMENTS) + (
    entry_point("kortex_description", "robots/gen3.xacro", {"dof": "7"}, "dof=7",
                "kortex_description", "gen3_facts.cases"),
    # Driven with a distinguishing name as well as the variant: the variant alone would key this
    # document identically to the plain ur.urdf.xacro entry point above.
    entry_point("ur_description", "urdf/ur_mocked.urdf.xacro", dict(UR, name="ur_mocked"),
                "name=ur_mocked ur_type=ur5e", "ur_description", "ur_mocked_facts.cases"),
    entry_point(AUTHORED, "gantry.urdf.xacro", {}, "", AUTHORED, "gantry_facts.cases"),
)


# The id every record, inventory row and ledger row keys this entry point by: the argument set
# where one selects the variant, the document's own filename where every argument defaults.
def key_of(one):
    return one.arguments or Path(one.document).name


# The KR6 resolves $(find) across two sibling packages, so the root it is named under is the
# directory holding both rather than the share of either.
def document_root(share, name):
    if name == AUTHORED:
        return AUTHORED_DIR
    root = share(name)
    return root.parent if name == "kuka_kr6_support" else root


# What pins an in-repo document, standing where a fetched one's archive digest stands: the same
# hash of hashes the corpus listfile recomputes at configure, so the two agree or configure fails.
def content_digest():
    joined = "".join(hashlib.sha256((AUTHORED_DIR / one).read_bytes()).hexdigest()
                     for one in AUTHORED_FILES)
    return hashlib.sha256(joined.encode()).hexdigest()


LIMIT_SEEDS = ("shoulder_pan", "shoulder_lift", "elbow_joint", "wrist_1", "wrist_2", "wrist_3")


def interpreter(venv):
    return venv / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def distributions(python):
    listing = subprocess.run([str(python), "-m", "pip", "list", "--format=freeze"],
                             capture_output=True, text=True, check=True).stdout
    return dict(line.lower().split("==", 1) for line in listing.split() if "==" in line)


def bootstrap(venv):
    python = interpreter(venv)
    if not python.exists():
        subprocess.run([sys.executable, "-m", "venv", str(venv)], check=True)
    for name, want in PINS:
        have = distributions(python).get(name)
        if have and have != want:
            sys.exit("oracle: {} {} is installed where {} is pinned; delete {} and rerun"
                     .format(name, have, want, venv))
    subprocess.run([str(python), "-m", "pip", "install", "--quiet", "--disable-pip-version-check",
                    *("{}=={}".format(n, v) for n, v in PINS)], check=True)
    extra = set(distributions(python)) - set(n for n, _ in PINS) - set(TOOLING)
    if extra:
        sys.exit("oracle: resolution pulled unpinned distributions: " + " ".join(sorted(extra)))
    return python


def package_roots(given):
    roots = [Path(one).resolve() for one in given]
    # A fetched upstream lands in the build tree's resource directory beside the package-root copy
    # the corpus makes, so one root named by hand also admits the trees fetched alongside it.
    for root in list(roots):
        roots += sorted(p for p in (root.parent / "_meios_resources").glob("*") if p.is_dir())
    return roots


def escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace('"', "&quot;")


def render(tmp, body, mappings=None):
    import xacro

    source = tmp / "probe.xacro"
    source.write_text(DOC % body, encoding="utf-8", newline="\n")
    return xacro.process_file(str(source), mappings=mappings or {})


def children(node, tag):
    return [c for c in node.childNodes if c.nodeType == c.ELEMENT_NODE and c.tagName == tag]


def attribute_text(node, names):
    return " ".join("{}={}".format(n, node.getAttribute(n)) for n in names if node.hasAttribute(n))


def origin_rows(link):
    rows = []
    for kind in ("visual", "collision"):
        for at, part in enumerate(children(link, kind)):
            for origin in children(part, "origin"):
                key = "origin.{}.{}".format(link.getAttribute("name"), kind)
                rows.append((key if not at else "{}.{}".format(key, at),
                             attribute_text(origin, ("xyz", "rpy"))))
    return rows


# force_abs_paths renders a mesh URI as file://$(find <package>)/..., which bakes this recording
# run's own absolute build-tree location. Normalized to a portable file://<package-root>/... form
# so the record stays reproducible across build trees; the C++ comparison applies the identical
# normalization to its own resolved package root before comparing, so both sides agree regardless
# of where either one's package root actually sits on disk.
def portable_mesh_uri(filename, package_root):
    prefix = "file://" + str(package_root)
    if filename.startswith(prefix):
        return "file://<package-root>" + filename[len(prefix):]
    return filename


# The model is the link and joint children of the robot root and nothing else. A sweep of the whole
# document also collects the <joint> elements an extension block declares -- a ros2_control interface
# list names one per actuated joint, with no type and no limit -- which would record a joint twice
# under contradicting values and count a model this project never builds.
def facts_rows(doc, package_root):
    links = children(doc.documentElement, "link")
    joints = children(doc.documentElement, "joint")
    rows = [("link.count", str(len(links))), ("joint.count", str(len(joints)))]
    rows += [("link.name.{}".format(at), l.getAttribute("name")) for at, l in enumerate(links)]
    rows += [("joint.name.{}".format(at), j.getAttribute("name")) for at, j in enumerate(joints)]
    for joint in joints:
        rows.append(("joint.type." + joint.getAttribute("name"), joint.getAttribute("type")))
        for limit in children(joint, "limit"):
            rows.append(("joint.limit." + joint.getAttribute("name"),
                         attribute_text(limit, ("lower", "upper", "effort", "velocity"))))
    meshes = []
    for link in links:
        for mesh in link.getElementsByTagName("mesh"):
            filename = portable_mesh_uri(mesh.getAttribute("filename"), package_root)
            if filename not in meshes:
                meshes.append(filename)
    rows += [("mesh.filename.{}".format(at), m) for at, m in enumerate(meshes)]
    for link in links:
        rows += origin_rows(link)
    return rows


def robot_facts(root, document, mappings):
    import xacro

    return facts_rows(xacro.process_file(str(root / document), mappings=mappings), root.parent)


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


def same_length(builder, what, counted, probed):
    if counted != probed:
        sys.exit("oracle: {} produced {} {} where {} were probed; nothing was recorded"
                 .format(builder, counted, what, probed))


def rendering_rows(tmp, config_dir):
    probes = probe_set(config_dir)
    body = "\n".join(' <p v="${%s}"/>' % escape(one) for one in probes)
    rendered = list(render(tmp, body).getElementsByTagName("p"))
    same_length("rendering", "rendered nodes", len(rendered), len(probes))
    return list(zip(probes, (node.getAttribute("v") for node in rendered)))


def closure_expressions(share):
    seen = []
    for name in ("urdf/ur.urdf.xacro", "urdf/ur_macro.xacro", "urdf/inc/ur_common.xacro"):
        for found in re.finditer(r"\$\{([^{}]*)\}", (share / name).read_text(encoding="utf-8")):
            if found.group(1) not in seen:
                seen.append(found.group(1))
    trivial = re.compile(r"^[A-Za-z_]\w*(\['[^']*'\])*$")
    return [one for one in seen if not trivial.match(one)]


def seed_body(seed_file, sequence_file, collision_file):
    rows = [' <xacro:property name="seed_file" value="%s"/>' % escape(str(seed_file)),
            ' <xacro:property name="sequence_file" value="%s"/>' % escape(str(sequence_file)),
            ' <xacro:property name="collision_file" value="%s"/>' % escape(str(collision_file))]
    rows += [' <xacro:property name="%s_parameters_file" value="%s"/>' % (n, escape(str(seed_file)))
             for n in ("joint_limits", "kinematics", "physical", "visual")]
    rows += [' <xacro:property name="%s" value="%s"/>' % (n, escape(v)) for n, v in SEEDS.items()]
    for at, joint in enumerate(LIMIT_SEEDS):
        rows += [' <xacro:property name="%s_%s_limit" value="%s"/>' % (joint, edge, sign * (6.0 + at))
                 for edge, sign in (("lower", -1.0), ("upper", 1.0))]
    return "\n".join(rows)


def write_seed_documents(tmp):
    (tmp / "seed.yaml").write_text(SEED_YAML, encoding="utf-8", newline="\n")
    (tmp / "sequence.yaml").write_text(SEQUENCE_YAML, encoding="utf-8", newline="\n")
    (tmp / "collisions.yaml").write_text(COLLISION_YAML, encoding="utf-8", newline="\n")
    return seed_body(tmp / "seed.yaml", tmp / "sequence.yaml", tmp / "collisions.yaml")


# A run in which the environment rather than the expression produced the failures would otherwise
# be recorded as a corpus of expected refusals, and the digest gate would then certify it. The floor
# leaves room for a genuine future refusal without admitting a wholesale failure.
REFUSAL_FLOOR = 2


# The record carries the failure's first line and nothing more, because a multi-line message would
# put an embedded newline into a tab-delimited row.
def refusal_text(failure):
    lines = str(failure).splitlines()
    return lines[0] if lines and lines[0].strip() else type(failure).__name__


def closure_cases(share):
    return [("e{:02d}".format(at + 1), one)
            for at, one in enumerate(closure_expressions(share))]


def sequence_cases():
    return [("s{:02d}".format(at + 1), one) for at, one in enumerate(SEQUENCE_PROBES)]


def collision_cases():
    return [("c{:02d}".format(at + 1), one) for at, one in enumerate(COLLISION_PROBES)]


def constructor_cases():
    return [("d{:02d}".format(at + 1), one) for at, one in enumerate(CONSTRUCTOR_PROBES)]


def string_cases():
    return [("t{:02d}".format(at + 1), one) for at, one in enumerate(STRING_PROBES)]


def authored_cases():
    return sequence_cases() + collision_cases() + constructor_cases() + string_cases()


def expression_cases(share):
    return closure_cases(share) + authored_cases()


def probe_row(tmp, seeds, case_id, expression):
    body = '%s\n <e v="${%s}"/>' % (seeds, escape(expression))
    try:
        text = render(tmp, body).getElementsByTagName("e")[0].getAttribute("v")
        return (case_id, expression, text, "")
    except Exception as failure:
        return (case_id, expression, "REFUSED", refusal_text(failure))


def refused_in(rows):
    return sum(1 for one in rows if one[2] == "REFUSED")


# The closure floor and the authored floor bound the same failure from opposite ends: the closure
# is expected to render, so too many refusals is a broken environment, while the authored probes
# exist to record refusals, so a set in which nothing rendered is one.
def expression_rows(tmp, share):
    seeds = write_seed_documents(tmp)
    closure = [probe_row(tmp, seeds, one, text) for one, text in closure_cases(share)]
    if refused_in(closure) > REFUSAL_FLOOR:
        sys.exit("oracle: {} expressions refused where at most {} is a measurement rather than a "
                 "broken environment; nothing was recorded"
                 .format(refused_in(closure), REFUSAL_FLOOR))
    authored = [probe_row(tmp, seeds, one, text) for one, text in authored_cases()]
    if refused_in(authored) == len(authored):
        sys.exit("oracle: every authored subscript probe refused, which is a broken environment "
                 "rather than a measurement; nothing was recorded")
    return closure + authored


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


def imported_from_pin(module_name):
    origin = Path(importlib.import_module(module_name).__file__).resolve()
    if not origin.is_relative_to(Path(sys.prefix).resolve()):
        sys.exit("oracle: {} was imported from {}, outside the environment at {}; nothing was "
                 "recorded".format(module_name, origin, sys.prefix))


# The record's claim that exactly these distributions produced every measurement below it is a fact
# only if the run reads the versions out of the environment it measures in.
def measured_pins():
    versions = {}
    for name, want in PINS:
        imported_from_pin(MODULES[name])
        have = importlib.metadata.version(name)
        if have != want:
            sys.exit("oracle: the environment holds {} {} where {} is pinned; nothing was recorded"
                     .format(name, have, want))
        versions[name] = have
    return versions


# A corpus package is not an installed distribution and has no metadata to read a version from,
# so its own manifest in the fetched tree is the statement of record.
def package_version(share, name, expected):
    manifest = share(name) / "package.xml"
    text = manifest.read_text(encoding="utf-8") if manifest.is_file() else ""
    found = re.search(r"<version>\s*([^<]*?)\s*</version>", text)
    if not found or not found.group(1):
        sys.exit("oracle: no version in the manifest {}; nothing was recorded".format(manifest))
    if found.group(1) != expected:
        sys.exit("oracle: {} declares {} where {} is pinned; nothing was recorded"
                 .format(name, found.group(1), expected))
    return found.group(1)


def package_digest(listfile, name):
    found = re.search(r"NAME {}.*?HASH SHA256=([0-9a-f]+)".format(re.escape(name)), listfile, re.S)
    return found.group(1)


def pin_rows(versions, share):
    listfile = (REPO / "cmake" / "corpus.cmake").read_text(encoding="utf-8")
    return [("xacro", versions["xacro"], "-"), ("PyYAML", versions["pyyaml"], "-"),
            (AUTHORED, "in-repo", content_digest()),
            ("ur_description", package_version(share, "ur_description", CORPUS_PIN),
             package_digest(listfile, "ur_description")),
            ("lbr_med14_r820_description",
             package_version(share, "lbr_med14_r820_description", LBR_PIN),
             package_digest(listfile, "lbr_med14_r820_description")),
            ("franka_description", package_version(share, "franka_description", FRANKA_PIN),
             package_digest(listfile, "franka_description")),
            ("kortex_description",
             package_version(share, "kortex_description", KORTEX_PIN),
             package_digest(listfile, "kortex_description"))]


LEDGER_COLUMNS = 8
LEDGER_CARRIED = 3
LEDGER = "compatibility_ledger.cases"


# The last three columns describe this project's own load, which nothing driving upstream can
# observe. They are read back from the committed record and re-emitted unchanged, so a run that
# measures upstream never invents them and never drops them; the test that reads the record is what
# holds them to an actual load. A committed record with no row for an entry point yields empty
# columns, and that row then fails its comparison rather than passing on a claim nobody made.
def carried_columns():
    path = REPO / "tests" / "golden" / "oracle" / LEDGER
    carried = {}
    if not path.is_file():
        return carried
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split("\t")
        if not line.startswith("#") and len(fields) == LEDGER_COLUMNS:
            carried[fields[0]] = fields[-LEDGER_CARRIED:]
    return carried


# A fetched document is pinned by the archive digest the listfile names; the authored one is in
# this repository and is pinned by its own bytes instead.
def revision_of(one, listfile):
    digest = content_digest() if one.upstream == AUTHORED \
        else package_digest(listfile, one.upstream)
    return "{}@{}".format(one.upstream, digest)


def shape_text(rows):
    measured = dict(rows)
    return "links={} joints={}".format(measured["link.count"], measured["joint.count"])


def ledger_rows(measured):
    listfile = (REPO / "cmake" / "corpus.cmake").read_text(encoding="utf-8")
    carried = carried_columns()
    rows = []
    for one in CORPUS_DOCUMENTS:
        key = key_of(one)
        rows.append((key, one.document, revision_of(one, listfile), one.arguments,
                     shape_text(measured[key]))
                    + tuple(carried.get(key, [""] * LEDGER_CARRIED)))
    return rows


def write_record(out, name, header, rows):
    text = "".join("# " + line + "\n" for line in header)
    text += "".join("\t".join(row) + "\n" for row in rows)
    (out / name).write_text(text, encoding="utf-8", newline="\n")


HEADERS = {
    "PINS": ["name <TAB> version or revision <TAB> content digest, or a bare - where the version",
             "is the identity. Every measurement below was produced by exactly these."],
    "rendering.cases": ["probe <TAB> the text upstream renders it as, measured by evaluating the",
                        "probe in an attribute and reading the attribute back."],
    "expressions.cases": ["case <TAB> expression <TAB> upstream rendering <TAB> failure. Every",
                          "non-trivial form in the closure of the Universal Robots document,",
                          "followed by the authored spellings no pinned description reaches -- an",
                          "index into a sequence, a key the mapping wrapper answers itself, the",
                          "keyword-argument mapping constructor, and the string meanings a vendor",
                          "description evaluates: two strings added, a string's truth value,",
                          "substring containment and the named split -- each driven through a",
                          "minimized document seeding the names it reads. A refusing row renders",
                          "REFUSED and carries the failure's first line."],
    "unit_tags.cases": ["tag <TAB> operand <TAB> the text upstream renders the converted value",
                        "as. The operand is written after the tag exactly as recorded. Every",
                        "operand measured here is a decimal literal; upstream evaluates the tagged",
                        "text as an expression and so also accepts a name, a call or a",
                        "hexadecimal literal, none of which are measured here."],
    "collisions.cases": ["name <TAB> the category the attribute spelling yields <TAB> the",
                         "category the subscript spelling yields. Every non-dunder attribute the",
                         "mapping wrapper carries, read off the wrapper itself, followed by one",
                         "control key that is not an attribute name at all. The attribute",
                         "spelling's own text is not recorded because it embeds the object's",
                         "address and does not reproduce between runs, which is also why these",
                         "rows carry no live comparison probe. A dunder-prefixed name is not",
                         "recorded: how many there are is a property of the interpreter that did",
                         "the recording rather than of the mapping type, so the prefix is carried",
                         "as a rule instead."],
    "yaml_scalars.cases": ["source <TAB> resolved kind <TAB> rendered text. The source is the",
                           "scalar exactly as written after the key; an empty source column is a",
                           "key written with no value at all."],
    LEDGER: ["entry point <TAB> document <TAB> revision <TAB> arguments <TAB> upstream result",
             "<TAB> native result <TAB> exercised constructs <TAB> reviewed divergence. One row",
             "per pinned entry point, keyed exactly as the measurement records are keyed. The",
             "pairing is enforced both ways by the test that reads this file: a pinned entry point",
             "with no row here fails, and a row here naming no pinned entry point fails. So is the",
             "construct column, which is compared against the set observed by loading that",
             "document -- a claimed construct the load does not exercise and an exercised",
             "construct the row does not claim each fail, and a spelling the vocabulary does not",
             "carry fails rather than resolving to anything. The first five columns are measured",
             "against the pinned upstream by the run that writes this file. The last three",
             "describe this project's own load, which no run driving upstream can observe: such a",
             "run carries them forward from the committed record unchanged, and the test is what",
             "holds them to an actual load. A row whose divergence column is empty claims the two",
             "results agree and a row whose column is filled claims they do not, and both claims",
             "are checked. What is recorded here is the whole of what this project claims about",
             "the descriptions it loads: an entry point absent from this file is not claimed, and",
             "the listfile that names the corpus records which documents its pinned upstreams",
             "ship that are not pinned here, and why. Every row but one is a vendor's own",
             "document. The exception is a description this project authored, because no surveyed",
             "vendor writes a merge key into a file a description loads, and a row whose revision",
             "names corpus_merged is that document rather than somebody else's robot. One member",
             "of the construct vocabulary is supported and witnessed by no load at all: a mapping",
             "keyed by something other than text. Such a key is admitted and takes its place in",
             "the mapping, but every subscript spelling here is text, so nothing reaches it and a",
             "load exercising it would exercise a dead end rather than a construct. Its evidence",
             "is a unit case and a divergence-manifest row, deliberately not a row here."],
    "facts": ["fact <TAB> value, measured from the document upstream renders. A joint that",
              "declares no limit element carries no joint.limit row at all."],
}


def records(tmp, share, versions):
    ur = share("ur_description")
    facts = HEADERS["facts"]
    measured = {key_of(one): robot_facts(document_root(share, one.share), one.document,
                                         one.mappings)
                for one in CORPUS_DOCUMENTS}
    return {
        "PINS": (HEADERS["PINS"], pin_rows(versions, share)),
        "rendering.cases": (HEADERS["rendering.cases"], rendering_rows(tmp, ur / "config" / "ur5e")),
        "expressions.cases": (HEADERS["expressions.cases"],
                              expression_rows(tmp, ur)),
        "unit_tags.cases": (HEADERS["unit_tags.cases"], unit_tag_rows(tmp)),
        "yaml_scalars.cases": (HEADERS["yaml_scalars.cases"], scalar_rows(tmp, ur)),
        "collisions.cases": (HEADERS["collisions.cases"], collision_rows(tmp)),
        LEDGER: (HEADERS[LEDGER], ledger_rows(measured)),
        **{one.record: (facts, measured[key_of(one)]) for one in CORPUS_DOCUMENTS},
    }


def measure(out, versions):
    from ament_index_python.packages import get_package_share_directory

    with tempfile.TemporaryDirectory() as scratch:
        tmp = Path(scratch)
        written = records(tmp, lambda name: Path(get_package_share_directory(name)), versions)
    out.mkdir(parents=True, exist_ok=True)
    for name, (header, rows) in written.items():
        write_record(out, name, header, rows)
    # Configure fails on any record present but unlisted, so the digest set is every record on
    # disk rather than only the ones this run regenerated: a curated record the recorder does not
    # produce keeps its coverage instead of silently losing it on the next run.
    covered = sorted(set(written) | {found.name for found in out.glob("*.cases")})
    digests = [(hashlib.sha256((out / name).read_bytes()).hexdigest(), name)
               for name in covered]
    write_record(out, "RECORDS.sha256", ["sha256 <TAB> file. Every record above is listed here."],
                 digests)


def worker(out):
    try:
        import ament_index_python.packages  # noqa: F401
    except ImportError:
        sys.exit("oracle: the vendored package index is not on PYTHONPATH; nothing was recorded")
    measure(out, measured_pins())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", action="append", default=[])
    parser.add_argument("--out", default=str(REPO / "tests" / "golden" / "oracle"))
    parser.add_argument("--venv", default=str(REPO / "build" / "oracle-venv"))
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        return worker(Path(args.out))
    if not args.package_root:
        parser.error("--package-root names the directory the descriptions resolve against")
    environment = dict(os.environ)
    # Every import-path entry precedes the environment's own site-packages, and an empty entry --
    # which is what joining an unset inherited value yields -- is the working directory.
    environment["PYTHONPATH"] = str(HERE)
    environment.pop("PYTHONHOME", None)
    environment["ORACLE_PACKAGE_ROOTS"] = os.pathsep.join(
        str(one) for one in package_roots(args.package_root))
    python = bootstrap(Path(args.venv))
    subprocess.run([str(python), __file__, "--worker", "--out", args.out],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
