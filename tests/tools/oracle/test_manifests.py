import tempfile
import unittest
from pathlib import Path

from recorder.manifests import archive_revision, package_digest, package_version

LISTFILE = ("meios_corpus_package(NAME ur_description\n"
            "  URL https://example.invalid/ur.tar.gz\n  HASH SHA256=0123abcd)\n")


class package_version_refusal(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.root = Path(self.scratch.name)
        self.share = lambda name: self.root / name

    def tearDown(self):
        self.scratch.cleanup()

    def write_manifest(self, text):
        (self.root / "pkg").mkdir()
        (self.root / "pkg" / "package.xml").write_text(text, encoding="utf-8")

    def test_missing_manifest_refuses(self):
        with self.assertRaisesRegex(SystemExit, "no version in the manifest"):
            package_version(self.share, "pkg", "1.0.0")

    def test_manifest_without_version_refuses(self):
        self.write_manifest("<package><name>pkg</name></package>")
        with self.assertRaisesRegex(SystemExit, "no version in the manifest"):
            package_version(self.share, "pkg", "1.0.0")

    def test_empty_version_refuses(self):
        self.write_manifest("<package><version>  </version></package>")
        with self.assertRaisesRegex(SystemExit, "no version in the manifest"):
            package_version(self.share, "pkg", "1.0.0")

    def test_mismatched_version_refuses_naming_both(self):
        self.write_manifest("<package><version>1.0.1</version></package>")
        with self.assertRaisesRegex(SystemExit, "pkg declares 1.0.1 where 1.0.0 is pinned"):
            package_version(self.share, "pkg", "1.0.0")

    def test_pinned_version_is_returned(self):
        self.write_manifest("<package><version> 1.0.0 </version></package>")
        self.assertEqual(package_version(self.share, "pkg", "1.0.0"), "1.0.0")


class listfile_refusal(unittest.TestCase):
    def test_digest_of_a_listed_package_is_returned(self):
        self.assertEqual(package_digest(LISTFILE, "ur_description"), "0123abcd")

    def test_digest_of_an_unlisted_package_refuses(self):
        with self.assertRaisesRegex(SystemExit, "no archive digest for kortex_description"):
            package_digest(LISTFILE, "kortex_description")

    def test_digest_without_a_hash_refuses(self):
        with self.assertRaises(SystemExit):
            package_digest("meios_corpus_package(NAME ur_description URL x)", "ur_description")

    def test_revision_without_an_archive_commit_refuses(self):
        with self.assertRaisesRegex(SystemExit, "neither by a version nor by an archive commit"):
            archive_revision(LISTFILE, "ur_description")


if __name__ == "__main__":
    unittest.main()
