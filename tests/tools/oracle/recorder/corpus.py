import hashlib
from pathlib import Path
from collections import namedtuple

REPO = Path(__file__).resolve().parents[4]

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

