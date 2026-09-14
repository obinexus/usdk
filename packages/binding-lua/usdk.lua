--[[
usdk.lua - LuaJIT FFI bindings for the native USDK C ABI.

**UNTESTED.** No lua/luajit runtime exists anywhere in this development
environment (checked: `which lua luajit lua5.1 lua5.3 lua5.4` all found
nothing, and no package manager listed one either - see
packages/binding-lua/README.md for the exact commands run). This module
is written to the same struct-layout/ABI knowledge verified against the
real headers and DLL while building packages/binding-python (see
usdk/_ffi.py there, and its 8 passing tests against the actual
build/bin/usdk_perceive.dll and libusdk_contracts.dll) - the layouts
below are believed correct for the same reason the Python ones were
confirmed correct, but they have not themselves been executed. Do not
treat this file as validated; treat packages/binding-python as the
validated reference implementation of the same bindings.

Scope mirrors packages/binding-python: usdk-perceive only
(create/destroy/observe/vote) plus usdk_candidate_create from
usdk-contracts, so a Candidate's content_digest is computed by the real
native canonical encoding rather than a Lua reimplementation of it.
usdk-deliberate/usdk-verify/usdk-core are not bound - see
packages/binding-python/README.md "Scope of this release", which
applies identically here.

Usage (once a luajit runtime is available to actually run this):

    local usdk = require("usdk")
    local perceive = usdk.Perceive.new("build/bin/usdk_perceive.dll")
    local contracts = usdk.Contracts.new("build/bin/libusdk_contracts.dll")
    local evidence = perceive:observe("sensor reading: 22.4C", 0.05)
    local candidate = contracts:make_candidate{
        session_id = "s1", round_id = 1, candidate_id = "c1",
        proposed_response = "It's 22.4C.",
        evidence_refs = { evidence },
        constraint_ids = { "workspace:temp-only" },
    }
    local vote = perceive:vote(candidate)
    print(vote.verdict, vote.reason_code)
    perceive:close()
]]

local ffi = require("ffi")

-- Struct/function declarations mirroring include/usdk/{types,candidate,
-- vote,plugin,perceive}.h field-for-field - see the same headers cited
-- in packages/binding-python/usdk/_ffi.py's module docstring. Kept as
-- ONE cdef block, the Lua analogue of that file being "the only place
-- struct layouts are declared."
ffi.cdef[[
typedef enum { USDK_ROLE_UNSPECIFIED = 0, USDK_ROLE_PERCEIVE = 1,
               USDK_ROLE_DELIBERATE = 2, USDK_ROLE_VERIFY = 3,
               USDK_ROLE_DRIVER = 4 } usdk_role_t;

typedef struct { const uint8_t* data; uint32_t len; } usdk_buffer_t;
typedef struct { usdk_buffer_t data; } usdk_config_t;

typedef struct {
    char evidence_id[64];
    uint8_t evidence_digest[32];
    double uncertainty;
} usdk_evidence_ref_t;

typedef struct { char constraint_id[64]; } usdk_constraint_t;

typedef struct {
    uint32_t struct_size;
    uint32_t protocol_version;
    uint32_t policy_version;
    char session_id[64];
    uint64_t round_id;
    char candidate_id[64];
    uint8_t content_digest[32];
    const usdk_evidence_ref_t* evidence_refs;
    uint32_t evidence_ref_count;
    usdk_buffer_t proposed_response;
    const usdk_constraint_t* constraints;
    uint32_t constraint_count;
    int64_t deadline_ns;
} usdk_candidate_t;

typedef struct {
    uint32_t struct_size;
    char party_id[64];
    uint32_t role;
    uint32_t verdict;
    char reason_code[64];
    char reason_detail[256];
    uint8_t candidate_digest_seen[32];
    int64_t voted_at_ns;
} usdk_vote_t;

typedef struct usdk_role_instance usdk_role_instance_t;

int usdk_perceive_create(const usdk_config_t* cfg, usdk_role_instance_t** out);
void usdk_perceive_destroy(usdk_role_instance_t* inst);
int usdk_perceive_observe(usdk_role_instance_t* inst, usdk_buffer_t raw_input,
                           double uncertainty, usdk_evidence_ref_t* out_ref);
int usdk_perceive_vote(usdk_role_instance_t* inst, const usdk_candidate_t* candidate,
                        usdk_vote_t* out_vote);

int usdk_candidate_create(
    uint32_t policy_version, const char* session_id, uint64_t round_id,
    const char* candidate_id,
    const usdk_evidence_ref_t* evidence_refs, uint32_t evidence_ref_count,
    usdk_buffer_t proposed_response,
    const usdk_constraint_t* constraints, uint32_t constraint_count,
    int64_t deadline_ns, usdk_candidate_t* out);
int64_t usdk_monotonic_ns(void);
]]

local STATUS_NAMES = {
    [0] = "OK", "INVALID_ARGUMENT", "ABI_VERSION_MISMATCH", "STRUCT_SIZE_MISMATCH",
    "CAPABILITY_NOT_FOUND", "MODULE_LOAD_FAILED", "MODULE_ENTRY_NOT_FOUND",
    "DEPENDENCY_CYCLE", "DEPENDENCY_UNRESOLVED", "DEPENDENCY_VERSION_INCOMPATIBLE",
    "STALE_ROUND", "DUPLICATE_VOTE", "PARTY_CONFLICT", "CANDIDATE_MUTATED",
    "DEADLINE_EXCEEDED", "ROUND_NOT_OPEN", "OUT_OF_MEMORY", "IO", "NOT_IMPLEMENTED",
}
-- STATUS_NAMES[0] set above; indices 1.. filled positionally by the list
-- part of the constructor, matching usdk_status_t's C enum order exactly
-- (see include/usdk/status.h) - index N here means C value N.

local VERDICT_NAMES = { [0] = "accept", [1] = "reject", [2] = "abstain" }

local function status_name(code)
    return STATUS_NAMES[code] or ("UNKNOWN(" .. tostring(code) .. ")")
end

local function checked(status, context)
    if status ~= 0 then
        error(string.format("%s: %s (%d)", context, status_name(status), status))
    end
end

local M = { status_name = status_name }

-- usdk.Perceive ------------------------------------------------------

local Perceive = {}
Perceive.__index = Perceive

function Perceive.new(dll_path, config_json)
    local lib = ffi.load(dll_path)
    local self = setmetatable({ _lib = lib, _handle = ffi.new("usdk_role_instance_t*[1]") }, Perceive)

    local cfg = ffi.new("usdk_config_t")
    if config_json then
        -- Kept alive on self so it survives past this call, matching the
        -- lifetime note in packages/binding-python's Perceive.__init__.
        self._cfg_buf = ffi.new("uint8_t[?]", #config_json, config_json)
        cfg.data.data = self._cfg_buf
        cfg.data.len = #config_json
    end
    checked(lib.usdk_perceive_create(cfg, self._handle), "usdk_perceive_create")
    return self
end

function Perceive:close()
    if self._handle[0] ~= nil then
        self._lib.usdk_perceive_destroy(self._handle[0])
        self._handle[0] = nil
    end
end

function Perceive:observe(data, uncertainty)
    assert(uncertainty >= 0.0 and uncertainty <= 1.0, "uncertainty must be in [0, 1]")
    local buf = ffi.new("uint8_t[?]", #data, data)
    local raw = ffi.new("usdk_buffer_t", { buf, #data })
    local out_ref = ffi.new("usdk_evidence_ref_t")
    checked(self._lib.usdk_perceive_observe(self._handle[0], raw, uncertainty, out_ref), "usdk_perceive_observe")
    return out_ref
end

-- Accepts either a raw usdk_candidate_t cdata or the wrapper table
-- Contracts:make_candidate returns (whose backing buffers must stay
-- alive for this call - see make_candidate's closing comment).
function Perceive:vote(candidate)
    local c = (type(candidate) == "table") and candidate.struct or candidate
    local out_vote = ffi.new("usdk_vote_t")
    checked(self._lib.usdk_perceive_vote(self._handle[0], c, out_vote), "usdk_perceive_vote")
    return {
        party_id = ffi.string(out_vote.party_id),
        role = tonumber(out_vote.role),
        verdict = VERDICT_NAMES[tonumber(out_vote.verdict)] or "unknown",
        reason_code = ffi.string(out_vote.reason_code),
        reason_detail = ffi.string(out_vote.reason_detail),
        voted_at_ns = out_vote.voted_at_ns,
    }
end

M.Perceive = Perceive

-- usdk.Contracts -------------------------------------------------------

local Contracts = {}
Contracts.__index = Contracts

function Contracts.new(dll_path)
    return setmetatable({ _lib = ffi.load(dll_path) }, Contracts)
end

-- opts: { session_id, round_id, candidate_id, proposed_response,
--         evidence_refs = {...}, constraint_ids = {...}, deadline_ns }
function Contracts:make_candidate(opts)
    local resp = opts.proposed_response
    local resp_buf = ffi.new("uint8_t[?]", math.max(#resp, 1), resp)
    local proposed = ffi.new("usdk_buffer_t", { resp_buf, #resp })

    local evidence_refs = opts.evidence_refs or {}
    local evidence_array = nil
    if #evidence_refs > 0 then
        evidence_array = ffi.new("usdk_evidence_ref_t[?]", #evidence_refs)
        for i, ref in ipairs(evidence_refs) do
            evidence_array[i - 1] = ref
        end
    end

    local constraint_ids = opts.constraint_ids or {}
    local constraint_array = nil
    if #constraint_ids > 0 then
        constraint_array = ffi.new("usdk_constraint_t[?]", #constraint_ids)
        for i, id in ipairs(constraint_ids) do
            constraint_array[i - 1].constraint_id = id
        end
    end

    local deadline_ns = opts.deadline_ns
    if not deadline_ns then
        deadline_ns = self._lib.usdk_monotonic_ns() + 5000000000LL
    end

    local candidate = ffi.new("usdk_candidate_t")
    checked(self._lib.usdk_candidate_create(
        1, opts.session_id, opts.round_id, opts.candidate_id,
        evidence_array, #evidence_refs,
        proposed,
        constraint_array, #constraint_ids,
        deadline_ns, candidate
    ), "usdk_candidate_create")

    -- Anchor backing buffers on the returned table so LuaJIT's GC does not
    -- collect them while `candidate`'s borrowed pointers still reference
    -- them - the Lua analogue of packages/binding-python's
    -- candidate._resp_buf / ._evidence_array / ._constraint_array.
    return { struct = candidate, _resp_buf = resp_buf, _evidence_array = evidence_array, _constraint_array = constraint_array }
end

M.Contracts = Contracts

return M
