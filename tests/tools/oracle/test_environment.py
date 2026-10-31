import sys
import tempfile
import unittest
from pathlib import Path
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
                mock.patch.object(sys, "version_info", (3, 14, 0, "final", 0)), \
                mock.patch.object(environment, "imported_from_pin") as origin:
            result = environment.measured_pins()
        self.assertEqual(origin.call_count, len(environment.PINS))
        return result

    def test_pinned_versions_are_read_from_the_metadata(self):
        self.assertEqual(self.measure(pinned()), dict(pinned(), python="3.14"))

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


class interpreter_pin(unittest.TestCase):
    def test_bootstrap_under_another_minor_refuses_before_any_environment(self):
        with tempfile.TemporaryDirectory() as scratch, \
                mock.patch.object(sys, "version_info", (3, 12, 3, "final", 0)), \
                mock.patch.object(environment.subprocess, "run") as run:
            venv = Path(scratch) / "venv"
            with self.assertRaisesRegex(SystemExit, "Python 3.12, where 3.14 is pinned"):
                environment.bootstrap(venv)
        run.assert_not_called()
        self.assertFalse(venv.exists())

    def test_worker_under_another_minor_refuses_before_reading_a_distribution(self):
        with mock.patch.object(sys, "version_info", (3, 13, 1, "final", 0)), \
                mock.patch.object(environment, "metadata") as read, \
                mock.patch.object(environment, "imported_from_pin") as origin:
            with self.assertRaisesRegex(SystemExit, "Python 3.13, where 3.14 is pinned"):
                environment.measured_pins()
        read.version.assert_not_called()
        origin.assert_not_called()

    def test_patch_level_is_not_pinned(self):
        with mock.patch.object(sys, "version_info", (3, 14, 8, "final", 0)), \
                mock.patch.object(environment, "metadata", fake_metadata(pinned())), \
                mock.patch.object(environment, "imported_from_pin"):
            self.assertEqual(environment.measured_pins(), dict(pinned(), python="3.14"))


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
