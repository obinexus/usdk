"""
Real integration test: loads the actual build/bin/usdk_perceive.dll and
build/bin/libusdk_contracts.dll native libraries and exercises them
through the ctypes binding in usdk/. This mirrors tests/unit/test_perceive.c
case-for-case so the Python binding is checked against the same behavior
the native test suite already establishes, not a reimplementation of it.

Run with: python -m pytest tests/ -v   (or) python -m unittest tests.test_perceive
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from usdk import Contracts, Perceive  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
BUILD_BIN = os.path.join(REPO_ROOT, "build", "bin")
PERCEIVE_DLL = os.path.join(BUILD_BIN, "usdk_perceive.dll")
CONTRACTS_DLL = os.path.join(BUILD_BIN, "libusdk_contracts.dll")


@unittest.skipUnless(
    os.path.isfile(PERCEIVE_DLL) and os.path.isfile(CONTRACTS_DLL),
    f"native build not found under {BUILD_BIN} - run `make` at the repo root first",
)
class TestPerceiveBinding(unittest.TestCase):
    def setUp(self):
        self.contracts = Contracts(CONTRACTS_DLL)
        self.perceive = Perceive(PERCEIVE_DLL)

    def tearDown(self):
        self.perceive.close()

    def test_observe_returns_nonempty_evidence_id(self):
        ref = self.perceive.observe(b"reading: 42", uncertainty=0.3)
        self.assertTrue(len(ref.evidence_id) > 0)
        self.assertGreater(bytes(ref.evidence_digest), b"\x00" * 32)  # a real digest, not left zeroed

    def test_vote_accepts_candidate_citing_real_evidence(self):
        ref = self.perceive.observe(b"reading: 42", uncertainty=0.3)
        candidate = self.contracts.make_candidate(
            session_id="s", round_id=1, candidate_id="c1",
            proposed_response=b"r", evidence_refs=[ref],
        )
        vote = self.perceive.vote(candidate)
        self.assertEqual(vote.verdict, "accept")
        self.assertEqual(vote.role, 1)  # USDK_ROLE_PERCEIVE

    def test_vote_rejects_fabricated_evidence_not_abstains(self):
        from usdk._ffi import EvidenceRef

        self.perceive.observe(b"reading: 42", uncertainty=0.3)  # unrelated real observation
        fake = EvidenceRef(evidence_id=b"ev-fabricated")
        candidate = self.contracts.make_candidate(
            session_id="s", round_id=2, candidate_id="c2",
            proposed_response=b"r", evidence_refs=[fake],
        )
        vote = self.perceive.vote(candidate)
        self.assertEqual(vote.verdict, "reject")
        self.assertEqual(vote.reason_code, "evidence-not-found")

    def test_vote_rejects_zero_evidence_by_default(self):
        candidate = self.contracts.make_candidate(
            session_id="s", round_id=3, candidate_id="c3", proposed_response=b"r",
        )
        vote = self.perceive.vote(candidate)
        self.assertEqual(vote.verdict, "reject")
        self.assertEqual(vote.reason_code, "no-evidence-cited")

    def test_require_evidence_false_permits_zero_evidence_refs(self):
        with Perceive(PERCEIVE_DLL, config_json=b'{"require_evidence":false}') as lenient:
            candidate = self.contracts.make_candidate(
                session_id="s", round_id=1, candidate_id="c4", proposed_response=b"r",
            )
            vote = lenient.vote(candidate)
            self.assertEqual(vote.verdict, "accept")

    def test_invalid_uncertainty_rejected_by_python_layer(self):
        with self.assertRaises(ValueError):
            self.perceive.observe(b"x", uncertainty=1.5)

    def test_id_too_long_rejected_by_python_layer(self):
        with self.assertRaises(ValueError):
            self.contracts.make_candidate(
                session_id="s" * 64, round_id=1, candidate_id="c", proposed_response=b"r",
            )

    def test_context_manager_closes_handle(self):
        with Perceive(PERCEIVE_DLL) as p:
            p.observe(b"x", uncertainty=0.1)
        self.assertIsNone(p._handle)


if __name__ == "__main__":
    unittest.main()
