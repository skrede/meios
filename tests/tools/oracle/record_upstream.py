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

PINS = (("xacro", "2.1.1"), ("pyyaml", "6.0.3"))
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
SEEDS = {"safety_pos_margin": "0.15", "mass": "3.7", "radius": "0.06", "length": "0.12",
         "wrist_3_joint_type": "continuous", "name": "base", "type": "visual",
         "sec_mesh_files": "${xacro.load_yaml(seed_file)['mesh_files']}"}
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


def facts_rows(doc):
    links = doc.getElementsByTagName("link")
    joints = doc.getElementsByTagName("joint")
    rows = [("link.count", str(len(links))), ("joint.count", str(len(joints)))]
    rows += [("link.name.{}".format(at), l.getAttribute("name")) for at, l in enumerate(links)]
    rows += [("joint.name.{}".format(at), j.getAttribute("name")) for at, j in enumerate(joints)]
    for joint in joints:
        rows.append(("joint.type." + joint.getAttribute("name"), joint.getAttribute("type")))
        for limit in children(joint, "limit"):
            rows.append(("joint.limit." + joint.getAttribute("name"),
                         attribute_text(limit, ("lower", "upper", "effort", "velocity"))))
    meshes = []
    for mesh in doc.getElementsByTagName("mesh"):
        if mesh.getAttribute("filename") not in meshes:
            meshes.append(mesh.getAttribute("filename"))
    rows += [("mesh.filename.{}".format(at), m) for at, m in enumerate(meshes)]
    for link in links:
        rows += origin_rows(link)
    return rows


def robot_facts(share, document, mappings):
    import xacro

    return facts_rows(xacro.process_file(str(share / document), mappings=mappings))


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


def seed_body(seed_file):
    rows = [' <xacro:property name="seed_file" value="%s"/>' % escape(str(seed_file))]
    rows += [' <xacro:property name="%s_parameters_file" value="%s"/>' % (n, escape(str(seed_file)))
             for n in ("joint_limits", "kinematics", "physical", "visual")]
    rows += [' <xacro:property name="%s" value="%s"/>' % (n, escape(v)) for n, v in SEEDS.items()]
    for at, joint in enumerate(LIMIT_SEEDS):
        rows += [' <xacro:property name="%s_%s_limit" value="%s"/>' % (joint, edge, sign * (6.0 + at))
                 for edge, sign in (("lower", -1.0), ("upper", 1.0))]
    return "\n".join(rows)


# A run in which the environment rather than the expression produced the failures would otherwise
# be recorded as a corpus of expected refusals, and the digest gate would then certify it. The floor
# leaves room for a genuine future refusal without admitting a wholesale failure.
REFUSAL_FLOOR = 2


# The record carries the failure's first line and nothing more, because a multi-line message would
# put an embedded newline into a tab-delimited row.
def refusal_text(failure):
    lines = str(failure).splitlines()
    return lines[0] if lines and lines[0].strip() else type(failure).__name__


def expression_rows(tmp, share, seed_file):
    rows = []
    seeds = seed_body(seed_file)
    for at, expression in enumerate(closure_expressions(share)):
        body = '%s\n <e v="${%s}"/>' % (seeds, escape(expression))
        try:
            text = render(tmp, body).getElementsByTagName("e")[0].getAttribute("v")
            rows.append(("e{:02d}".format(at + 1), expression, text, ""))
        except Exception as failure:
            rows.append(("e{:02d}".format(at + 1), expression, "REFUSED", refusal_text(failure)))
    refused = sum(1 for one in rows if one[2] == "REFUSED")
    if refused > REFUSAL_FLOOR:
        sys.exit("oracle: {} expressions refused where at most {} is a measurement rather than a "
                 "broken environment; nothing was recorded".format(refused, REFUSAL_FLOOR))
    return rows


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


def pin_rows(versions):
    listfile = (REPO / "cmake" / "corpus.cmake").read_text(encoding="utf-8")
    digest = re.search(r"NAME ur_description.*?HASH SHA256=([0-9a-f]+)", listfile, re.S)
    return [("xacro", versions["xacro"], "-"), ("PyYAML", versions["pyyaml"], "-"),
            ("ur_description", "4.3.1", digest.group(1))]


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
                          "non-trivial form in the closure of the Universal Robots document, each",
                          "driven through a minimized document seeding the names it reads. A",
                          "refusing row renders REFUSED and carries the failure's first line."],
    "yaml_scalars.cases": ["source <TAB> resolved kind <TAB> rendered text. The source is the",
                           "scalar exactly as written after the key; an empty source column is a",
                           "key written with no value at all."],
    "facts": ["fact <TAB> value, measured from the document upstream renders. A joint that",
              "declares no limit element carries no joint.limit row at all."],
}


def records(tmp, share, versions):
    ur = share("ur_description")
    kuka = share("kuka_kr6_support").parent
    facts = HEADERS["facts"]
    return {
        "PINS": (HEADERS["PINS"], pin_rows(versions)),
        "rendering.cases": (HEADERS["rendering.cases"], rendering_rows(tmp, ur / "config" / "ur5e")),
        "expressions.cases": (HEADERS["expressions.cases"],
                              expression_rows(tmp, ur, tmp / "seed.yaml")),
        "yaml_scalars.cases": (HEADERS["yaml_scalars.cases"], scalar_rows(tmp, ur)),
        "ur5e_facts.cases": (facts, robot_facts(ur, "urdf/ur.urdf.xacro",
                                                {"ur_type": "ur5e", "name": "ur"})),
        "ur3e_facts.cases": (facts, robot_facts(ur, "urdf/ur.urdf.xacro",
                                                {"ur_type": "ur3e", "name": "ur"})),
        "kr6_facts.cases": (facts, robot_facts(kuka, "kuka_kr6_support/urdf/kr6r900sixx.xacro",
                                               {})),
    }


def measure(out, versions):
    from ament_index_python.packages import get_package_share_directory

    with tempfile.TemporaryDirectory() as scratch:
        tmp = Path(scratch)
        (tmp / "seed.yaml").write_text(SEED_YAML, encoding="utf-8", newline="\n")
        written = records(tmp, lambda name: Path(get_package_share_directory(name)), versions)
    out.mkdir(parents=True, exist_ok=True)
    for name, (header, rows) in written.items():
        write_record(out, name, header, rows)
    digests = [(hashlib.sha256((out / name).read_bytes()).hexdigest(), name)
               for name in sorted(written)]
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
