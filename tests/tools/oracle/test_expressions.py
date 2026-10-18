import tempfile
import unittest
from pathlib import Path
from unittest import mock

from recorder import expressions

CLOSURE = ("urdf/ur.urdf.xacro", "urdf/ur_macro.xacro", "urdf/inc/ur_common.xacro")


def stub_probe(refused):
    def probe(tmp, seeds, case_id, expression):
        if refused(case_id):
            return (case_id, expression, "REFUSED", "stubbed")
        return (case_id, expression, "1", "")
    return probe


class expression_floor_refusal(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.share = Path(self.scratch.name) / "share"
        self.tmp = Path(self.scratch.name) / "tmp"
        self.tmp.mkdir()
        for at, name in enumerate(CLOSURE):
            (self.share / name).parent.mkdir(parents=True, exist_ok=True)
            (self.share / name).write_text("${a%d + 1} ${b%d * 2}" % (at, at), encoding="utf-8")

    def tearDown(self):
        self.scratch.cleanup()

    def rows(self, refused):
        with mock.patch.object(expressions, "probe_row", stub_probe(refused)):
            return expressions.expression_rows(self.tmp, self.share)

    def test_closure_refusals_past_the_floor_refuse(self):
        floor = expressions.REFUSAL_FLOOR
        allowed = {"e{:02d}".format(at + 1) for at in range(floor + 1)}
        with self.assertRaisesRegex(SystemExit, "{} expressions refused".format(floor + 1)):
            self.rows(lambda case_id: case_id in allowed)

    def test_closure_refusals_at_the_floor_are_a_measurement(self):
        allowed = {"e{:02d}".format(at + 1) for at in range(expressions.REFUSAL_FLOOR)}
        rows = self.rows(lambda case_id: case_id in allowed)
        self.assertEqual(sum(1 for one in rows if one[2] == "REFUSED"), len(allowed))

    def test_every_authored_probe_refused_refuses(self):
        with self.assertRaisesRegex(SystemExit, "every authored subscript probe refused"):
            self.rows(lambda case_id: not case_id.startswith("e"))

    def test_rows_are_the_closure_then_the_authored_cases(self):
        rows = self.rows(lambda case_id: False)
        closure = [one for one in rows if one[0].startswith("e")]
        self.assertEqual(len(closure), 2 * len(CLOSURE))
        self.assertEqual(len(rows), len(closure) + len(expressions.authored_cases()))


if __name__ == "__main__":
    unittest.main()
