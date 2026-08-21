# Renders every minimized expression case and every native-evaluator corpus document fresh
# through upstream xacro, for the live differential comparator (native_differential_test.cpp) to
# read. Reuses bootstrap()/render()/package_roots()/expression_cases()/write_seed_documents() from
# record_upstream.py verbatim -- this file writes nothing but scratch renders; the committed
# records under tests/golden/oracle stay the recorder's sole output.

import os
import sys
import shutil
import argparse
import tempfile
import subprocess
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import record_upstream as oracle  # noqa: E402

# The known divergences, measured and deliberately not changed: and/or yielding a boolean rather
# than the deciding operand, a self-referential alias graph that loads here as a value containing
# itself, the two mapping-constructor argument shapes upstream accepts that are refused here, the
# four string meanings upstream carries that this grammar does not -- repetition, ordering, and
# the split's two further argument shapes -- the one mathematics name this grammar answers and
# upstream does not, and three differences in what the span scanner hands the evaluator,
# upstream's own span pattern being ^\$\{[^\}]*\} and so neither quote-aware nor nesting. None
# needs the expressions.cases seed scope -- the third reads only the document written beside the
# probe, which load_yaml resolves relative to the probe itself.
DIVERGENCE_PROBES = (("div_or_operand", "1 or 2"),
                     ("div_and_operand", "2 and 3"),
                     ("div_self_reference", "xacro.load_yaml('recursive.yaml')['a']"),
                     ("div_dict_pair_sequence", "dict([('a', 1)])"),
                     ("div_dict_mixed_arguments", "dict([('a', 1)], b=2)"),
                     ("div_string_repetition", "'ab' * 3"),
                     ("div_string_ordering", "'a' < 'b'"),
                     ("div_string_split_whitespace", "'a b'.split()"),
                     ("div_string_split_limit", "'a b c'.split(' ', 1)"),
                     ("div_absolute_value", "abs(-3)"),
                     ("div_span_closing_brace_in_literal", "'a}b'"),
                     ("div_span_format_operator", "'%.3f' % 1.2345"),
                     ("div_span_nested_expression", "${'1 + 2'}"))
RECURSIVE_YAML = "a: &a [1, *a]\n"

# A difference needing a macro, a conditional and a call site has no expression to wrap, so its
# probe is a whole committed document read from tests/fixtures/xacro/probes by both sides. Ids
# only: the document is the fixture, and repeating it here is what a rename would silently defeat.
DOCUMENT_PROBES = ("non_text_mapping_key",)
PROBE_DIR = oracle.REPO / "tests" / "fixtures" / "xacro" / "probes"


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


# The bare rendered value, not wrapped in XML: these ids feed the divergence manifest's
# exact-text match directly, not the structural comparator.
def render_divergence_probe(tmp, out, case_id, expression):
    body = ' <e v="${%s}"/>' % oracle.escape(expression)
    try:
        node = oracle.render(tmp, body).getElementsByTagName("e")[0]
        write_text(out, case_id, ".txt", node.getAttribute("v") + "\n")
    except Exception as failure:
        write_refusal(out, case_id, ".txt", failure)


# Every file sharing the probe's stem travels with it, so an auxiliary document beside the probe
# resolves document-relative in the scratch directory exactly as it does in the fixture directory.
def stage_probe(tmp, case_id):
    for one in sorted(PROBE_DIR.glob(case_id + ".*")):
        shutil.copy(one, tmp / one.name)
    return tmp / (case_id + ".xacro")


# Driven through process_file rather than render(), which wraps a body in its own <robot> and
# would defeat the point of a document-level probe.
def render_document_probe(tmp, out, case_id):
    import xacro

    try:
        write_text(out, case_id, ".txt", xacro.process_file(str(stage_probe(tmp, case_id))).toxml())
    except Exception as failure:
        write_refusal(out, case_id, ".txt", failure)


# The entry points are the recorder's own table, read rather than repeated: a second list of the
# same descriptions is a rename away from rendering one document and comparing another.
def render_corpus_document(share, out, one):
    import xacro

    case_id = oracle.key_of(one)
    try:
        rendered = xacro.process_file(str(oracle.document_root(share, one.share) / one.document),
                                      mappings=one.mappings)
        write_text(out, case_id, ".xml", rendered.toxml())
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


def render_all(tmp, share, out):
    ur = share("ur_description")
    seeds = oracle.write_seed_documents(tmp)
    (tmp / "recursive.yaml").write_text(RECURSIVE_YAML, encoding="utf-8", newline="\n")
    for case_id, expression in oracle.expression_cases(ur):
        render_expression_case(tmp, out, seeds, case_id, expression)
    for case_id, expression in DIVERGENCE_PROBES:
        render_divergence_probe(tmp, out, case_id, expression)
    for case_id in DOCUMENT_PROBES:
        render_document_probe(tmp, out, case_id)
    for one in oracle.CORPUS_DOCUMENTS:
        render_corpus_document(share, out, one)


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
