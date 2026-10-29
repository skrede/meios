import re
import hashlib
import unittest

from recorder import generated
from recorder.rendering import refusal_cause
from recorder.generated_families import FAMILIES

MACHINE_PATH = re.compile(r"/home/|/tmp/|/Users/|[A-Za-z]:\\")
PARAMETER = re.compile(r"(\*{0,2})([a-z])(:?=|$)")


class wrapping(Exception):
    def __init__(self, inner):
        super().__init__("wrapped")
        self.exc = inner


def family_texts(name):
    return [text for _, one, text in generated.generated_cases() if one.name == name]


def family_hash(name):
    return hashlib.sha256("\n".join(family_texts(name)).encode("utf-8")).hexdigest()


def declared_and_called(text):
    spec, attributes, _, _ = text.split("\t")
    matched = [PARAMETER.match(token) for token in spec.split()]
    declared = [one.group(2) for one in matched if one and not one.group(1)]
    return declared, re.findall(r'(\w+)="', attributes)


class generated_set(unittest.TestCase):
    def test_two_generations_are_equal(self):
        first = [(case_id, one.name, text) for case_id, one, text in generated.generated_cases()]
        second = [(case_id, one.name, text) for case_id, one, text in generated.generated_cases()]
        self.assertEqual(first, second)

    def test_every_family_yields_the_committed_count(self):
        self.assertEqual(generated.COUNT, 200)
        self.assertEqual([one.name for one in FAMILIES], ["include", "span", "macro"])
        for one in FAMILIES:
            self.assertEqual(len(family_texts(one.name)), 200)

    def test_ids_name_their_family_and_position(self):
        ids = [case_id for case_id, _, _ in generated.generated_cases()]
        self.assertEqual(ids[0], "include_001")
        self.assertEqual(len(set(ids)), len(ids))

    def test_the_sequence_is_splitmix64(self):
        published = generated.splitmix64(0)
        self.assertEqual(published.next(), 0xE220A8397B1DCDAF)
        self.assertEqual(published.next(), 0x6E789E6AA1B965F4)
        seeded = generated.splitmix64(generated.SEED)
        self.assertEqual(generated.SEED, 0x6D65696F73)
        self.assertEqual([seeded.next() for _ in range(3)],
                         [0x6AF68C71D027A653, 0x99F0739CCC5D998C, 0xC3F5347EF1F236AB])

    def test_the_include_family_is_pinned(self):
        self.assertEqual(family_hash("include"),
                         "bff9e89e4893daca8d9b726db5616b3824ae605410ec4f873ab1d017d4b7ca16")

    def test_the_macro_family_is_pinned(self):
        self.assertEqual(family_hash("macro"),
                         "daeff020b727679be51349a68fa3c42bd736856431af3959978b544f733780ce")

    def test_the_span_family_is_pinned(self):
        self.assertEqual(family_hash("span"),
                         "8783260e57bd0a6d2e4604c7630f4b5ee22f22262ae17d73d84ad7112f229838")

    def test_call_attributes_name_declared_parameters(self):
        undeclared = 0
        for text in family_texts("macro"):
            declared, called = declared_and_called(text)
            self.assertEqual(len(called), len(set(called)), text)
            for name in called:
                self.assertTrue(name in declared or name == "d", text)
            undeclared += "d" in called
        self.assertGreater(undeclared, 0)

    def test_a_doubly_declared_parameter_is_called(self):
        twice = [text for text in family_texts("macro")
                 if any(declared_and_called(text)[0].count(name) > 1
                        for name in declared_and_called(text)[1])]
        self.assertTrue(twice)

    def test_no_text_or_template_holds_a_machine_path(self):
        repo = str(generated.REPO)
        for _, one, text in generated.generated_cases():
            self.assertNotRegex(text, MACHINE_PATH)
            self.assertNotIn(repo, text)
        for line in generated.generated_header():
            self.assertNotRegex(line, MACHINE_PATH)
            self.assertNotIn(repo, line)

    def test_every_slot_of_every_case_is_written(self):
        for _, one, text in generated.generated_cases():
            self.assertEqual(len(text.split("\t")), len(one.slots))
            self.assertNotIn("@", generated.document(one, text))

    def test_a_text_is_written_escaped_into_its_attribute(self):
        include = FAMILIES[0]
        written = generated.document(include, 'a&b<"c')
        self.assertIn('filename="a&amp;b&lt;&quot;c"', written)

    def test_a_refusal_records_its_class_and_the_class_it_wraps(self):
        self.assertEqual(refusal_cause(wrapping(NameError("name 'x' is not defined"))),
                         ("wrapping", "NameError"))
        self.assertEqual(refusal_cause(RuntimeError()), ("RuntimeError", "-"))
        self.assertEqual(refusal_cause(wrapping(None)), ("wrapping", "-"))

    def test_every_recorded_case_names_its_cause(self):
        record = generated.REPO / "tests" / "golden" / "oracle" / generated.RECORD
        lines = record.read_text(encoding="utf-8").splitlines()
        rows = [line.split("\t") for line in lines if not line.startswith("#")]
        self.assertEqual(len(rows), generated.COUNT * len(FAMILIES))
        for row in rows:
            self.assertEqual(len(row), 6, row[0])
            if row[3] == "REFUSED":
                self.assertRegex(row[4], r"^\w+ (\w+|-)$", row[0])
            else:
                self.assertEqual((row[4], row[5]), ("-", ""), row[0])


if __name__ == "__main__":
    unittest.main()
