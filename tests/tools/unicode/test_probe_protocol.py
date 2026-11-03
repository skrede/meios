import subprocess
import sys
import unittest

PROBE = None


def run(feed):
    return subprocess.run([PROBE], input=feed.encode("ascii"), capture_output=True)


def answered(got):
    return got.stdout.decode("utf-8").splitlines()


class well_formed(unittest.TestCase):
    def test_a_code_point_answers_its_three_fields(self):
        got = run("41\n")
        self.assertEqual(got.returncode, 0)
        self.assertEqual(answered(got), ["41\t1\t-"])

    def test_the_empty_line_answers_three_fields(self):
        got = run("\n")
        self.assertEqual(got.returncode, 0)
        self.assertEqual([len(line.split("\t")) for line in answered(got)], [3])


class malformed(unittest.TestCase):
    def test_a_line_that_is_not_single_spaced_code_points_refuses_naming_it(self):
        for line in ("41 zz", "41,42", "41  42", " 41", "41 ", "110000"):
            with self.subTest(line=line):
                got = run(line + "\n")
                self.assertNotEqual(got.returncode, 0)
                self.assertEqual(answered(got), [])
                self.assertIn("line 1 ", got.stderr.decode("utf-8"))

    def test_the_lines_before_a_malformed_one_are_still_answered(self):
        got = run("41\n41 zz\n")
        self.assertNotEqual(got.returncode, 0)
        self.assertEqual(answered(got), ["41\t1\t-"])
        self.assertIn("line 2 ", got.stderr.decode("utf-8"))


if __name__ == "__main__":
    PROBE = sys.argv.pop()
    unittest.main()
