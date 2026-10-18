from recorder.corpus import AUTHORED, CORPUS_DOCUMENTS, REPO, content_digest, key_of
from recorder.manifests import package_digest

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

