import io
import os
import re
import sys
import shutil
import contextlib

from recorder.corpus import REPO
from recorder.rendering import escape, refusal_text, same_length
from recorder.generated_families import ESCAPED, FAMILIES

RECORD = "generated_cases.cases"
SEED = 0x6D65696F73
COUNT = 200
MASK = (1 << 64) - 1
FIXTURES = REPO / "tests" / "fixtures" / "xacro" / "generated"


# splitmix64: Steele, Lea and Flood, "Fast Splittable Pseudorandom Number Generators", OOPSLA 2014.
# Self-contained so the generated set never depends on what an interpreter's own generator does.
class splitmix64:
    def __init__(self, seed):
        self.state = seed & MASK

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
        return z ^ (z >> 31)


class draws:
    def __init__(self, seed):
        self.source = splitmix64(seed)

    def below(self, bound):
        return self.source.next() % bound

    def pick(self, options):
        return options[self.below(len(options))]

    def between(self, low, high):
        return low + self.below(high - low + 1)

    def weighted(self, pairs):
        at = self.below(sum(weight for _, weight in pairs))
        for option, weight in pairs:
            if at < weight:
                return option
            at -= weight
        raise AssertionError("unreachable")

    def distinct(self, options, count):
        left = list(options)
        return [left.pop(self.below(len(left))) for _ in range(count)]


def generated_cases():
    draw = draws(SEED)
    return [("{}_{:03d}".format(one.name, at + 1), one, one.case(draw))
            for one in FAMILIES for at in range(COUNT)]


def document(one, text):
    fields = dict(zip(one.slots, text.split("\t")))
    return re.sub(r"@([A-Z]+)@", lambda m: escape(fields[m.group(1)])
                  if m.group(1) in ESCAPED else fields[m.group(1)], one.template)


def column(text):
    return text.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def verdict(rendered, normal):
    return column(normal(rendered.documentElement.toxml()))


def normalizer(stage):
    pairs = ((str(stage / "R"), "@ROOT@"), (str(stage / "doc"), "@DOC@"))

    def normal(text):
        for path, token in pairs:
            text = text.replace(path, token)
        if str(stage) in text:
            sys.exit("oracle: a staging path survived normalization in {!r}; nothing was recorded"
                     .format(text))
        return text
    return normal


# Upstream writes declared argument defaults into the mapping it is handed, so each render gets its
# own.
def render_case(source, normal):
    import xacro

    try:
        with contextlib.redirect_stderr(io.StringIO()):
            rendered = xacro.process_file(str(source), mappings={})
        return verdict(rendered, normal), ""
    except Exception as failure:
        return "REFUSED", normal(refusal_text(failure))


@contextlib.contextmanager
def package_root(root):
    saved = os.environ.get("ORACLE_PACKAGE_ROOTS")
    os.environ["ORACLE_PACKAGE_ROOTS"] = str(root)
    try:
        yield
    finally:
        if saved is None:
            os.environ.pop("ORACLE_PACKAGE_ROOTS", None)
        else:
            os.environ["ORACLE_PACKAGE_ROOTS"] = saved


def generated_rows(tmp):
    stage = tmp / "generated"
    shutil.copytree(FIXTURES, stage)
    source, normal, rows = stage / "doc" / "case.xacro", normalizer(stage), []
    with package_root(stage / "R"):
        for case_id, one, text in generated_cases():
            source.write_text(document(one, text), encoding="utf-8", newline="\n")
            upstream, refusal = render_case(source, normal)
            rows.append((case_id, one.name, column(text), upstream, column(refusal)))
    same_length("the generated set", "rows", len(rows), COUNT * len(FAMILIES))
    return rows


def generated_header():
    return ["id <TAB> family <TAB> text <TAB> upstream <TAB> refusal. A seeded, fixed-size set of",
            "include filenames, dollar shapes and macro specifications, each written into its",
            "family's template below and rendered by the pinned reference as doc/case.xacro beside",
            "tests/fixtures/xacro/generated, whose R directory is the only package root. The text",
            "and refusal columns escape a backslash, a tab and a newline as \\\\, \\t and \\n. A",
            "slot the escaped line names is written into an attribute with &, < and \" escaped,",
            "and every other slot is markup written as it is. A multi-slot text separates its",
            "fields with a tab, in the order its slots line names. The upstream column is",
            "REFUSED, or the rendered document element as the reference writes it, each staged",
            "path in it replaced by the token the next line names and escaped as the text column",
            "is. Both sides compare it as the project's own canonical text, so every element and",
            "attribute is compared.",
            "The staged package root reads @ROOT@ and the staged document directory @DOC@. The",
            "refusal column carries a refusal's first line and is not compared.",
            "seed 0x{:x}".format(SEED),
            *("count {} {}".format(one.name, COUNT) for one in FAMILIES),
            "escaped " + " ".join(ESCAPED),
            *("slots {} {}".format(one.name, " ".join(one.slots)) for one in FAMILIES),
            *("template {} {}".format(one.name, one.template) for one in FAMILIES)]
