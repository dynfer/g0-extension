import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

GENERATOR = Path(__file__).resolve().parents[1] / "tools/firmware_version.py"
spec = importlib.util.spec_from_file_location("g0_version", GENERATOR)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "main.cpp"
        self.source.write_text("int main() {}\n")
        self.header = self.root / "build/g0_firmware_version.h"

    def fingerprint(self, options="-Os", revision=b"abc", compiler=b"gcc 11.3"):
        with patch.object(generator.subprocess, "check_output", side_effect=[revision, compiler]):
            return generator.fingerprint(self.root, "gcc", options)

    def test_identity_tracks_build_inputs(self):
        first = self.fingerprint()
        self.assertEqual(first, self.fingerprint())
        self.assertNotEqual(first, self.fingerprint(options="-O2"))
        self.assertNotEqual(first, self.fingerprint(revision=b"def"))
        self.assertNotEqual(first, self.fingerprint(compiler=b"gcc 14.2"))
        self.source.write_text("int main() { return 1; }\n")
        self.assertNotEqual(first, self.fingerprint())
        changed = self.fingerprint()
        (self.root / "README.md").write_text("Documentation only\n")
        self.assertEqual(changed, self.fingerprint())

    def test_header_is_stable_and_updates_for_source_change(self):
        argv = [str(GENERATOR), "--root", str(self.root), "--output", str(self.header),
                "--compiler", "gcc"]
        with patch.object(sys, "argv", argv), patch.object(
                generator.subprocess, "check_output", return_value=b"fixed"):
            generator.main()
            self.assertRegex(self.header.read_text(), r"#define G0_FIRMWARE_VERSION 0x[0-9a-f]{8}U")
            original = self.header.read_text()
            mtime = self.header.stat().st_mtime_ns
            generator.main()
            self.assertEqual(mtime, self.header.stat().st_mtime_ns)
            self.source.write_text("int main() { return 1; }\n")
            generator.main()
            self.assertNotEqual(original, self.header.read_text())
            self.assertFalse(self.header.with_suffix(".tmp").exists())


if __name__ == "__main__":
    unittest.main()
