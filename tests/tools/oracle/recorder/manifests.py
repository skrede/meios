import re
import sys

from recorder.corpus import AUTHORED, CORPUS_DOCUMENTS, REPO, content_digest

CORPUS_PIN = "4.3.1"
LBR_PIN = "2.5.0"
FRANKA_PIN = "2.8.1"
KORTEX_PIN = "0.2.5"


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
    if not found:
        sys.exit("oracle: the listfile names no archive digest for {}; nothing was recorded"
                 .format(name))
    return found.group(1)


# A package that tags releases is pinned by the version its manifest declares; one fetched at a
# bare commit has no release to name, so the commit its archive URL carries is the revision.
VERSION_PINS = {"ur_description": CORPUS_PIN, "lbr_med14_r820_description": LBR_PIN,
                "franka_description": FRANKA_PIN, "kortex_description": KORTEX_PIN}


def archive_revision(listfile, name):
    found = re.search(r"NAME {}\s+URL\s+\S*/archive/([0-9a-f]{{40}})\.tar\.gz"
                      .format(re.escape(name)), listfile)
    if not found:
        sys.exit("oracle: {} is pinned neither by a version nor by an archive commit; nothing was "
                 "recorded".format(name))
    return found.group(1)


def source_pin(share, listfile, name):
    if name == AUTHORED:
        return (AUTHORED, "in-repo", content_digest())
    revision = package_version(share, name, VERSION_PINS[name]) if name in VERSION_PINS \
        else archive_revision(listfile, name)
    return (name, revision, package_digest(listfile, name))


# One row per source behind a record, derived from the entry points themselves, so a source added
# to the corpus cannot produce measurements this record does not name.
def pin_rows(versions, share):
    listfile = (REPO / "cmake" / "corpus.cmake").read_text(encoding="utf-8")
    sources = dict.fromkeys(one.upstream for one in CORPUS_DOCUMENTS)
    return [("xacro", versions["xacro"], "-"), ("PyYAML", versions["pyyaml"], "-")] \
        + [source_pin(share, listfile, name) for name in sources]

