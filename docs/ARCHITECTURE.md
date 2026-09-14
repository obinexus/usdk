# Architecture

USDK is an in-process C SDK: a neutral orchestrator (`usdk-core`) loads
three independently-built role libraries at runtime through a shared
dynamic-loading layer (`usdk-ffi`), presents each of them the same
immutable candidate and evidence, collects a verdict from each, and only
dispatches a proposed action if all three unanimously accept it under the
protocol in `docs/CONSENSUS_PROTOCOL.md`.

This is a single-process design, not a distributed system: the three
role modules are shared libraries (`.dll`/`.so`/`.dylib`) loaded into one
process's address space, called synchronously through function pointers
obtained via `usdk_plugin_query_v1` (`docs/ABI.md`). There are no
brokers, no network protocol, and no separate party processes in this
implementation. This is a deliberate scope decision, not an oversight -
see "What this is not" below.

## The three constituent roles

| Package | Responsibility | Distinct check it performs on a candidate |
|---|---|---|
| `usdk-perceive` | Converts incoming information into structured observations and evidence-bearing context; represents uncertainty and missing evidence. | Does the candidate's `evidence_refs` actually trace back to evidence this perceive instance produced, and is the cited uncertainty within its own declared bounds? Rejects candidates that cite evidence it never produced, or whose uncertainty was too high to license the proposed action. |
| `usdk-deliberate` | Produces the candidate response/plan/action, using a pluggable backend driver (`usdk-driver-<backend>`) and the supplied evidence. | Self-consistency: is this exact candidate (by content digest) one this instance actually generated, within its own declared capability and constraint set - not a rubber-stamp of whatever it's handed. |
| `usdk-verify` | Independently checks the candidate against available evidence, declared constraints, and action permissions. | The real permission/sufficiency gate: evaluates the candidate's proposed action against the constraint list and evidence sufficiency threshold, independent of whichever party proposed it. |

These are engineering role names, not claims about consciousness,
cognition, or sentience - see `docs/RESEARCH_REVIEW.md` section 3.5 for
the specific reason this is stated explicitly (the research contains a
direct, unresolved self-contradiction on exactly this point, and USDK
does not repeat it).

Each constituent:
- has its own public API (`include/usdk/perceive.h`,
  `include/usdk/deliberate.h`, `include/usdk/verify.h`) and lifecycle
  (`usdk_<role>_create`/`_destroy`);
- builds and is tested as an independent CMake target
  (`tests/unit/test_perceive.c`, etc., each linking only its own role
  library plus `usdk-contracts`);
- is exercised in integration tests and examples against fixture
  implementations of the *other two* roles (`tests/fixtures/`,
  `examples/*_standalone/`), never against each other's real code;
- does not link `usdk-perceive`, `usdk-deliberate`, or `usdk-verify`
  against one another - confirmed by the CMake target graph
  (`docs/PACKAGES.md` "Dependency graph") having no edges between them;
- is reachable exclusively through the shared C ABI
  (`usdk_plugin_query_v1`) - the orchestrator in `usdk-core` never
  `#include`s a role's private headers, only `include/usdk/plugin.h`;
- documents its inputs, outputs, failure modes, and limitations in its
  own header's doc comments plus a short section in this file.

## Comparison with the archive research

The task requires comparing this decomposition with the actual research
before freezing it, and explaining any mismatch. See
`docs/RESEARCH_REVIEW.md` sections 1.6, 2.4, and 4.1 for the full
evidence; summarized here:

**Where the research supports this shape.** The highest-quality, most
tested code path found in any of the three archives -
`obiai-main/src/obiai/agents/engine.py`'s `UReasoningEngine.reason()` -
is Observation -> ontology mapping -> uncertainty-scored Bayesian update
-> DAG-traversal-driven candidate action -> bias audit -> safety audit ->
auditable Decision. That is the same three-stage shape (evidence-with-
uncertainty, candidate-with-reasoning, independent audit-before-acting),
arrived at independently, for a different product, by different code.
This is real, if indirect, support for the proposed decomposition - not
proof that it is the only correct one.

**Where the research does not supply an existing three-way *voting*
protocol.** Three different "rule of three" concepts exist across the
archives - the Eze/Uche/Obi persona framing, the Trident `CH_0`/`CH_1`/
`CH_2` channel model, and the DIRAM severity-escalation cascade
(`_ObinexusPersona`/`_UchePersona`/`_EzePersona`) - and none of them is a
simultaneous, unanimous vote by three independent evaluators over one
candidate. The DIRAM cascade in particular is checked directly in
`docs/RESEARCH_REVIEW.md` section 2.4 to be a single-active-authority
hand-off (only one persona's output is used at a time, escalating by
severity), not a vote. The closest *working, tested* multi-party
agreement code found anywhere in the research
(`obi-main/examples/epistemic-dag/obinexus_ai_full.py`'s `DBFTConsensus`,
section 3.2) is a weighted, threshold-quorum (default 2-of-3 by weight)
mechanism explicitly self-labeled by its own author as "a research
demonstration, not a production BFT protocol" - not a fixed, unanimous
3-of-3 vote either.

**Conclusion, stated explicitly per the task's instruction**: USDK's
three roles retain the task's proposed `usdk-<role>` naming and the
`perceive`/`deliberate`/`verify` responsibilities, because the research
supports that shape at the pipeline-stage level. The unanimous 3-of-3
consensus *protocol* itself (`docs/CONSENSUS_PROTOCOL.md`) is **new
engineering work for this project**, not an adaptation of anything found
in the research - the research's closest analogues are a hand-off chain
and a fake-signature-bearing threshold quorum, neither of which is what
the task specifies. Where the research is used to shape the protocol, it
is used negatively as much as positively: three independent, corroborated
findings that every "AuraSeal" cryptographic-validation occurrence in the
research is fake or unimplemented (`docs/RESEARCH_REVIEW.md` sections
1.3.7, 2.3, 3.4) directly justify why USDK's protocol treats a candidate
digest as tamper-evidence only, never as authentication.

## What this is not

- **Not a distributed consensus system.** All three parties run as
  shared libraries in one process. There is no network protocol, no
  partial-network-failure handling, and no Byzantine fault tolerance
  (see `docs/CONSENSUS_PROTOCOL.md` "Tradeoffs and non-goals"). Extending
  this to out-of-process or networked parties is future work requiring
  real authentication (not a digest) and is explicitly out of scope here.
- **Not a claim that any role "understands," "reasons," or is
  "conscious."** These are structured data transformations with declared
  failure modes.
- **Not a general plugin-signature/trust system.** The reference
  `usdk-ffi` validates declared ABI compatibility (version/struct-size
  negotiation) before invoking a module's operations; it does not
  validate that a module's *behavior* is safe or non-malicious. Loading a
  native library can execute its initialization code before any ABI
  check runs - see `docs/ABI.md` "Trust model."

## Dependency graph

See `docs/PACKAGES.md` for the full package list, responsibilities, and
the CMake target graph with allowed dependency directions.
