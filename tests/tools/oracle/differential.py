# Renders every minimized expression case, every minimized binding document and every
# native-evaluator corpus document fresh through upstream xacro, for the live differential
# comparator (native_differential_test.cpp) to read. Reuses bootstrap()/render()/package_roots()/
# expression_cases()/write_seed_documents() from the recorder package verbatim -- this file writes
# nothing but scratch renders; the committed records under tests/golden/oracle stay the
# recorder's sole output.

import os
import sys
import shutil
import contextlib
import argparse
import tempfile
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from recorder.corpus import REPO, CORPUS_DOCUMENTS, document_root, key_of  # noqa: E402
from recorder.rendering import render, escape, refusal_text, refusal_cause  # noqa: E402
from recorder.environment import bootstrap, package_roots  # noqa: E402
from recorder.expressions import expression_cases, write_seed_documents  # noqa: E402
from differential_probes import (DIVERGENCE_PROBES, RECURSIVE_YAML, DOCUMENT_PROBES,  # noqa: E402
                                 PROBE_DIR, AGREEMENT_PROBES, PROBE_DIRECTORY_ARGUMENT,
                                 DIRECTORY_BOUND_PROBES, probe_root)


def sanitize(case_id):
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in case_id)


def write_text(out, case_id, suffix, text):
    (out / (sanitize(case_id) + suffix)).write_text(text, encoding="utf-8", newline="\n")


def write_refusal(out, case_id, suffix, failure):
    fields = (*refusal_cause(failure), refusal_text(failure))
    write_text(out, case_id, suffix, "REFUSED\t" + "\t".join(fields) + "\n")


# Written as a tiny single-attribute document rather than the bare value, so the C++ comparator
# can run the same canonical_xml() normalizer this repository uses everywhere else, never a
# second one, over both an expression case's render and a corpus document's whole render.
def render_expression_case(tmp, out, seeds, case_id, expression):
    body = "%s\n <e v=\"${%s}\"/>" % (seeds, escape(expression))
    try:
        node = render(tmp, body).getElementsByTagName("e")[0]
        write_text(out, case_id, ".xml", '<e v="%s"/>\n' % escape(node.getAttribute("v")))
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


# The bare rendered value, not wrapped in XML: these ids feed the divergence manifest's
# exact-text match directly, not the structural comparator.
def render_divergence_probe(tmp, out, case_id, expression):
    body = ' <e v="${%s}"/>' % escape(expression)
    try:
        node = render(tmp, body).getElementsByTagName("e")[0]
        write_text(out, case_id, ".txt", node.getAttribute("v") + "\n")
    except Exception as failure:
        write_refusal(out, case_id, ".txt", failure)


# Every file sharing the probe's stem travels with it, and so does a <stem>.d/ directory, links as
# links, so a document opened through a link resolves its includes where upstream would resolve
# them. Probes sharing a fixture stage the same entries, so one already staged is kept. A probe
# with no fixture raises rather than rendering, so it can never be recorded as a refusal.
def stage_probe(tmp, case_id):
    document = PROBE_DIR / (case_id + ".xacro")
    if not document.is_file():
        raise FileNotFoundError("no probe fixture " + str(document))
    for one in sorted(PROBE_DIR.glob(case_id + ".*")):
        if os.path.lexists(tmp / one.name):
            continue
        if one.is_dir() and not one.is_symlink():
            shutil.copytree(one, tmp / one.name, symlinks=True)
        else:
            shutil.copy(one, tmp / one.name, follow_symlinks=False)
    return tmp / document.name


# The package index reads its roots at call time, so a probe's own root and working directory hold
# only while it renders; every other probe keeps the corpus roots and the worker's directory.
@contextlib.contextmanager
def probe_package_root(tmp, case_id):
    root = probe_root(case_id, tmp)
    if root is None:
        yield
        return
    saved_directory, saved_roots = os.getcwd(), os.environ.get("ORACLE_PACKAGE_ROOTS")
    try:
        os.environ["ORACLE_PACKAGE_ROOTS"] = root
        os.chdir(tmp)
        yield
    finally:
        os.chdir(saved_directory)
        if saved_roots is None:
            os.environ.pop("ORACLE_PACKAGE_ROOTS", None)
        else:
            os.environ["ORACLE_PACKAGE_ROOTS"] = saved_roots


# Driven through process_file rather than render(), which wraps a body in its own <robot> and
# would defeat the point of a document-level probe.
def render_document_probe(tmp, out, case_id):
    import xacro

    staged = stage_probe(tmp, case_id)
    mappings = {PROBE_DIRECTORY_ARGUMENT: str(tmp)} if case_id in DIRECTORY_BOUND_PROBES else None
    try:
        with probe_package_root(tmp, case_id):
            rendered = xacro.process_file(str(staged), mappings=mappings).toxml()
        write_text(out, case_id, ".txt", rendered)
    except Exception as failure:
        write_refusal(out, case_id, ".txt", failure)


# Upstream writes every declared default into the mapping it is handed, so each render is handed
# its own copy rather than the table's.
def render_agreement_probe(tmp, out, case_id, fixture, mappings):
    import xacro

    staged = stage_probe(tmp, fixture)
    try:
        with probe_package_root(tmp, case_id):
            rendered = xacro.process_file(str(staged), mappings=dict(mappings) if mappings else None)
        write_text(out, case_id, ".xml", rendered.toxml())
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


# The entry points are the recorder's own table, read rather than repeated: a second list of the
# same descriptions is a rename away from rendering one document and comparing another.
def render_corpus_document(share, out, one):
    import xacro

    case_id = key_of(one)
    try:
        rendered = xacro.process_file(str(document_root(share, one.share) / one.document),
                                      mappings=one.mappings)
        write_text(out, case_id, ".xml", rendered.toxml())
    except Exception as failure:
        write_refusal(out, case_id, ".xml", failure)


def render_all(tmp, share, out):
    ur = share("ur_description")
    seeds = write_seed_documents(tmp)
    (tmp / "recursive.yaml").write_text(RECURSIVE_YAML, encoding="utf-8", newline="\n")
    for case_id, expression in expression_cases(ur):
        render_expression_case(tmp, out, seeds, case_id, expression)
    for case_id, expression in DIVERGENCE_PROBES:
        render_divergence_probe(tmp, out, case_id, expression)
    for case_id in DOCUMENT_PROBES:
        render_document_probe(tmp, out, case_id)
    for case_id, fixture, mappings in AGREEMENT_PROBES:
        render_agreement_probe(tmp, out, case_id, fixture, mappings)
    for one in CORPUS_DOCUMENTS:
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
    parser.add_argument("--venv", default=str(REPO / "build" / "oracle-venv"))
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if args.worker:
        return worker(out)
    if not args.package_root:
        parser.error("--package-root names the directory the descriptions resolve against")
    environment = dict(os.environ)
    environment["PYTHONPATH"] = str(HERE)
    environment.pop("PYTHONHOME", None)
    environment["ORACLE_PACKAGE_ROOTS"] = os.pathsep.join(
        str(one) for one in package_roots(args.package_root))
    python = bootstrap(Path(args.venv))
    subprocess.run([str(python), __file__, "--worker", "--out", str(out)],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
