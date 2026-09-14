"""
Raw ctypes struct/function declarations mirroring the native USDK C ABI
(include/usdk/{types,candidate,vote,plugin}.h). This module is
deliberately the ONLY place struct layouts and argtypes/restypes are
declared - see docs/ABI.md "Validate declared ABI compatibility before
invoking role operations": a concrete cross-file struct-field mismatch
bug found in the supplied research (docs/RESEARCH_REVIEW.md section
1.3.10) is exactly the failure mode a single source of truth like this
file exists to avoid - two files independently declaring "the same"
struct is how that bug happened there.

This release binds usdk-perceive only (create/destroy/observe/vote) -
see README.md for why usdk-deliberate/usdk-verify/usdk-core are not
bound here yet; the same pattern extends to them directly.
"""

import ctypes as C

USDK_ID_LEN = 64
USDK_DIGEST_LEN = 32

# usdk_status_t (include/usdk/status.h) - order matters, mirrors the C enum exactly.
STATUS_NAMES = [
    "OK", "INVALID_ARGUMENT", "ABI_VERSION_MISMATCH", "STRUCT_SIZE_MISMATCH",
    "CAPABILITY_NOT_FOUND", "MODULE_LOAD_FAILED", "MODULE_ENTRY_NOT_FOUND",
    "DEPENDENCY_CYCLE", "DEPENDENCY_UNRESOLVED", "DEPENDENCY_VERSION_INCOMPATIBLE",
    "STALE_ROUND", "DUPLICATE_VOTE", "PARTY_CONFLICT", "CANDIDATE_MUTATED",
    "DEADLINE_EXCEEDED", "ROUND_NOT_OPEN", "OUT_OF_MEMORY", "IO", "NOT_IMPLEMENTED",
]


def status_name(code: int) -> str:
    if 0 <= code < len(STATUS_NAMES):
        return STATUS_NAMES[code]
    return f"UNKNOWN({code})"


class UsdkError(RuntimeError):
    def __init__(self, status_code: int, context: str = ""):
        self.status_code = status_code
        self.status_name = status_name(status_code)
        super().__init__(f"{context}: {self.status_name} ({status_code})" if context else self.status_name)


ROLE_PERCEIVE = 1
VERDICT_ACCEPT, VERDICT_REJECT, VERDICT_ABSTAIN = 0, 1, 2


class Buffer(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_uint8)), ("len", C.c_uint32)]


class Config(C.Structure):
    _fields_ = [("data", Buffer)]


class EvidenceRef(C.Structure):
    _fields_ = [
        ("evidence_id", C.c_char * USDK_ID_LEN),
        ("evidence_digest", C.c_uint8 * USDK_DIGEST_LEN),
        ("uncertainty", C.c_double),
    ]


class Constraint(C.Structure):
    _fields_ = [("constraint_id", C.c_char * USDK_ID_LEN)]


class Candidate(C.Structure):
    # Field order and types must match include/usdk/candidate.h exactly -
    # see the module docstring.
    _fields_ = [
        ("struct_size", C.c_uint32),
        ("protocol_version", C.c_uint32),
        ("policy_version", C.c_uint32),
        ("session_id", C.c_char * USDK_ID_LEN),
        ("round_id", C.c_uint64),
        ("candidate_id", C.c_char * USDK_ID_LEN),
        ("content_digest", C.c_uint8 * USDK_DIGEST_LEN),
        ("evidence_refs", C.POINTER(EvidenceRef)),
        ("evidence_ref_count", C.c_uint32),
        ("proposed_response", Buffer),
        ("constraints", C.POINTER(Constraint)),
        ("constraint_count", C.c_uint32),
        ("deadline_ns", C.c_int64),
    ]


class Vote(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32),
        ("party_id", C.c_char * USDK_ID_LEN),
        ("role", C.c_uint32),
        ("verdict", C.c_uint32),
        ("reason_code", C.c_char * USDK_ID_LEN),
        ("reason_detail", C.c_char * 256),
        ("candidate_digest_seen", C.c_uint8 * USDK_DIGEST_LEN),
        ("voted_at_ns", C.c_int64),
    ]


def load_contracts_library(path: str):
    """Loads usdk-contracts (libusdk_contracts.dll) and declares
    usdk_candidate_create - used so a Candidate's content_digest is
    computed by the SAME canonical wire encoding + usdk_sha256 the
    native side uses (src/contracts/candidate.c ->
    usdk_wire_encode_candidate + usdk_sha256), instead of a Python
    reimplementation of that encoding drifting out of sync with it.
    See docs/ABI.md "Serialized messages vs. in-process structures" -
    the canonical encoding is exactly the kind of detail that must have
    one source of truth, not two independent implementations."""
    lib = C.CDLL(path)

    lib.usdk_candidate_create.argtypes = [
        C.c_uint32,           # policy_version
        C.c_char_p,           # session_id
        C.c_uint64,           # round_id
        C.c_char_p,           # candidate_id
        C.POINTER(EvidenceRef), C.c_uint32,  # evidence_refs, evidence_ref_count
        Buffer,                # proposed_response
        C.POINTER(Constraint), C.c_uint32,   # constraints, constraint_count
        C.c_int64,             # deadline_ns
        C.POINTER(Candidate),  # out
    ]
    lib.usdk_candidate_create.restype = C.c_int

    return lib


def load_perceive_library(path: str):
    """Loads usdk_perceive's native shared library via ctypes.CDLL (cdecl
    calling convention - matches USDK_CALL/__cdecl in include/usdk/platform.h)
    and declares argtypes/restype for every bound function, so ctypes
    itself catches an arity/type mistake here rather than silently
    misreading memory - the closest a ctypes binding can come to the
    native ABI's own struct-size/version negotiation
    (docs/ABI.md "ABI version and structure-size negotiation").
    """
    lib = C.CDLL(path)

    lib.usdk_perceive_create.argtypes = [C.POINTER(Config), C.POINTER(C.c_void_p)]
    lib.usdk_perceive_create.restype = C.c_int

    lib.usdk_perceive_destroy.argtypes = [C.c_void_p]
    lib.usdk_perceive_destroy.restype = None

    lib.usdk_perceive_observe.argtypes = [C.c_void_p, Buffer, C.c_double, C.POINTER(EvidenceRef)]
    lib.usdk_perceive_observe.restype = C.c_int

    lib.usdk_perceive_vote.argtypes = [C.c_void_p, C.POINTER(Candidate), C.POINTER(Vote)]
    lib.usdk_perceive_vote.restype = C.c_int

    return lib
