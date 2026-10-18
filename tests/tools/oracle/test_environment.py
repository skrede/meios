import sys
import tempfile
import unittest
from unittest import mock

from recorder import environment
from recorder.rendering import same_length


class fake_metadata:
    def __init__(self, versions):
        self.versions = versions

    def version(self, name):
        return self.versions[name]


def pinned():
    return {name: want for name, want in environment.PINS}


class measured_pins_refusal(unittest.TestCase):
    def measure(self, versions):
        with mock.patch.object(environment, "metadata", fake_metadata(versions)), \
                mock.patch.object(environment, "imported_from_pin") as origin:
            result = environment.measured_pins()
        self.assertEqual(origin.call_count, len(environment.PINS))
        return result

    def test_pinned_versions_are_read_from_the_metadata(self):
        self.assertEqual(self.measure(pinned()), pinned())

    def test_each_drifted_distribution_refuses(self):
        for name, want in environment.PINS:
            with self.subTest(name=name):
                drifted = dict(pinned(), **{name: want + ".post1"})
                with self.assertRaisesRegex(SystemExit, "holds {} {}".format(name, want)):
                    self.measure(drifted)

    def test_module_imported_from_outside_the_environment_refuses(self):
        with tempfile.TemporaryDirectory() as elsewhere, \
                mock.patch.object(sys, "prefix", elsewhere):
            with self.assertRaisesRegex(SystemExit, "outside the environment"):
                environment.imported_from_pin("json")


class same_length_refusal(unittest.TestCase):
    def test_short_measurement_refuses(self):
        with self.assertRaisesRegex(SystemExit, "rendering produced 2 nodes where 3 were probed"):
            same_length("rendering", "nodes", 2, 3)

    def test_long_measurement_refuses(self):
        with self.assertRaises(SystemExit):
            same_length("rendering", "nodes", 4, 3)

    def test_equal_lengths_pass(self):
        self.assertIsNone(same_length("rendering", "nodes", 3, 3))


if __name__ == "__main__":
    unittest.main()
