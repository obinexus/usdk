"""
usdk - Python bindings for the native USDK C ABI.

This release binds the usdk-perceive role only: create/destroy a
perceive instance, observe evidence, and cast a vote on a candidate.
usdk-deliberate and usdk-verify are NOT bound yet - the native
libraries exist (build/bin/usdk_deliberate.dll, usdk_verify.dll) and
the same struct/ctypes pattern in _ffi.py applies directly, but only
perceive has been implemented and tested in this release. See
docs/UAGENT_ARCHITECTURE.md "Package structure" for where this
package sits among the JS bindings, and README.md in this directory
for exact usage and the reason the other two roles are deferred.

Only stdlib (ctypes) is used - no third-party dependencies.
"""

import os

from ._ffi import (
    Buffer,
    Candidate,
    Config,
    Constraint,
    EvidenceRef,
    ROLE_PERCEIVE,
    UsdkError,
    VERDICT_ACCEPT,
    VERDICT_ABSTAIN,
    VERDICT_REJECT,
    Vote,
    load_contracts_library,
    load_perceive_library,
    status_name,
)
import ctypes as C

__all__ = [
    "Perceive",
    "Contracts",
    "UsdkError",
    "status_name",
    "VERDICT_ACCEPT",
    "VERDICT_REJECT",
    "VERDICT_ABSTAIN",
]

_VERDICT_NAMES = {VERDICT_ACCEPT: "accept", VERDICT_REJECT: "reject", VERDICT_ABSTAIN: "abstain"}


def _encode_id(value: str) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) >= 64:
        raise ValueError(f"id {value!r} does not fit in the ABI's 64-byte id field")
    return encoded


class Perceive:
    """A single usdk-perceive native instance, bound via ctypes.

    Usage:
        with Perceive("build/bin/usdk_perceive.dll") as p:
            evidence = p.observe(b"sensor reading: 22.4C", uncertainty=0.05)
            vote = p.vote(candidate)
    """

    def __init__(self, dll_path: str, config_json: bytes = None):
        """`config_json`, if given, is passed through verbatim as the
        role's opaque config payload (include/usdk/perceive.h documents
        its one field: {"require_evidence": bool}, default true)."""
        if not os.path.isfile(dll_path):
            raise FileNotFoundError(f"usdk_perceive native library not found: {dll_path}")
        self._lib = load_perceive_library(dll_path)
        self._handle = C.c_void_p()

        if config_json:
            cfg_type = C.c_uint8 * len(config_json)
            self._cfg_buf = cfg_type.from_buffer_copy(config_json)  # kept alive only for this call
            data = Buffer(data=C.cast(self._cfg_buf, C.POINTER(C.c_uint8)), len=len(config_json))
        else:
            data = Buffer(data=None, len=0)
        config = Config(data=data)
        status = self._lib.usdk_perceive_create(C.byref(config), C.byref(self._handle))
        if status != 0:
            raise UsdkError(status, "usdk_perceive_create")

    def close(self):
        if self._handle:
            self._lib.usdk_perceive_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def __del__(self):
        # Best-effort - explicit close()/context-manager use is the
        # documented path (see README.md); __del__ only guards against a
        # forgotten close() leaking the native handle.
        try:
            self.close()
        except Exception:
            pass

    def observe(self, data: bytes, uncertainty: float) -> EvidenceRef:
        """Wraps usdk_perceive_observe: records `data` as evidence with a
        caller-supplied uncertainty in [0, 1], returning an EvidenceRef
        struct (by value) the caller can attach to a Candidate."""
        if not 0.0 <= uncertainty <= 1.0:
            raise ValueError(f"uncertainty must be in [0, 1], got {uncertainty}")
        buf_type = C.c_uint8 * len(data)
        c_data = buf_type.from_buffer_copy(data)
        buffer = Buffer(data=C.cast(c_data, C.POINTER(C.c_uint8)), len=len(data))
        evidence_out = EvidenceRef()
        status = self._lib.usdk_perceive_observe(self._handle, buffer, uncertainty, C.byref(evidence_out))
        if status != 0:
            raise UsdkError(status, "usdk_perceive_observe")
        # Keep c_data alive for the duration of the call above; the ABI
        # copies what it needs synchronously inside usdk_perceive_observe,
        # so it is safe to let c_data be collected once this returns.
        return evidence_out

    def vote(self, candidate: Candidate) -> "PerceiveVote":
        """Wraps usdk_perceive_vote: casts this role's vote on an immutable
        Candidate struct, returning the native Vote verdict/reason."""
        vote_out = Vote()
        status = self._lib.usdk_perceive_vote(self._handle, C.byref(candidate), C.byref(vote_out))
        if status != 0:
            raise UsdkError(status, "usdk_perceive_vote")
        return PerceiveVote(vote_out)


class PerceiveVote:
    """Read-only, Pythonic view over a native Vote struct."""

    def __init__(self, raw: Vote):
        self.party_id = raw.party_id.decode("utf-8", "replace")
        self.role = raw.role
        self.verdict = _VERDICT_NAMES.get(raw.verdict, f"unknown({raw.verdict})")
        self.reason_code = raw.reason_code.decode("utf-8", "replace")
        self.reason_detail = raw.reason_detail.decode("utf-8", "replace")
        self.voted_at_ns = raw.voted_at_ns

    def __repr__(self):
        return f"PerceiveVote(verdict={self.verdict!r}, reason_code={self.reason_code!r})"


class Contracts:
    """Loads libusdk_contracts.dll to build Candidate structs through the
    real usdk_candidate_create - see _ffi.load_contracts_library for why
    this goes through the native function rather than a Python
    reimplementation of the canonical digest encoding."""

    def __init__(self, dll_path: str):
        if not os.path.isfile(dll_path):
            raise FileNotFoundError(f"usdk-contracts native library not found: {dll_path}")
        self._lib = load_contracts_library(dll_path)

    def make_candidate(
        self,
        session_id: str,
        round_id: int,
        candidate_id: str,
        proposed_response: bytes,
        evidence_refs=(),
        constraint_ids=(),
        deadline_ns: int = None,
        deadline_from_now_ns: int = 5_000_000_000,
    ) -> Candidate:
        """Builds a native Candidate struct via usdk_candidate_create,
        which fills in content_digest itself (src/contracts/candidate.c).
        Keeps the backing buffers alive on the returned struct instance -
        ctypes does not keep POINTER() targets alive on its own, so a
        caller that dropped them before passing the struct to vote()
        would read freed memory."""
        resp_bytes = proposed_response
        resp_type = C.c_uint8 * max(len(resp_bytes), 1)
        resp_buf = resp_type.from_buffer_copy(resp_bytes.ljust(1, b"\0") if not resp_bytes else resp_bytes)
        proposed = Buffer(data=C.cast(resp_buf, C.POINTER(C.c_uint8)), len=len(resp_bytes))

        evidence_array = (EvidenceRef * len(evidence_refs))(*evidence_refs) if evidence_refs else None
        constraint_array = (
            (Constraint * len(constraint_ids))(*[Constraint(constraint_id=_encode_id(c)) for c in constraint_ids])
            if constraint_ids
            else None
        )

        candidate = Candidate()
        status = self._lib.usdk_candidate_create(
            1,  # policy_version
            _encode_id(session_id),
            round_id,
            _encode_id(candidate_id),
            evidence_array,
            len(evidence_refs),
            proposed,
            constraint_array,
            len(constraint_ids),
            # deadline_ns is on usdk_monotonic_ns()'s clock, NOT wall-clock
            # (include/usdk/candidate.h) - default to "monotonic-now plus a
            # few seconds" via the real usdk_monotonic_ns(), not
            # time.time_ns(), which is a different clock entirely.
            deadline_ns if deadline_ns is not None else self._lib.usdk_monotonic_ns() + deadline_from_now_ns,
            C.byref(candidate),
        )
        if status != 0:
            raise UsdkError(status, "usdk_candidate_create")

        # Anchor the backing buffers on the struct instance - see docstring.
        # usdk_candidate_create stores borrowed pointers into `candidate`
        # (evidence_refs/proposed_response.data/constraints), matching
        # include/usdk/candidate.h's documented borrowing contract.
        candidate._resp_buf = resp_buf
        candidate._evidence_array = evidence_array
        candidate._constraint_array = constraint_array
        return candidate
