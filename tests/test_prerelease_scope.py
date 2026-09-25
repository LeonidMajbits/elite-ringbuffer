"""Regression guard for the reviewed pre-release metadata corrections.

No runtime concurrency guarantee follows from these text/hash assertions.
"""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PreReleaseScopeTests(unittest.TestCase):
    def test_current_source_binding_matches(self):
        record = json.loads((ROOT / "formal/SOURCE_BINDING.json").read_text())
        for name, item in record["files"].items():
            self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), item["sha256"], name)

    def test_executed_tool_claim_not_invented(self):
        for name in ("README.md", "NOTICE.md"):
            text = (ROOT / name).read_text()
            self.assertNotIn("formal TLA+/Prover9 model checking under strict stage contracts", text)
            self.assertIn("No executed TLA+/Prover9 verification is supplied or claimed", text)
            self.assertIn("bounded", text)

    def test_raw_receipt_scope_not_universalized(self):
        for name in ("README.md", "NOTICE.md"):
            text = (ROOT / name).read_text()
            self.assertNotIn("verified with zero-tolerance mathematical oracles, and validated on bare-metal hardware", text)
            self.assertIn("LAB_REPORTED", text)
            self.assertIn("rounding/ULP", text)

    def test_human_copyright_and_algorithm_attribution_preserved(self):
        self.assertIn("Copyright (c) 2026 Leonid Majbits", (ROOT / "LICENSE").read_text())
        self.assertIn("Ruslan Nikolaev", (ROOT / "NOTICE.md").read_text())
        self.assertIn("DISC 2019", (ROOT / "NOTICE.md").read_text())

if __name__ == "__main__":
    unittest.main()
