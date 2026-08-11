# Renders every minimized expression case and every native-evaluator corpus document fresh
# through upstream xacro, for the live differential comparator (native_differential_test.cpp) to
# read. Reuses bootstrap()/render()/package_roots()/closure_expressions()/seed_body() from
# record_upstream.py verbatim -- this file writes nothing but scratch renders; the committed
# records under tests/golden/oracle stay the recorder's sole output.

import os
import sys
import argparse
import tempfile
import subprocess
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import record_upstream as oracle  # noqa: E402

# The evaluation targets a native-evaluator corpus document resolves against: a package share
# name (as ament_index_python resolves it), the document's own path under that share, its
# key=value arguments, and the id corpus::record_for() keys the same document by in C++ -- kept
# in that order so a fetch-side rename shows up as a diff here rather than a silent mismatch.
CORPUS_DOCS = (
    ("ur_description", "urdf/ur.urdf.xacro", {"ur_type": "ur5e", "name": "ur"}, "ur_type=ur5e"),
    ("ur_description", "urdf/ur.urdf.xacro", {"ur_type": "ur3e", "name": "ur"}, "ur_type=ur3e"),
    ("ur_description", "urdf/ur.urdf.xacro", {"ur_type": "ur7e", "name": "ur"}, "ur_type=ur7e"),
    ("ur_description", "urdf/ur.urdf.xacro",
     {"ur_type": "ur5e", "name": "ur", "safety_limits": "true"}, "safety_limits=true ur_type=ur5e"),
    ("ur_description", "urdf/ur.urdf.xacro",
     {"ur_type": "ur5e", "name": "ur", "force_abs_paths": "true"},
     "force_abs_paths=true ur_type=ur5e"),
    ("kuka_kr6_support", "kuka_kr6_support/urdf/kr6r900sixx.xacro", {}, "kr6r900sixx.xacro"),
    ("lbr_med14_r820_description", "urdf/lbr_med14_r820.urdf.xacro", {}, "lbr_med14_r820.urdf.xacro"),
)

# The two known divergences (D-12, measured in a prior phase and deliberately not changed):
# and/or yielding a boolean rather than the deciding operand. Self-contained expressions, so
# neither needs the expressions.cases seed scope.
DIVERGENCE_PROBES = (("div_or_operand", "1 or 2"), ("div_and_operand", "2 and 3"))


def sanitize(case_id):
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in case_id)


def write_text(out, case_id, suffix, text):
    (out / (sanitize(case_id) + suffix)).write_text(text, encoding="utf-8", newline="\n")


def write_refusal(out, case_id, suffix, failure):
    write_text(out, case_id, suffix, "REFUSED\t" + oracle.refusal_text(failure) + "\n")


# Written as a tiny single-attribute document rather than the bare value, so the C++ comparator
# can run the same canonical_xml() normalizer this repository uses everywhere else, never a
# second one, over both an expression case's render and a corpus document's whole render.
def render_expression_case(tmp, out, seeds, case_id, expression):
    body = "%s\n <e v=\"${%s}\"/>" % (seeds, oracle.escape(expression))
    try:
        node = oracle.render(tmp, body).getElementsByTagName("e")[0]
        write_text(out, case_id, ".xml", '<e v="%s"/>\n' % oracle.escape(node.getAttribute("v")))
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


# The bare rendered value, not wrapped in XML: these two ids feed the divergence manifest's
# exact-text match directly, not the structural comparator.
def render_divergence_probe(tmp, out, case_id, expression):
    body = ' <e v="${%s}"/>' % oracle.escape(expression)
    try:
        node = oracle.render(tmp, body).getElementsByTagName("e")[0]
        write_text(out, case_id, ".txt", node.getAttribute("v") + "\n")
    except Exception as failure:
        write_refusal(out, case_id, ".txt", failure)


def render_corpus_document(share, out, share_name, document, mappings, case_id):
    import xacro

    root = share(share_name)
    if share_name == "kuka_kr6_support":
        root = root.parent
    try:
        rendered = xacro.process_file(str(root / document), mappings=mappings)
        write_text(out, case_id, ".xml", rendered.toxml())
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


def render_all(tmp, share, out):
    seed_file = tmp / "seed.yaml"
    seed_file.write_text(oracle.SEED_YAML, encoding="utf-8", newline="\n")
    ur = share("ur_description")
    seeds = oracle.seed_body(seed_file)
    for at, expression in enumerate(oracle.closure_expressions(ur)):
        render_expression_case(tmp, out, seeds, "e{:02d}".format(at + 1), expression)
    for case_id, expression in DIVERGENCE_PROBES:
        render_divergence_probe(tmp, out, case_id, expression)
    for share_name, document, mappings, case_id in CORPUS_DOCS:
        render_corpus_document(share, out, share_name, document, mappings, case_id)


def worker(out):
    try:
        import ament_index_python.packages  # noqa: F401
    except ImportError:
        sys.exit("differential: the vendored package index is not on PYTHONPATH; nothing was rendered")
    from ament_index_python.packages import get_package_share_directory

    with tempfile.TemporaryDirectory() as scratch:
        render_all(Path(scratch), lambda name: Path(get_package_share_directory(name)), out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", action="append", default=[])
    parser.add_argument("--out", required=True)
    parser.add_argument("--venv", default=str(oracle.REPO / "build" / "oracle-venv"))
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if args.worker:
        return worker(out)
    if not args.package_root:
        parser.error("--package-root names the directory the descriptions resolve against")
    environment = dict(os.environ)
    environment["PYTHONPATH"] = str(oracle.HERE)
    environment.pop("PYTHONHOME", None)
    environment["ORACLE_PACKAGE_ROOTS"] = os.pathsep.join(
        str(one) for one in oracle.package_roots(args.package_root))
    python = oracle.bootstrap(Path(args.venv))
    subprocess.run([str(python), __file__, "--worker", "--out", str(out)],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
