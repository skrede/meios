import hashlib
import tempfile
from pathlib import Path

from recorder.headers import HEADERS
from recorder.yaml_rows import collision_rows, rendering_rows, scalar_rows, unit_tag_rows
from recorder.facts import robot_facts
from recorder.ledger import LEDGER, ledger_rows
from recorder.corpus import CORPUS_DOCUMENTS, document_root, key_of
from recorder.manifests import pin_rows
from recorder.expressions import expression_rows


def write_record(out, name, header, rows):
    text = "".join("# " + line + "\n" for line in header)
    text += "".join("\t".join(row) + "\n" for row in rows)
    (out / name).write_text(text, encoding="utf-8", newline="\n")


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

