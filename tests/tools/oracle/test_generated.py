import re
import hashlib
import unittest

from recorder import generated
from recorder.generated_families import FAMILIES

MACHINE_PATH = re.compile(r"/home/|/tmp/|/Users/|[A-Za-z]:\\")


def family_texts(name):
    return [text for _, one, text in generated.generated_cases() if one.name == name]


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

    def test_the_include_family_is_unchanged(self):
        texts = "\n".join(family_texts("include")).encode("utf-8")
        self.assertEqual(hashlib.sha256(texts).hexdigest(),
                         "c2236fc5dfd72d4eea036e2d94b4ae71620dcc46ca00ade05ab6113e2b6698f5")

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


if __name__ == "__main__":
    unittest.main()
