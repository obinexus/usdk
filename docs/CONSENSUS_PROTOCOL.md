# The trilateral agreement protocol

This document specifies exactly what `usdk-perceive`, `usdk-deliberate`,
and `usdk-verify` agree on before USDK dispatches any external effect,
and precisely how that agreement is checked. It is new engineering for
this project - see `docs/RESEARCH_REVIEW.md` section 4.1 and
`docs/ARCHITECTURE.md` "Comparison with the archive research" for why no
existing protocol from the supplied research was adapted instead.

**Execution model, stated once here because it changes the meaning of
several requirements below**: all three parties are shared libraries
loaded into one process (`docs/ARCHITECTURE.md`). A "round" is a
sequence of in-process, synchronous function calls, not network messages.
Where a requirement below (stale rounds, replay, crash/restart) is
usually associated with a distributed system, it is restated in terms of
what can actually happen in this execution model, not assumed by
analogy.

## The candidate

Defined in `include/usdk/candidate.h` as `usdk_candidate_t`. Immutable
once created - every field is set at construction
(`usdk_candidate_create`) and the type provides no setter. Fields, all
required by the task brief:

| Field | Type | Meaning |
|---|---|---|
| `protocol_version` | `uint32_t` | This document's protocol version. A party built against an incompatible major version refuses to vote (`USDK_VERDICT_ABSTAIN`, reason `protocol-version-mismatch`) rather than guess. |
| `policy_version` | `uint32_t` | Caller-supplied identifier for the constraint/threshold policy in force for this round. Not interpreted by `usdk-core`; parties compare it against their own loaded policy and abstain on mismatch. |
| `session_id` | `char[USDK_ID_LEN]` | Identifies the logical conversation/task this round belongs to. Caller-supplied, opaque to the protocol. |
| `round_id` | `uint64_t` | Strictly increasing within one session (enforced by `usdk-core`, see "Stale and replayed rounds"). |
| `candidate_id` | `char[USDK_ID_LEN]` | Unique identifier for this specific candidate's content. A revision gets a new `candidate_id` and a new `round_id` - never reuses either (see "Revisions"). |
| `content_digest` | `uint8_t[32]` | SHA-256 over the canonical byte encoding (`include/usdk/wire.h`) of every other field below this row. Tamper-evidence only - see "Digest is not authentication." |
| `evidence_refs` / `evidence_ref_count` | `const usdk_evidence_ref_t*` / `uint32_t` | Fixed-size records (`evidence_id`, a 32-byte `evidence_digest`, an `uncertainty` in `[0,1]`) - references to evidence `usdk-perceive` produced, never raw pointers into perceive's internal state and never the evidence payload itself (see `docs/ABI.md` "Serialized messages vs. in-process structures"). |
| `proposed_response` | `usdk_buffer_t` (pointer + length) | The candidate action/response payload, opaque to `usdk-core`. |
| `constraints` / `constraint_count` | `const usdk_constraint_t*` / `uint32_t` | Fixed-size string tags (e.g. `"workspace:temp-only"`, `"action:read-only"`) the proposed action must satisfy. |
| `deadline_ns` | `int64_t` | An absolute value on the same monotonic clock `usdk-core` uses to open the round (`usdk_monotonic_ns()`), not wall-clock. A round whose deadline has passed when `usdk_round_decide` is called ends `USDK_ROUND_TIMED_OUT` regardless of votes already collected. |

### Digest is not authentication

The content digest proves a candidate a party is looking at is
byte-for-byte the one that was created - it says nothing about who
created it, and it is not a substitute for checking party identity. This
distinction is stated explicitly because the supplied research got it
wrong in three independent places: `docs/RESEARCH_REVIEW.md` sections
1.3.7, 2.3, and 3.4 document that every occurrence of "AuraSeal"
cryptographic validation found across all three archives is either
unimplemented or, where code exists, provably not cryptographic (a plain
hash-equality check, a bare `length >= 32` check, or a comment reading
"demo - replace with real crypto"). USDK does not repeat that mistake:
digest and identity are separate fields, checked separately, and neither
is described as more than it is.

## Party identity

Because all calls are in-process function-pointer calls obtained from
`usdk_plugin_query_v1` (`docs/ABI.md`), "identity" here means: the
specific loaded module instance whose function pointer was actually
invoked. `usdk-core` records, for each role slot, the `usdk_module_t*`
handle it loaded and calls `vote()` only through that handle - there is
no separate network message whose sender could be spoofed. Each vote
(`usdk_vote_t`) additionally carries a `party_id` string that the module
declares in its manifest/descriptor (`docs/ABI.md`); `usdk-core` checks
that this declared `party_id` is consistent for a given role across the
whole round (a module cannot claim to be a different party mid-round)
but does **not** cryptographically verify it - there is no unforgeable
signature involved, and none is claimed.

**This identity model is sufficient only because all three parties run
inside one trusted process that the caller controls the loading of.** It
is explicitly insufficient for a future out-of-process or networked
extension of this protocol, which would need real authentication (a
verified signature or an authenticated channel) instead of "which
function pointer did the call arrive through" - building that is
out of scope for this release and is not simulated with fake
cryptography, per the lesson in "Digest is not authentication" above.

## Verdict vocabulary and the commit rule

```c
typedef enum usdk_verdict {
    USDK_VERDICT_ACCEPT  = 0,
    USDK_VERDICT_REJECT  = 1,
    USDK_VERDICT_ABSTAIN = 2
} usdk_verdict_t;
```

**Commit rule (unanimous acceptance, the only rule this release
implements):**

- The round commits if and only if all three role slots hold a valid
  `ACCEPT` vote, cast on the exact `content_digest` of the round's one
  candidate, before the deadline.
- Any `REJECT` from any party prevents commit for that candidate. A
  rejecting party's `reason_code`/`reason_detail` are preserved in the
  round record so a caller can construct a revised candidate.
- A missing vote (a party slot never voted), a stale vote (see below), a
  malformed vote (fails `usdk_vote_t` validation - wrong `struct_size`,
  unrecognized `verdict`, digest field of the wrong length), or an
  `ABSTAIN` all prevent commit, with the same effect as a rejection on
  this round: no commit, and the reason is recorded.
- A deadline passing before all three votes are collected ends the round
  `USDK_ROUND_TIMED_OUT`, regardless of how many `ACCEPT`s were already
  recorded, and executes nothing.

## Duplicate and conflicting votes

`usdk_round_submit_vote()` is keyed by role, not by call order. A second
vote submitted for a role slot that already holds a vote in the current
round is a **duplicate**: rejected by `usdk-core` with
`USDK_ERR_DUPLICATE_VOTE`, and the original vote is kept unchanged - a
party cannot overwrite its own vote by calling again. A vote whose
declared `party_id` differs from the `party_id` already recorded for that
role slot in this round is a **conflict**: rejected with
`USDK_ERR_PARTY_CONFLICT`, and neither vote is accepted for that slot.
Both cases leave the round `OPEN` (or eventually `TIMED_OUT`) - neither
can ever cause a commit.

## Stale rounds and replay rejection

`round_id` must be strictly increasing within a `session_id`, enforced
by `usdk_core_open_round()`: opening a round with a `round_id` less than
or equal to the highest `round_id` already seen for that session returns
`USDK_ERR_STALE_ROUND` and opens nothing. A vote presented for a
`round_id` that is not the session's current open round (including a
replayed vote from a round that already committed, rejected, or timed
out) is rejected with `USDK_ERR_STALE_ROUND` at the vote-submission call,
never silently accepted into a different round.

## Candidate mutation detection

Every vote carries the digest of the candidate content the voting party
actually evaluated (`usdk_vote_t.candidate_digest_seen`). Before counting
a vote toward the commit rule, `usdk_core_round_decide()` recomputes the
round's candidate's own canonical digest and compares it against every
recorded vote's `candidate_digest_seen`. Any mismatch - meaning the
in-memory candidate was altered after a party voted, which should be
impossible given the type has no setters, but is checked anyway as a
defense against a caller bypassing the API via direct memory access -
fails that vote with `USDK_ERR_CANDIDATE_MUTATED` and prevents commit.
This is exercised directly by `tests/integration/test_consensus_mutation.c`
(see `docs/VALIDATION.md`).

## Bounded retries and cancellation

A round is single-attempt for its one candidate: there is no in-place
retry of a timed-out or rejected round. `usdk_core_cancel_round()` moves
an `OPEN` round to `CANCELLED` and releases it; nothing is dispatched. A
caller that wants to try again constructs a **new candidate** (new
`candidate_id`, new `round_id`) - this is "Revisions" below, and
`usdk-core` does not impose a retry limit itself. `usdk-cli demo` (and
the trilateral integration example) impose their own small bounded
retry count as a caller-side policy, to demonstrate the pattern without
baking a specific limit into the library.

## Revisions

A revision - any change to the proposed response, evidence set, or
constraints - is a new candidate: new `candidate_id`, new `round_id`,
fresh votes from all three parties. Nothing in `usdk-core` allows amending
a candidate that already has any recorded vote.

## Crash and restart behavior

Because this is a single process, "a party crashes" means the shared
library's function call itself crashes (e.g. a native fault) or hangs.
**A native ABI call crashing takes down the entire host process - there
is no fault isolation between the orchestrator and a loaded role module
in this design.** This is stated plainly here and again in
`docs/ABI.md` "Trust model": loading and calling a native library is not
a sandbox. A hang is bounded only by the round's `deadline_ns`; a
same-process synchronous call cannot be interrupted once entered, so
`usdk-core` cannot recover from a hung vote call within the current round
- the deadline only bounds how long `usdk_core_round_decide()` is willing
to wait *between* calls it controls (see `docs/IMPLEMENTATION_STATUS.md`
for how the reference demo structures calls to keep this bound
meaningful).

"The orchestrating process restarts" - e.g. the host application is
killed and relaunched between opening a round and it committing - is
handled by the append-only round journal described next: on restart, a
caller that reopens the same `runtime_dir` sees any round journal records
left in `OPEN` state with no matching `COMMITTED`/`REJECTED`/
`TIMED_OUT` terminal record, and `usdk-core` refuses to silently resume
or re-dispatch them - it surfaces them via `usdk inspect` /
`usdk_core_recover_incomplete_rounds()` for the caller to decide (open a
fresh candidate, or treat as abandoned).

## Commit records and side-effect dispatch

On the unanimous-accept path, `usdk-core` appends a commit record to the
runtime directory's journal *before* calling the tool-action dispatch
callback, and appends a second record (`DISPATCHED` or
`DISPATCH_UNKNOWN`) after the callback returns or is interrupted. This
two-phase append is what makes the "external action may have run but its
acknowledgement was lost" case detectable: if the process dies between
the two appends, restart finds a `COMMITTED` record with no matching
`DISPATCHED`/`DISPATCH_UNKNOWN` record, which is exactly the state
`usdk_core_recover_incomplete_rounds()` reports as "action outcome
unknown - do not assume it did not happen."

**USDK does not claim exactly-once external effects.** The mechanism it
actually provides is at-least-once dispatch plus an idempotency key
(`candidate_id`), which only produces an exactly-once *effect* if the
tool action itself is idempotent under that key. Recovery from a lost
acknowledgement is therefore the caller's/driver's responsibility: retry
the dispatch with the same `candidate_id` as idempotency key, and design
the tool action so a repeated dispatch with the same key converges to the
same end state rather than repeating the effect. The vertical slice's one
tool action (`docs/IMPLEMENTATION_STATUS.md` "Vertical slice") is written
this way - see "Idempotency requirements" below.

## Idempotency requirements for tool execution

Any tool-action dispatch function registered with `usdk-core`
(`usdk_core_set_dispatch_fn`) receives the committed candidate's
`candidate_id` as an explicit idempotency key and **must** ensure that
invoking it twice with the same key produces the same observable end
state as invoking it once (e.g. "write this exact file content," checked
and skipped if already present with matching content, not "append this
content" or any operation that compounds on repeat). This is a
requirement on driver/dispatch implementations documented here and in
`include/usdk/core.h`'s doc comment on `usdk_dispatch_fn_t`; `usdk-core`
does not and cannot enforce it mechanically for an arbitrary caller-
supplied function - it can only guarantee it calls that function with a
stable key and records whether the call returned.

## Tradeoffs and non-goals

- **Unanimous agreement can stop all progress when one party is
  unavailable, hung, or persistently abstains/rejects.** There is no
  quorum fallback in this release - a missing vote has exactly the same
  effect on commit as an explicit rejection. This is a deliberate
  simplicity choice for the first implementation, not an oversight.
- **This protocol does not provide Byzantine fault tolerance.** It
  assumes every loaded module is either honest or absent/crashed, not
  actively malicious-but-alive. A compromised or buggy module that
  returns `ACCEPT` for a genuinely bad candidate is not detected by this
  protocol - it is exactly why each party is required to have a distinct,
  real check (`docs/ARCHITECTURE.md`) rather than being a rubber stamp,
  but three distinct-but-all-wrong checks can still agree.
- **Unanimous agreement does not prove the underlying response is
  true, safe, or correct.** It proves that three specific, independently
  implemented checks did not find a reason to object, under one policy
  version, at one point in time. Nothing in this protocol evaluates
  ground truth.

## Where this is tested

`tests/integration/test_consensus_*.c` (see `docs/VALIDATION.md`) exercise:
unanimous accept and commit; a rejecting party preventing commit; an
abstaining party preventing commit; a missing/never-voting party and
deadline expiry; a duplicate vote from the same role; a conflicting
`party_id` claim for one role; a stale/replayed round id; and candidate
mutation detection.
