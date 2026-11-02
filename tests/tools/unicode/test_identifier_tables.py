import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import unicode_tables
import identifier_tables


class interpreter_pin(unittest.TestCase):
    def refused(self, version, unicode):
        with tempfile.TemporaryDirectory() as scratch, \
                mock.patch.object(sys, "version_info", version), \
                mock.patch.object(identifier_tables.unicodedata, "unidata_version", unicode):
            out = Path(scratch) / "tables"
            with self.assertRaises(SystemExit) as refusal:
                identifier_tables.main(["--out", str(out)])
            self.assertFalse(out.exists())
        return str(refusal.exception)

    def test_another_minor_refuses_and_writes_nothing(self):
        message = self.refused((3, 12, 3, "final", 0), "15.0.0")
        self.assertIn("Python 3.12 with Unicode 15.0.0, where 3.14 with Unicode 16.0.0 is pinned", message)

    def test_another_unicode_version_refuses_and_writes_nothing(self):
        message = self.refused((3, 14, 0, "final", 0), "16.1.0")
        self.assertIn("nothing was written", message)


class rendering(unittest.TestCase):
    def test_every_cell_is_padded_to_the_widest_of_its_table(self):
        cells = ["0x{:X}".format(point) for point in (0x1, 0x10, 0x100, 0x1000)] * 20
        padded = len("0x1000,") + 1
        for line in unicode_tables.rows(cells):
            body = line[unicode_tables.INDENT:]
            self.assertTrue(all(match.start() % padded == 0 for match in re.finditer("0x", body)), line)

    def test_the_last_cell_ends_the_list_without_a_comma(self):
        self.assertEqual(unicode_tables.table("char32_t", "pool", ["0x1", "0x2"])[-2], " " * unicode_tables.INDENT + "0x1, 0x2};")

    def test_a_long_list_keeps_the_fewest_columns_that_need_the_fewest_lines(self):
        self.assertEqual(unicode_tables.columns(30, 9), 15)
        self.assertEqual(unicode_tables.columns(100, 9), 17)


class range_compression(unittest.TestCase):
    def test_consecutive_code_points_merge_into_one_range(self):
        self.assertEqual(unicode_tables.ranges(lambda point: point in (3, 4, 5), range(10)), [(3, 5)])

    def test_a_gap_splits_a_range(self):
        self.assertEqual(unicode_tables.ranges(lambda point: point in (1, 2, 4), range(10)), [(1, 2), (4, 4)])

    def test_a_run_splits_where_its_value_changes(self):
        values = {1: 220, 2: 220, 3: 230}
        self.assertEqual(unicode_tables.runs(lambda point: values.get(point, 0), range(10)), [(1, 2, 220), (3, 3, 230)])


if __name__ == "__main__":
    unittest.main()
