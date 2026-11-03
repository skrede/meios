"""Holds the core's identifier rules to the interpreter: every string of the definition,
normalization and fault domains goes through the identifier probe, and each answer is compared
with CPython's own."""
import ast
import keyword
import itertools
import subprocess
import tempfile
import unicodedata

from unicode_tables import LIMIT

CHUNK = 250000
SHOWN = 20


# xacro 2.1.1's is_valid_name (xacro/__init__.py:553-570), written as upstream writes it.
def upstream_valid(name):
    try:
        root = ast.parse(name)
    except SyntaxError:
        return False
    return (isinstance(root, ast.Module) and len(root.body) == 1 and isinstance(root.body[0], ast.Expr)
            and isinstance(root.body[0].value, ast.Name) and root.body[0].value.id == name)


def stored_form(text):
    return unicodedata.normalize("NFKC", text) if text.isidentifier() else None


# What the reader refuses an expression with is what compile() refuses it with: the lexer's
# message, or the parser's refusal of a name that normalizes to a constant.
def lexical_verdict(text):
    try:
        compile(text, "<expression>", "eval")
    except SyntaxError as error:
        return error.msg
    except ValueError as error:
        return str(error)
    return "-"


def definition_domain():
    yield ""
    yield from keyword.kwlist + keyword.softkwlist
    for point in range(LIMIT):
        if not 0xD800 <= point <= 0xDFFF:
            yield from (chr(point), "a" + chr(point), chr(point) + "a")


def fault_domain():
    yield from ("\uFF34rue", "\uFF26alse", "\uFF2Eone", "\uFF49\uFF46")
    for point in range(0x80, LIMIT):
        if not 0xD800 <= point <= 0xDFFF:
            yield from (chr(point), "a" + chr(point))


def canonical_pairs():
    for point in range(LIMIT):
        fields = unicodedata.decomposition(chr(point)).split()
        if len(fields) == 2 and not fields[0].startswith("<"):
            yield int(fields[0], 16), int(fields[1], 16), point


def hangul():
    for lead, vowel in itertools.product(range(0x1100, 0x1113), range(0x1161, 0x1176)):
        yield chr(lead) + chr(vowel)
        yield from (chr(lead) + chr(vowel) + chr(trail) for trail in range(0x11A7, 0x11C4))
    for syllable in range(0xAC00, 0xD7A4):
        yield from (chr(syllable) + chr(trail) for trail in (0x11A7, 0x11A8, 0x11C2, 0x11C3))


def chains(pairs):
    seconds_of = {}
    for first, second, _ in pairs:
        seconds_of.setdefault(first, []).append(second)
    for first, second, composite in pairs:
        yield from (chr(first) + chr(second) + chr(later) for later in seconds_of.get(composite, ()))


def normalization_domain():
    continuing = [chr(point) for point in range(LIMIT) if ("a" + chr(point)).isidentifier()]
    pairs = list(canonical_pairs())
    firsts = sorted({chr(first) for first, _, _ in pairs})
    seconds = sorted({chr(second) for _, second, _ in pairs})
    bases = sorted(set(firsts) | {chr(composite) for _, _, composite in pairs})
    marks = sorted({one for one in continuing if unicodedata.combining(one)} | set(seconds))
    yield from continuing
    yield from ("a" + one for one in continuing)
    yield from (base + mark for base, mark in itertools.product(bases, marks))
    yield from (first + one + two for first, one, two in itertools.product(firsts, seconds, seconds))
    yield from hangul()
    yield from chains(pairs)


def hex_points(text):
    return "-" if text is None else " ".join("{:X}".format(ord(one)) for one in text)


def answers(probe, texts):
    with tempfile.TemporaryFile("w+", encoding="ascii", newline="\n") as feed:
        feed.write("".join(hex_points(text) + "\n" for text in texts))
        feed.seek(0)
        run = subprocess.run([str(probe)], stdin=feed, capture_output=True, check=True)
    lines = run.stdout.decode("utf-8").split("\n")[:-1]
    if len(lines) != len(texts):
        raise SystemExit("unicode: the probe answered {} lines for {} strings".format(len(lines), len(texts)))
    return [line.split("\t") for line in lines]


class tally:
    def __init__(self):
        self.compared = {"normalization": 0, "definition": 0, "fault": 0}
        self.mismatched = {"normalization": 0, "definition": 0, "fault": 0}
        self.shown = []

    def record(self, field, text, have, want):
        self.compared[field] += 1
        if have != want:
            self.mismatched[field] += 1
            if len(self.shown) < SHOWN:
                self.shown.append((field, hex_points(text), have, want))


def compare(probe, domain, field, counts):
    while chunk := list(itertools.islice(domain, CHUNK)):
        for text, (form, valid, fault) in zip(chunk, answers(probe, chunk)):
            counts.record("normalization", text, form, hex_points(stored_form(text)))
            if field == "definition":
                counts.record("definition", text, valid, "1" if upstream_valid(text) else "0")
            if field == "fault":
                counts.record("fault", text, fault, lexical_verdict(text))


def check(probe):
    counts = tally()
    compare(probe, definition_domain(), "definition", counts)
    compare(probe, normalization_domain(), "normalization", counts)
    compare(probe, fault_domain(), "fault", counts)
    for field, compared in counts.compared.items():
        print("{}: {} strings compared, {} mismatches".format(field, compared, counts.mismatched[field]))
    for field, points, have, want in counts.shown:
        print("mismatch {} [{}]: probe {!r}, interpreter {!r}".format(field, points, have, want))
    return 1 if counts.shown else 0
