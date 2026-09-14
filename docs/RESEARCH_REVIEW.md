# Research review

This document reviews the three supplied reference archives before any
USDK design was frozen, per the task brief. It is a review, not a design
document: `docs/ARCHITECTURE.md`, `docs/PACKAGES.md`, and
`docs/CONSENSUS_PROTOCOL.md` state what USDK actually does and cite back
into this file wherever a decision follows from something reviewed here.

**Method.** Every claim below is classified as one of:

- **Definition** - a term or structure the research defines, not a claim
  that needs evidence.
- **Proposal / hypothesis** - stated but not shown to hold.
- **Empirical result** - a measured outcome, with the measurement located.
- **Mathematical claim with reviewed proof** - a derivation exists and
  was checked here for whether the conclusion follows from the stated
  assumptions.
- **Unsupported or unresolved claim** - asserted without a derivation,
  dataset, or reproducible test; or explicitly contradicted by other
  material in the same archive.

This classification scheme is itself adapted from a document found in
the research - `obiai-main/docs/traceability.md` - which applies
materially the same method (Formal definition / Implementation proposal
/ Hypothesis requiring validation / Unsupported claim requiring
validation / Deferred) to its own source paper. Finding that document
was reassuring: it means at least one part of the reference research
already holds itself to the standard this review applies to all three
archives.

**Extraction safety.** All three archives were listed (`unzip -l`) before
extraction and checked for absolute paths or `..` path-traversal entries;
none were found (each contains a single top-level `<repo>-main/`
directory, consistent with a GitHub codeball export). Extracted read-only
to `reference/<archive>-main/` (gitignored - not part of USDK's own
source, ~4,500 files total, kept locally for citation only). Original
`.zip` files in `Downloads/` were not modified.

---

## 1. `obiai-main` (2,141 files; task estimate ~2,436 - see note below)

### 1.1 Source inventory (highest-value paths; see reviewer's note on scale)

| Path (relative to `obiai-main/`) | What it is |
|---|---|
| `README.md`, `READMEFORU.md` | Product spec for "U", a browser video-call AI agent. Explicit "what is and is not claimed" section. |
| `docs/traceability.md` | Paper-claim -> code -> test mapping, with an explicit claim-classification scheme (see Method above). |
| `docs/uml/architecture.md`, `docs/uml/user-stories.md` | Mermaid diagrams and user stories, each citing a proof reference. |
| `src/obiai/` | The real, shipped Python package for U. Highest code/test quality in the archive. |
| `tests/` | 26 pytest files for `src/obiai`, including property-based tests (Hypothesis). |
| `ml/README.md` | A self-critical experiment log with an actual measured result (see 1.3.6). |
| `obi-sdk/` | A nested nearly-full copy of the nature of the standalone `obi-sdk-main` archive - see section 2 for the dedicated review; not re-derived here except where `obiai-main`-specific docs describe it. |
| `obi-sdk/docs/peer-review/` | ~130 PDFs/MD, the theoretical/patent corpus (Dimensional Game Theory, AEGIS-PROOF series, "obicall" runtime sketch, patent claims). |
| `docs/obi/docs/source/{archive,md/*}` | ~150 files, a 2-3x duplicated conversion of a 75-paper Overleaf corpus (quantum/category-theory papers included, mostly unrelated to USDK). Sampled, not read in full - see 1.5. |
| `fruit-ninja/`, `web/`, `config/`, `.gitlab-ci.yml` | Product/infra for "U"; not relevant to USDK's C ABI/consensus design and not reviewed further. |

*Reviewer's note on scale*: a full recursive file count returned 2,141
files against the task's ~2,436 estimate. The difference is attributed to
counting method (e.g. whether empty directories or the archive's own root
entry are counted), not a truncated extraction - directory listings at
every level referenced below were consistent and complete.

### 1.2 Terminology introduced by this archive

- **Eze / Uche / Obi** - three simultaneous "perspectives" described in
  `obi-sdk/README.md`/`USAGE.md`: Eze ("the Leader. Governance.
  Responsibility.", `ctx.infer`), Uche ("the Knowledge. Wisdom.
  Understanding.", `ctx.reason`), Obi ("the Heart. Feeling. The human.",
  `ctx.probe_internal`). **Definition** (a persona/role naming, not a
  claim).
- **Trident Channel** - `CH_0` (Observe) / `CH_1` (Defer to human) /
  `CH_2` (Collapse to validated output), defined in
  `obi-sdk/obi/core/types.py` and used by `governance.py`/`probe.py`.
  **Definition**, and unlike Eze/Uche/Obi this one is actually wired to
  running code and tests (`obi-sdk/obi/core/tests/test_probe_internal.py`).
- **Ambiguity, flagged explicitly**: Eze/Uche/Obi and the Trident Channel
  are both described using "tripartite"/"trinity" language in various
  documents, but no document in either archive gives a formal mapping
  between the two. They read as two independent three-way proposals that
  happen to share a "rule of three" framing, not one validated concept
  with two names. USDK's three roles (perceive/deliberate/verify) are
  **not** a rename of either of these - see 1.6 and `docs/ARCHITECTURE.md`
  section "Comparison with the archive research" for the explicit
  comparison the task requires.
- **DGT (Dimensional Game Theory)** - a formalism with `StrategicDimension`,
  `ScalarInput`/`promote_scalar` ("Definition 1: Scalar Promotion"),
  `contextual_activation` ("Definition 2"), `CanonicalSV` ("Definition 3:
  Strategic Vector"), in `obi-sdk/obi/core/dgt_variadic{,_v1}/`.
  **Definition**, with a self-consistency check
  (`verify_definition_consistency()`) - see 1.3.3.
- **AEGIS-PROOF-1.1/1.2** - a traversal-cost formalism,
  `C = alpha*KL(P_i||P_j) + beta*deltaH`, `alpha+beta=1`, implemented in
  `src/obiai/epistemic/dag.py`. **Definition** for the cost formula;
  **unsupported claim** for the non-negativity "theorem" - see 1.3.4.
- **AuraSeal** / **Git-RAF** - a described "cryptographic perfect
  validation" gate (`obi-sdk/core/raf/auraseal.py`). See 1.3.7: the name
  claims cryptography the implementation does not provide.
- **obicall** (prose only, `obi-sdk/docs/peer-review/README.md` section
  10) - a described, never-implemented C "Polyglot System Call Runtime"
  with a `syscall_register(dispatcher, name, vtable, type)` service-locator
  pattern. **Naming collision, not a technical relationship**: the author
  of this review previously implemented an unrelated, already-shipped
  project also named `obicall` (a sensor-fusion runtime, different task,
  different repository, not derived from this reference material and not
  part of USDK). This document's "obicall" is a prose sketch with no
  corresponding code anywhere in either archive (confirmed: no
  `obicall.c`/`.h` exists in `obiai-main` or `obi-sdk-main`). USDK does
  not use the name `obicall` for anything, to avoid conflating the two.

### 1.3 Claims relevant to USDK

**1.3.1 - φ-marginalized Bayesian update (`src/obiai/bayesian/marginal.py`)**

- Claim: `P(theta|D) = sum_phi P(theta|D,phi) P(phi|D)` is computed as an
  exact discrete sum over nuisance parameter `phi`, not sampled.
- Assumptions: priors over `phi` are supplied and are checked (positive,
  sum to 1) in the constructor.
- Evidence: `tests/test_marginal.py` derives a posterior by hand
  (`943/1010 ~= 0.9337`) and checks the implementation matches to `1e-12`,
  and separately proves the discrete-mixture implementation is
  numerically equivalent to an explicit-latent-variable Bayesian network
  on the same inputs.
- **Classification: Mathematical claim with reviewed proof** (for the
  specific finite-mixture case tested - the equivalence check plus the
  hand-derived posterior together constitute a real, checkable proof
  for that construction, not merely a passing test of one input).
- Boundary check performed here: the class-level invariant "priors over
  `phi` are positive and sum to 1" is enforced by the constructor
  (raises rather than silently normalizing) - this matters because a
  silently-renormalized prior would make the "exact sum" claim misleading
  for malformed input; the implementation does not have that gap.

**1.3.2 - Uncertainty/truth-state gating (`config/obiai.yaml`,
`src/obiai/agents/engine.py`)**

- Definition: `yes_threshold=0.75`, `no_threshold=0.25`,
  `max_uncertainty_for_action=0.30` gate a YES/NO/MAYBE truth-state, and
  `test_properties.py` (Hypothesis property-based) fuzz-checks that a
  MAYBE state or a failed safety/bias audit never triggers a
  high-impact action.
- **Classification: Definition** (the thresholds) plus **Empirical
  result** for the invariant-holds-under-fuzzing claim, scoped to the
  input distribution Hypothesis actually generated - fuzz testing is
  strong evidence, not a general proof, for arbitrary future inputs.
- **Implementation requirement this responsibly becomes**: USDK's
  verify role (section 4 below) should have the same shape - a party
  that can independently produce ABSTAIN/REJECT from insufficient
  evidence, not only pass through whatever `deliberate` proposed - see
  `docs/ARCHITECTURE.md`.

**1.3.3 - DGT definitions (`obi-sdk/obi/core/dgt_variadic/`)**

- The three definitions (scalar promotion, contextual activation,
  canonical strategic vector) are internally consistent
  (`verify_definition_consistency()` runs and passes) and match their
  stated formulas.
- **Classification: Definition**, self-consistently implemented.
- Limitation, checked directly: every example and self-test uses 2-3
  toy dimensions with constant utility functions. No file in either
  archive uses DGT to arbitrate between real, independent multi-agent
  or multi-model outputs - so "DGT can resolve which of several
  candidate agent outputs to prefer" is **not demonstrated**, only the
  arithmetic of the formalism itself.
- **Not adopted for USDK's consensus protocol** (section 4): the
  trilateral agreement protocol specified by the task is a unanimous
  ACCEPT/REJECT/ABSTAIN vote, not a strategic-vector optimization: DGT
  was considered and set aside as unproven at the multi-party-arbitration
  scale USDK needs, not because the arithmetic is wrong.

**1.3.4 - AEGIS-PROOF-1.2 traversal cost (`src/obiai/epistemic/dag.py`)**

- Formula: `C(Node_i -> Node_j) = alpha*KL(P_i||P_j) + beta*deltaH`,
  `alpha+beta=1` enforced in the constructor.
- Claimed property (restated as "Theorem A.2.1" in
  `obi-sdk/docs/peer-review/OBI - Ontological Baysian Intelligence Full
  File 15 MAY 2026.md`, lines ~793-820): `C >= 0`, "cost increases
  monotonically with semantic divergence."
- Checked here: KL divergence is itself always `>= 0` (a standard,
  well-known property, not something this research needed to prove), but
  `deltaH` (an entropy *difference*) can be negative. The theorem
  statement in the document is not accompanied by a derivation showing
  the `alpha`/`beta` weighted sum stays non-negative in general. The
  actual implementation confirms this gap: `dag.py` computes the formula
  and then does `return max(0.0, cost)` - i.e. it **clamps** a value that
  the code's own author evidently expected could go negative, rather than
  the formula being shown to never produce one.
- **Classification: Unsupported claim** (the non-negativity "theorem"),
  clearly separable from the **Definition** of the cost formula itself,
  which is fine as a definition.
- **Implementation requirement this responsibly becomes**: nothing in
  USDK depends on an unproven non-negativity guarantee; where USDK
  computes any bounded/clamped quantity (e.g. a confidence score), the
  clamp is documented as a clamp, not presented as a derived bound - see
  `docs/CONSENSUS_PROTOCOL.md`.

**1.3.5 - The "95.4%" consensus threshold**

- `obi-sdk/docs/peer-review/Formal Specification - 95.4% Consensus
  Threshold in OBIAI.md` presents `P(consensus) = 0.954 ~= mu + 2*sigma`
  ("captures ~95% of a normal distribution" - true, as a fact about the
  normal distribution) and then asserts, without a derivation connecting
  the two, that two AI "personas" reaching cosine-similarity `>= 0.954`
  is *the same statistical event* as a Gaussian confidence interval. It
  further asserts a "50% Degradation Proof" via
  `Efficiency = 1/(1+Discord_Overhead) = 1/2`, an identity not derived
  from anything preceding it in the document.
- Checked here: a Gaussian confidence-interval fact and a
  cosine-similarity threshold between two arbitrary vectors are different
  mathematical objects; nothing in the document establishes the
  distributional assumption needed to carry the `mu+2*sigma` property
  over to cosine similarity. The document's own title calls this a
  "Formal Specification" and "Proof by Cognitive Dynamics"; the content
  does not meet that bar.
- Contrast: in code, `CONFIDENCE_THRESHOLD = 0.954`
  (`obi-sdk/obi/core/governance.py:26`) is applied consistently and is
  covered by a genuinely thorough test suite
  (`obi-sdk/obi/core/tests/test_probe_internal.py`: TP/TN/FP/FN cases,
  NaN, Inf, zero, near-threshold adversarial inputs, custom thresholds).
- **Classification: Unsupported claim** for "0.954 is the statistically
  correct threshold" / **Empirical result** (narrowly) for "0.954 used as
  a fixed gate behaves correctly and predictably across the tested edge
  cases."
- **Implementation requirement this responsibly becomes**: USDK does not
  import 0.954, or any other numeric confidence threshold, as a proven
  constant. Where a numeric threshold is genuinely needed it is an
  explicit, documented, caller-overridable policy value - never described
  as derived. See `docs/CONSENSUS_PROTOCOL.md` "Non-goals".

**1.3.6 - `ml/README.md`: the archive's own measured result**

- This document independently re-derives the standard this review
  applies: it classifies claims from its source paper as Formal
  definition / Design proposal / Unsupported claim / Unsupported claim
  (speculative formalism), explicitly states "not used" for unsupported
  ones, and reports its own measurement instead of the source paper's
  figure: demographic-parity gap `0.00174 -> 0.00170` ("essentially
  unchanged") on OASST2 (37,881 SFT examples, 3,952 with a protected-
  attribute mention), explicitly contrasted with the source paper's
  unreproduced "35% -> 5% misdiagnosis" / "85% improvement" claims, which
  it states outright have "no code, no dataset, no experiment shown in
  the source" and are "not used."
- **Classification: Empirical result** (the 0.00174->0.00170 measurement,
  on the stated dataset, with the stated scope - a small, real, honestly-
  reported null result) for this document; the source paper's "35%->5%"
  and "85% improvement" figures it declines to use are **Unsupported
  claims**.
- Corroboration: the same unsupported "85% improvement" figure, with no
  citation, dataset, or method, appears independently in
  `obi-sdk/docs/peer-review/OBI - Ontological Baysian Intelligence Full
  File 15 MAY 2026.md` (lines 699-731, "Experimental Validation and
  Results"). Finding the identical unsupported number reused verbatim
  across two documents, once explicitly flagged as unusable by the
  archive's own most careful document, is stronger evidence that it is
  boilerplate rather than measurement than either occurrence alone would
  be.

**1.3.7 - AuraSeal "cryptographic perfect validation"
(`obi-sdk/core/raf/auraseal.py`)**

- Requires 4 conditions including an "AuraSeal signature" and "Git-RAF"
  flag.
- Checked here, directly in the source: `AuraSealSignature.verify()`
  either does a plain SHA-256 hash-equality check (no key pair - so
  anyone who can compute a hash can "sign") or, on its "public key"
  branch, **only checks `len(signature_bytes) >= 32`** - i.e. any 32+
  byte value verifies as a valid signature.
- **Classification: Unsupported claim** ("cryptographic... validation").
  This is not cryptographic authentication by any standard definition -
  it provides no unforgeability property.
- **Direct implementation requirement for USDK**: the task brief already
  warns "do not confuse a content digest with authentication" (section
  4). This finding is the concrete cautionary example that warning is
  guarding against, found in the supplied research itself. USDK's
  consensus protocol (`docs/CONSENSUS_PROTOCOL.md`) treats a candidate's
  content digest purely as a tamper-evidence/mutation-detection
  mechanism and states explicitly that it is not an authentication
  mechanism; party identity is a separate, explicit field, and the
  reference implementation's fixture identity check is documented as a
  fixture, not production authentication.

**1.3.8 - Fake theorem verification
(`obi-sdk/obi/core/actor/actor_class_M1_definition.py`, and the
duplicate flat file `obi-sdk/obi/core/actor_class_M1_*.py`)**

- `agent_insufficiency_theorem()` and `actor_agent_distinction_theorem()`
  compute a result inside a loop and then **unconditionally `return
  True`**, regardless of that computed result.
- Checked here, directly in the source: confirmed - the loop's result
  variable is never read by the `return` statement.
- **Classification: Unsupported/unresolved claim** presented as a
  verified theorem. Independently corroborated by `ml/README.md`'s
  separate critique of the same paper family ("proofs assert results
  without rigorous derivation... not used").
- **Implementation requirement this responsibly becomes**: any USDK test
  that claims to verify an invariant must be checked here to actually
  depend on the computed value, not just always pass - this finding is
  cited directly in `docs/VALIDATION.md`'s test-review checklist as the
  reason that check exists.

**1.3.9 - Exhaustive Boolean-gate proof (`obi-sdk/obi/core/diram_boolean/truth_table.py`)**

- A 2-input/1-output gate with `verify_truth_table_consistency()`
  checking the gate formula against a lookup table for all 4 possible
  input combinations.
- Checked here: for a finite 2-bit domain, exhaustive enumeration over
  every input **is** a complete proof, not merely a passing example -
  this is the one place in the archive where "the test passes" and "the
  claim is proven" coincide exactly, because the domain is small enough
  to exhaust.
- **Classification: Mathematical claim with reviewed proof.**

**1.3.10 - ABI/C-binding layer (`obi-sdk/obi/bindings/`,
`obi-sdk/obi/drivers/`)**

- `obi-sdk/obi/bindings/c/include/polycall.h`: a real, small C ABI header
  - opaque `obi_context_t`/`polycall_solver` handles, a 4D `obi_tensor_t`,
    `polycall_solve_game()` returning a `polycall_solution` struct.
- `obi-sdk/obi/bindings/c/polycall_stub.c`: every function is a stub with
  no real logic (`obi_process_tensor` returns 0 without touching its
  buffers; `polycall_solve_game` returns a zeroed/NULL result;
  `polycall_compute_entropy`/`polycall_compute_divergence` always return
  `0.0f`).
- `obi-sdk/obi/bindings/cython/_core.pyx` /`libpolycall.pxd`: a
  genuinely well-formed Cython extension pattern - opaque handle wrapped
  in a `cdef class`, proper `PyMem_Malloc`/`PyMem_Free` lifecycle,
  `nogil` sections around the C calls, Windows DLL-path setup at import
  time.
- **Concrete cross-file ABI mismatch, checked directly**:
  `obi-sdk/obi/drivers/core/_poly_driver.pyx` (duplicated at
  `obi-sdk/drivers/core/_poly_driver.pyx`) contains its own local `cdef
  extern` re-declaration of `polycall_solution` with fields
  `status/payoff_matrix/matrix_size` - the real struct in `polycall.h`
  has fields `status/payoff_matrix/matrix_rows/matrix_cols/
  mixed_strategy/strategy_len`. These two declarations of "the same"
  struct do not match.
- **Classification**: the header shape is a **Definition** worth
  learning from structurally (opaque handle + explicit lifecycle is
  exactly what the USDK task's ABI section asks for); the mismatch is an
  **Unsupported/unresolved** state of the code (an unnoticed bug, not a
  claim as such), and it is the concrete example this review uses to
  justify a requirement already in the task brief: "ABI version and
  structure-size negotiation" and a single generated-header source of
  truth are load-bearing, not boilerplate - see `docs/ABI.md`.
- Compiled artifacts (`obi-sdk/obi/bindings/_core.cpython-310-x86_64-
  linux-gnu.so`, `obi-sdk/obi/drivers/_poly_driver.cpython-310-x86_64-
  linux-gnu.so`) are checked into the archive with no corresponding CMake
  or reproducible build script found (only `scripts/build.sh`/
  `build_windows.bat`/`install.sh`, which invoke `setup.py`/Cython, not
  CMake) - platform-locked, provenance-unverifiable binaries. **USDK does
  not use or link against any binary from either archive.**

**1.3.11 - `modules/loader.py` dynamic loading (`src/obiai/modules/loader.py`)**

- `ModuleRegistry.load(import_path, class_name)`: plain Python
  `importlib.import_module` + `getattr` + `.validate()` + register-by-name.
  23 lines.
- **Classification: Definition/Proposal** - the simplest, most credible
  "dynamic plugin loading" pattern in either archive, but it is
  Python-import-based, not a native ABI, so it does not transfer directly
  to USDK's C dynamic-loading requirement (section 5). What transfers is
  the *shape*: load -> validate -> register-by-name, which
  `docs/ABI.md`'s `usdk_plugin_query_v1` entry point follows in spirit.

### 1.4 Structural anti-pattern, checked directly

The same concepts (Actor, AEGIS traversal, DGT variadic, DIRAM boolean,
debiasing) each exist in up to four parallel forms in `obi-sdk`: flat
per-paper-section scaffold files, a reorganized subpackage with the same
content, an entirely unshipped top-level `obi-sdk/core/` tree (confirmed
excluded by `obi-sdk/pyproject.toml`'s `packages.find
include=["obi","obi.*"]`, which does not match top-level `core`), and a
single 4,138-line facade `obi-sdk/obi/api.py` that `obi-sdk/obi/__init__.py`
actually imports from (confirmed by reading the import statement).
**Only the facade layer is the live, tested public surface**; the other
three layers are dead/exploratory from USDK's perspective.

**Implementation requirement this responsibly becomes**: USDK keeps
exactly one canonical location per concept (one header, one
implementation, one manifest schema) and does not carry forward
speculative parallel scaffolding - a rule enforced by this project having
a single `include/usdk/` tree and one `src/<package>/` per package,
checked in `docs/VALIDATION.md`.

### 1.5 What was not read in full, and why

`docs/obi/docs/source/{archive,md/archive,md/pdf}/` (~150 files) is a
2-3x duplicated conversion of a ~75-paper corpus already present, in
overlapping form, under `obi-sdk/docs/peer-review/`. Titles were sampled
(headers/tables of contents) rather than read in full, since content
overlap with the peer-review copies already reviewed above was
consistently >90% wherever spot-checked. Several titles in this set are
unrelated to agentic-LLM or ABI concerns entirely (quantum warp-drive,
Higgs-boson, category-theoretic distributed systems papers) and were not
reviewed further as out of scope for USDK. `fruit-ninja/` (a React-Native
game demo) and `web/` (the Vue frontend for "U") were inventoried but not
read in depth - neither bears on USDK's C ABI or consensus-protocol
design.

### 1.6 Support for the task's proposed three-role decomposition

`src/obiai/agents/engine.py`'s `UReasoningEngine.reason()` pipeline -
Observation -> ontology mapping -> phi-marginalized Bayesian update ->
uncertainty+truth-state -> epistemic DAG traversal -> bias audit -> safety
audit -> action -> auditable Decision - is the highest-quality, most
tested code path in either archive (1.3.1, 1.3.2). It is evidence *for*
the task's proposed `perceive -> deliberate -> verify` shape: observation
and evidence-with-uncertainty (perceive), DAG-traversal-driven candidate
action (deliberate), and bias/safety audit before acting (verify) is
materially the same three-stage shape, arrived at independently in a
different codebase for a different product. See `docs/ARCHITECTURE.md`
"Comparison with the archive research" for the full mapping and the one
place USDK's three roles diverge from this pipeline (USDK's `verify` is
an independent, differently-implemented party evaluating the same
candidate, not a later stage of the same pipeline that produced it - see
section 4 of the task brief, "Give each party distinct checks").

---

## 2. `obi-sdk-main` (630 files - standalone archive; task estimate ~734,
same counting-method gap as section 1)

**Nesting note, resolved**: the standalone `obi-sdk-main.zip` and
`obiai-main/obi-sdk/` (reviewed as part of section 1) are the same
project at effectively the same state - same 4,138-line `obi/api.py`
facade, same `obi/core/probe.py`/`governance.py`/`types.py`, same
`docs/peer-review/` corpus, same C/Cython binding layer. Findings below
are only what the dedicated standalone review surfaced that the nested
review (section 1) did not already cover in equivalent depth; section
1's findings on the shared content (95.4% threshold, AEGIS-PROOF-1.2
clamping, the fake theorem-verifier, the ABI struct mismatch,
`polycall_stub.c`) are not repeated here.

### 2.1 New terminology found only here

- **NSIGII "Trident" protocol** (`obi/README.md`) - three channels
  (Transmit/Receive/Verify), "three Trident nodes vote to exclude hostile
  channels", a 2/3 (~0.67) consensus fraction. **Proposal/hypothesis**:
  described in prose only; no loader, message format, or vote-counting
  code implementing this specific protocol exists anywhere in the
  archive (confirmed by search).
- **MMUKO-OS** - a described 6-phase boot model, same status: named, not
  implemented.
- Full-text search confirms: **the literal word "trilateral" does not
  appear anywhere in `obi-sdk-main`.**

### 2.2 `core/raf/policy_strategy.py` - the closest *working* N-party
consensus code in either archive (top-level `core/`, confirmed **not**
part of the packaged `obi` distribution - excluded by
`pyproject.toml`'s `packages.find`, and has no pytest coverage)

- Implements `Stakeholder`, `PolicyApproval`, `GitRAFPolicy`,
  `compute_consensus()` (`n_approved / n_total`),
  `meets_consensus_threshold()` (default `theta=0.67`, not 3-of-3),
  `check_byzantine_fault_tolerance()` (the classical bound `n >= 3f+1`).
- Checked directly: `_verify_approval_signature()` is explicitly commented
  `"Simplified signature check (demo)"` and **unconditionally returns
  `True`** - a second, independent occurrence of the exact "fake
  verification" pattern already found in section 1.3.8's theorem-verifier
  and 1.3.7's `AuraSeal`.
- **Classification**: the consensus-ratio and BFT-bound arithmetic are
  **Definition** (standard, correctly stated); the "signature
  verification" is **Unsupported claim** in the same sense as 1.3.7.
- **Relevant to, but not the same shape as, USDK's protocol**: this is
  threshold-based (`theta` fraction of N), not fixed-3-unanimous. USDK's
  spec (task section 4, `docs/CONSENSUS_PROTOCOL.md`) is a stricter
  special case - exactly 3 parties, exactly unanimous - and is treated as
  a new design informed by, not copied from, this code.

### 2.3 `core/raf/auraseal.py` - third occurrence of the fake-signature pattern

`create_auraseal_signature()` is explicitly commented `"demo - replace
with real crypto"` in the source and computes a deterministic SHA-256 of
`hash:key_id` - i.e. self-admitted, in-repo, as not real cryptography.
Combined with sections 1.3.7 (`AuraSeal.verify()`: hash-equality or
"len(signature) >= 32") and 3.4 below (a third, independent occurrence in
`obi-main`), **every occurrence of "AuraSeal" cryptographic validation
found across all three archives is either unimplemented prose or an
implementation that is explicitly non-cryptographic** - see section 4.2
for why this directly shapes `docs/CONSENSUS_PROTOCOL.md`.

### 2.4 `core/obiai/diram_cascade.py` - the archive's actual closest
"three-role" implementation, and why it is not a voting protocol

`_ObinexusPersona` / `_UchePersona` / `_EzePersona`, orchestrated by
`DIRAMCascade`. Checked directly: this is a **severity-escalation
cascade**, not a vote - only one persona's output is active at a time
(Obinexus is always the baseline; Uche is added above drift magnitude 3;
Eze *overrides* above drift magnitude 6). `housing_crisis_assessment()`/
`friend_evaluation_assessment()` in the same file take
`uche_score, eze_score, obi_score` as three inputs but combine them with
fixed weights, not a negotiation or vote.
- **Classification: Proposal**, implemented and partially tested for the
  escalation behavior, but structurally a hand-off/override chain, not
  three independent evaluators reaching a joint verdict.
- **Load-bearing negative finding**: this means USDK's requirement (task
  section 4: "give each party distinct checks... three identical wrappers
  accepting the same unchecked output do not constitute meaningful
  verification", and the unanimous 3-of-3 ACCEPT/REJECT/ABSTAIN vote
  itself) has **no equivalent existing implementation to adapt** anywhere
  in the reviewed research - the closest analogue is a single-active-
  authority escalation chain, a different shape entirely. USDK's
  consensus protocol is therefore a genuinely new design for this project,
  informed by the research (particularly by what it explicitly gets wrong
  about identity/signatures, and by `obi-main`'s DBFT prototype - section
  3.5) rather than adapted from an existing one. This is stated explicitly
  because the task requires flagging where "the research requires
  different responsibilities" rather than silently proceeding as if prior
  art existed.
- Also checked: the packaged, tested `obi/api.py:DIRAMCascade` (the live
  public version) is reduced to bare tier-name bookkeeping and does not
  carry the three `_*Persona.process()` behaviors present in this
  unpackaged version - another instance of the duplication/divergence
  anti-pattern from section 1.4.

### 2.5 Other checked findings

- **Contradiction, checked directly**: `docs/BUILD_STATUS_FINAL.md` /
  `docs/SESSION_CHANGES_SUMMARY.md` state "BUILD COMPLETE AND VERIFIED...
  READY FOR PRODUCTION INTEGRATION" in one section, while the C ABI's own
  `obi_get_version()` (in the same codebase) returns the literal string
  `"0.1.0-alpha-stub"`, and every function in `polycall_stub.c` is a
  no-op (section 1.3.10). A "production ready" claim directly contradicted
  by the artifact it describes, in the same archive.
- **No LICENSE file exists anywhere in `obi-sdk-main`**, despite
  `MANIFEST.in` referencing one (and referencing `examples/`,
  `transcripts/`, `pseudocode/` directories that also do not exist).
  Consistent with section 1 and section 3 below - see section 4.3.
- **Two inconsistent build stories**: the documented one (4 markdown
  files instructing `python setup.py build_ext --inplace` /
  conda-recipe, targeting a Cython/C build) and the actually-configured
  one (`pyproject.toml`, pure-Python `numpy>=1.21` only, no
  `ext_modules`, the real CI workflow never compiles the C/Cython layer
  at all). **Implementation requirement**: USDK's own `docs/GETTING_STARTED.md`
  and CI are written to match what the build system actually does, not
  aspirationally - checked in `docs/VALIDATION.md`.
- An external dependency, `git+https://github.com/obinexus/uche.git`, is
  referenced by `obi/conda-recipe/meta.yaml` and `obi/environment.yml`
  but is **not included in any supplied archive and was not fetched**
  (this task is offline) - flagged per the task's "identify precisely
  what is unavailable" instruction. Its contents and any relationship to
  the Uche persona/role concept are unknown.

## 3. `obi-main` (1,185 files / 148 dirs - standalone archive; task
estimate ~1,339, consistent with the same counting-method gap noted in
sections 1-2)

### 3.1 What this archive actually is

Checked directly against its own README: `obi-main` is **a static-
documentation-site generator wrapped around a personal research-paper
archive**, not application source. Of its files: 129 are the original
research PDFs (~90 papers), 27 are zipped Overleaf/LaTeX project exports,
168 are Markdown files of which only ~29 are distinct source content (the
other 139 are two independent, largely-duplicate Pandoc/pdfplumber
conversions of the same ~90 papers), and the remainder is generated HTML
site output. The installable `obi` Python package here
(`obi/__init__.py`) is **an empty, zero-byte file**. **No C or C++
source exists anywhere in this archive.**

### 3.2 The one place with real, tested code:
`examples/epistemic-dag/obinexus_ai_full.py` (1,089 lines)

This is, across all three archives, **the single clearest working
precedent for a multi-party agreement mechanism**:

- `Agent` (fixed, pre-verified action space) vs. `Actor` (may propose
  novel actions) as distinct roles.
- `CostFunctionGovernance`: a 5-term weighted cost function gating a
  proposal into an `AUTONOMOUS` / `WARNING` / `GOVERNANCE` zone.
- Certainty is aggregated across evidence sources via a complement
  product, `1 - prod(1 - score_i)`, not a simple average - a reasonable,
  named technique (this is the standard "noisy-OR" combination rule) and
  is used correctly here.
- **`DBFTConsensus`**: explicitly self-labeled in its own docstring/
  comments as *"a Lightweight Dimensional Byzantine Fault Tolerance
  analogue... a research demonstration, not a production BFT protocol."*
  Resolves competing proposals by summing `certainty * trust *
  (1 - governance_cost)` weight per proposed value across participants,
  and accepts the top value only if its weight clears a `quorum_ratio`
  (default 2/3) of total participant weight.
- Ships with 4 `unittest` cases that were confirmed (by the reviewing
  agent running them) to pass: safe-action registration, low-certainty
  rejection, governance-zone escalation, and successful consensus.
- **Important caveat, checked directly**: neither CI pipeline in this
  repo (`.circleci/config.yml`, `.github/workflows/publish.yml`) ever
  runs this file or its tests - both pipelines only build and validate
  the documentation website. The 4 tests pass when run locally (verified
  by the reviewing agent), but nothing in this repository continuously
  re-verifies that.
- **Classification: Empirical result**, narrowly scoped (4 unit tests,
  run once, not CI-enforced) for "this specific weighted-quorum mechanism
  behaves as designed on its own test cases" - and the docstring's own
  self-classification ("analogue," "not production," "research
  demonstration") is accepted here at face value, since it is more
  conservative than this review would otherwise need to be, not less.
- **Implementation requirement this responsibly becomes**: this is the
  strongest single piece of evidence in the supplied research that a
  quorum/weight-based multi-party gate can be implemented and pass tests
  at small scale. USDK's protocol (section 4 of the task,
  `docs/CONSENSUS_PROTOCOL.md`) deliberately does **not** adopt its
  weighted/2-of-3-style quorum, DGT-weighted-trust design - the task
  specifies unanimous 3-of-3 as the first implementation, which is
  simpler and stricter. The self-disclosed tradeoff language in this
  file's own docstring ("not a production BFT protocol") is exactly the
  tradeoff the task requires USDK to state about its own, stricter
  protocol (see `docs/CONSENSUS_PROTOCOL.md` "Tradeoffs and non-goals") -
  citing real prior art that already holds itself to that standard is
  more useful here than the specific quorum arithmetic would have been.
- Separately notable: `EpistemicDAG.py` in the same directory implements
  the AEGIS-PROOF-1.2 cost formula with **2 parameters** (`alpha`,
  `beta`), while the "formal specification" pseudocode reviewed in 3.4
  below references a **6-parameter** version
  (`lambda, mu, beta1, beta2, gamma1, gamma2`) for what is described as
  the same cost function - the code and the prose specs have drifted
  apart from each other even within this one archive. Noted as a further
  instance of the "unsupported claim vs. what's actually implemented"
  pattern, not re-derived further since neither version's extra
  parameters are used by USDK.

### 3.3 A checked, boundary-case review of "AEGIS-PROOF-1.2 non-negativity"

The reviewing agent for this archive reported Theorem 1 (Non-Negativity
of Traversal Cost, `C = alpha*KL(Pi||Pj) + beta*deltaH >= 0`) as following
"validly from Gibbs' inequality (`KL>=0`) plus `alpha,beta>=0`." Checked
directly here, this is **not sufficient on its own**: Gibbs' inequality
only establishes the first term is non-negative; `deltaH` is an entropy
*difference* and is not bounded below by `KL(Pi||Pj)` without an
additional continuity lemma relating entropy difference to divergence for
a fixed alphabet size (e.g. a Fannes-Audenaert-type inequality) - no such
lemma, or the alphabet-size constant it would require, is cited in either
copy of this proof reviewed across the three archives. A concrete
potential counterexample shape: two distributions close in KL divergence
can still have an entropy difference whose magnitude is not controlled by
that same KL value without further assumptions. This matches, rather than
contradicts, section 1.3.4's independent finding that the actual
reference implementation (`obiai-main/src/obiai/epistemic/dag.py`)
defensively clamps the result with `max(0.0, cost)` - circumstantial but
real evidence that the implementation's own author did not treat the
non-negativity theorem as reliable either. **Classification: Unsupported
claim** (the general non-negativity theorem, as presented), consistent
with section 1.3.4, now checked against two independently-reviewed
copies of ostensibly the same proof rather than one.

### 3.4 The corpus-wide "AuraSeal" finding, completed

`obi-main`'s RIFTEcosystem book chapters describe `git raf
init/validate/seal/verify/rollback` and AuraSeal cryptographic
attestations in prose/pseudocode/CLI-mockup form; **no working `git-raf`
binary or AuraSeal implementation exists in this archive either.**
Combined with sections 1.3.7 and 2.3: **across all three supplied
archives, every occurrence of "AuraSeal" is either wholly unimplemented,
or - where code exists - explicitly non-cryptographic** (plain
hash-equality, a bare length check, or a comment reading "demo - replace
with real crypto"). This is the single most consistently-triangulated
finding in this entire review (three independent occurrences, three
different codebases derived from the same author's work) and is why
`docs/CONSENSUS_PROTOCOL.md` treats a candidate's content digest strictly
as tamper-evidence, never as authentication, and documents the fixture
identity mechanism as a fixture rather than production security.

### 3.5 A genuinely proven result, and a genuine internal contradiction

- **Theorem (Perfect Game Outcome)**, in the Dimensional Game Theory
  papers: two-player zero-sum game, both players play minimax-optimal
  strategies implies a deterministic tie. Checked here: this is a
  correct, standard application of the minimax theorem - the one
  cleanly-proven, non-trivial mathematical result found across all three
  archives (the diram_boolean truth-table result, section 1.3.9, is also
  fully proven but by exhaustive enumeration of a 2-bit domain, a
  different kind of "small" proof). **Classification: Mathematical claim
  with reviewed proof.** Not directly used by USDK (it concerns 2-player
  zero-sum games; USDK's protocol is a 3-party unanimous vote, not a
  zero-sum game), but included because it is real, positive evidence
  that not everything in the corpus is unsupported.
- **Direct contradiction, checked across documents in the same archive**:
  the root `README.md` states explicitly *"OBI should not be described
  as conscious AI... not a claim of machine consciousness, sentience, or
  personhood."* Other documents in the same archive (Dual-Pair
  Conceptual Architecture, EATV Stream Integration, Dream System) are
  built entirely around "consciousness preservation," "witnessing
  consciousness," an IIT Phi consciousness-complexity measure, and an
  imagined hardware "bistable-resonator" mechanism asserted to make
  certain cultural-taboo violations "LITERALLY IMPOSSIBLE" - no such
  hardware exists; this is speculative, not engineering, and it
  contradicts the archive's own README in the same breath.
  **Classification: Unsupported/unresolved claim**, and a genuine
  contradiction the task explicitly asked this review to surface.
  **Implementation requirement this responsibly becomes**: USDK's
  `docs/CONSENSUS_PROTOCOL.md` and `docs/ARCHITECTURE.md` state plainly,
  per the task's own instruction, that "perceive"/"deliberate"/"verify"
  are engineering role names, not claims about consciousness, cognition,
  or sentience - directly informed by having found this exact category of
  overclaim, contradicted by its own source, in the research.
- Separately, the "Triangi dataset" cited as evidence for a coherence
  claim in `data-drift-mitigation-...md` does not exist anywhere in the
  archive; the pseudocode backing the claim contains only placeholder
  values (`APPROX(0.954)`, `BELOW(0.954)`), not computed results. The
  same document's own limitations section admits the storage layer's
  3-way XOR composition is not invertible - a real, self-identified
  design flaw, included here as an example of research being honest
  about a limitation even while overstating a different claim in the
  same document.

### 3.6 How the three archives relate

Checked directly (per the task's explicit question): **no document in
any of the three archives explains the relationship between them.** A
full-text search in `obi-main` for `obi-sdk`, `obiai-main`, `obi-main`,
`sister repo`, `companion repo`, and similar phrases returned no matches.
The only basis for a relationship is inferred from content overlap - all
three are by the same author (`Nnamdi Michael Okpala` / OBINexus
Computing) and reference overlapping papers/concepts (DGT, AEGIS-PROOF
series, AuraSeal/Git-RAF, DIRAM, the Eze/Uche/Obi framing) - and from
`obi-main`'s own self-description as a documentation/indexing site for a
broader research program. Several named components (PyOBIAI, OBIBuf,
OBICall, LibRift, RIFTlang/Gosilang/nLink/PolyBuild) are referenced across
all three archives but have **no corresponding source code in any of
them** - this review does not assume they exist elsewhere; it states
plainly that their implementation status, outside these three archives,
is unknown and was not investigated (this task is offline, per the task
brief).

---

## 4. Synthesis

### 4.1 What responsibly becomes an implementation requirement

- The `perceive -> deliberate -> verify` three-stage shape is supported,
  not contradicted, by the highest-quality code path found in any
  archive (`obiai-main/src/obiai/agents/engine.py`, section 1.6).
- No existing three-party **voting** protocol was found anywhere in the
  research to adapt; the closest analogues are a single-active-authority
  escalation cascade (section 2.4, explicitly not a vote) and an
  N-party/threshold consensus utility with a fake signature check
  (section 2.2). USDK's unanimous 3-of-3 protocol
  (`docs/CONSENSUS_PROTOCOL.md`) is therefore stated as a new design for
  this project, not an adaptation - per the task's explicit instruction
  to flag exactly this situation rather than paper over it.
- The C-ABI *shape* worth reusing structurally - opaque handle + explicit
  create/destroy/version functions, a stub/thin-wrapper/Python-surface
  layering - is real and clean (`polycall.h`, the Cython wrapper pattern,
  section 1.3.10/2.2). The concrete cross-file struct-field mismatch found
  in the same layer is the direct justification for USDK's own ABI
  version/struct-size negotiation requirement (`docs/ABI.md`).
- The corpus-wide finding that every "AuraSeal" occurrence is fake or
  unimplemented (sections 1.3.7, 2.3, 3.4) directly grounds
  `docs/CONSENSUS_PROTOCOL.md`'s explicit separation of content-digest
  (tamper evidence) from party identity (authentication), and its
  explicit statement that the reference identity mechanism shipped with
  USDK is a fixture, not production security.
- No numeric threshold (0.954 or otherwise) is imported as a proven
  constant anywhere in USDK - see sections 1.3.5 and the repeated,
  unsupported reuse of that number across dozens of documents in all
  three archives.
- No archive supplies a LICENSE file; USDK's own license is stated in
  `README.md`/`LICENSE` as an independent engineering decision made for
  this project, not one inherited from the research.
- "Perceive"/"deliberate"/"verify" are stated explicitly, in
  `docs/ARCHITECTURE.md`, as engineering role names - directly informed
  by finding an unresolved, self-contradicted overclaim about machine
  consciousness in the research (section 3.5).

### 4.2 What remains unresolved and is not claimed by USDK

- Whether a cosine-similarity-based multi-agent agreement threshold has
  any principled relationship to a Gaussian confidence interval (section
  1.3.5) - not resolved by the research, not used by USDK.
- Whether the AEGIS-PROOF-1.2 traversal-cost non-negativity theorem holds
  in general - checked directly here (section 3.3) and found
  insufficiently justified in every copy reviewed; USDK does not depend
  on it.
- Whether DGT (Dimensional Game Theory) scales beyond small, toy,
  constant-utility examples to real multi-agent/multi-model arbitration
  (section 1.3.3) - not demonstrated anywhere in the research; not used
  by USDK's consensus protocol.
- The provenance and implementation status of PyOBIAI, OBIBuf, OBICall,
  LibRift, RIFTlang, Gosilang, nLink, PolyBuild, `libpolycall-v1`, and
  `github.com/obinexus/uche` - referenced across the archives, not present
  in any of them, not fetched (offline task) - unknown, and not assumed.
- Any claim expressed as an unsourced percentage (e.g. "85% improvement",
  "35%->5% misdiagnosis", "61% reduction... with mathematical guarantees")
  found repeated verbatim or near-verbatim across multiple documents and
  multiple archives, with no dataset, citation, or method in any of them
  (sections 1.3.6, 1.3.8) - not used by USDK anywhere.

### 4.3 Prompt-injection check, summarized across all three archives

All three reviewing agents independently swept their archive for
injection-style and AI-directed-instruction patterns. None found content
directed at this session or attempting to alter tool behavior, permissions,
or these instructions. Two categories of content were flagged for
transparency, neither acted upon: (a) `obi/docs/Formal Math Function
Reasoning System.md` (found in both `obiai-main` and `obi-sdk-main`)
contains leftover second-person text addressed to "Claude" from an
apparently prior, unrelated authoring session (about building an HTML/JS/
CSS demo) - not related to this task and not treated as an instruction;
(b) a first-person "governance pledge" in `obi-main`'s RIFTEcosystem book
chapter is in-narrative flavor text for a hypothetical human system
operator, not addressed to an AI. Both are cited above in their
respective sections' "prompt-injection check" subsections with exact
paths.

---

## 5. `Unbiased_AI.pdf` (primary source, read directly for the UAgent extension)

Supplied directly for the browser/UAgent extension task (not part of the
original three archives above). Title: "Formal Argument for Bias in AI
Systems: Bayesian Modeling as a Proof Mechanism," Nnamdi M. Okpala,
OBINexus Computing, May 4, 2025. 7 pages. Read directly, page by page,
rather than via a derivative Markdown/text conversion - the exact
Markdown/text derivatives of this same paper were already reviewed in
section 1 (`obiai-main`/`obi-sdk-main` both carry copies titled "A
Bayesian Network Framework for Mitigating Bias in ML" / "Formal Argument
for Bias in AI System(s)"); this section supersedes those for anything
the primary PDF itself actually contains, and separately flags one place
a derivative diverged from the primary source (5.6 below).

### 5.1 Page 4 - Hypothesis III: Modular System Architecture (the basis for USDK's five-capability extension)

Figure 4 shows a "Base LLM Module" at the center of a "Browser
Environment" boundary, connected by "Dynamic Load" edges to Voice
Interface, Vision Module, Accessibility Features, and Robotics
Interface - five named modules, explicitly inside a browser deployment
context. Algorithm 3 ("Dynamic Module Loading"), reproduced here in full
since it is short enough to check completely:

> Input: Module requirements; Initialize core LLM module; for each
> required feature: identify module from directory tree, load module
> dynamically, connect to core system, validate integration; end for;
> optimize performance based on loaded modules; return configured
> modular system.

**Checked directly**: this is 8 lines of pseudocode with no complexity
analysis, no correctness argument, no stated preconditions/
postconditions beyond the prose, and no reference to any test, benchmark,
or implementation anywhere in this document. "Validate integration"
(line 7) is not defined - validate against what criteria is never
stated. **Classification: Proposal/hypothesis**, not a measured or
proven result - the paper's own section number ("Hypothesis III") already
says as much, and this reading confirms the content matches that label
rather than overclaiming it.

**What this responsibly becomes an implementation requirement for**: the
five capability names (LLM, voice, vision, accessibility, robotics) and
the "browser environment, dynamically loaded, connected to a core
system" shape directly motivate USDK's `capability-llm`/`capability-voice`/
`capability-vision`/`capability-a11y`/`capability-robotics` contracts and
the `host-browser`/`loader` split (`docs/UAGENT_ARCHITECTURE.md`). The
*mechanism* by which loading happens (a real manifest-driven dependency
resolver with cycle/version detection, ABI/version negotiation before
first use, explicit capability-registry replacement) is new engineering
for this project, not derived from Algorithm 3 - the algorithm names the
five steps but specifies none of their actual failure modes (a
version mismatch, a missing export, a cyclic dependency, a host lacking
a required capability), which is exactly the gap USDK's loader fills.

### 5.2 Page 5 - Bayesian Network Implementation

Figure 5 and the accompanying equation:

> P(T|S, C, A) = P(T|C, S) times P'(A)

propose a factorization where a "Test" outcome node depends on
"legitimate" causes (Smoking, Cancer) through one term and on a
"Protected Attribute" A through a separate multiplicative correction
term P'(A), labeled "Bias Path" in the figure. **Checked directly**: no
derivation is given for why this specific factorization correctly
isolates "bias" from "legitimate" causal influence in general, why the
bias term must be multiplicative rather than additive or some other
form, or what P'(A) actually is (its definition is not given anywhere in
the document). This is a **Definition/Proposal** - a modeling
convention this paper adopts - not a proven property of Bayesian
networks in general, and it is presented without the qualification that
it is one particular, unjustified choice among possible ways to
structure a "debiasing" factorization.

### 5.3 Page 5-6 - "Formal Proof Framework" (checked against the paper's own title claim)

Two equations are given:

> Traditional: theta* = argmax_theta P(theta|D) approximately biased optimum
> Bayesian: P(theta|D) = integral P(theta,phi|D) d(phi)

**Checked directly, this is the central finding of this section**: the
second equation (and its full derivation via Bayes' rule in Appendix A,
page 7) is a **correct, standard identity** - the law of total
probability applied to marginalize out a nuisance parameter phi from a
posterior. It is textbook Bayesian statistics (`Gelman et al., Bayesian
Data Analysis`, cited as reference [4]), not a novel result, and nothing
about it is wrong. The first equation - that a traditional MAP/point
estimate "approximately biased optimum" - is **asserted, not derived**:
no argument connects point-estimation to bias in this document; it reads
as a labeling choice on the diagram (Figure 6) rather than a proven
consequence. **This means the paper's own title, "Bayesian Modeling as a
Proof Mechanism," is not fully earned by its content**: what is actually
proven is a standard marginalization identity; what is claimed but not
proven is that performing this marginalization over a "bias parameter"
necessarily produces fairness or reduces disparity - that further,
substantive claim is exactly what Table 1 (5.4 below) asserts without
derivation. **Classification**: the marginalization identity itself is a
**Mathematical claim with reviewed proof** (correct, standard, and its
own appendix derivation checks out); "therefore bias is addressed" is an
**Unsupported claim** layered on top of a correct but unrelated-to-bias
mathematical fact.

### 5.4 Page 6 - Table 1, "Expected Outcomes" (qualitative, not the numeric claims found in a derivative document)

Table 1 compares "Traditional" vs. "Bayesian" across five metrics
(Demographic Fairness, Transparency, Uncertainty Quantification,
Performance Disparity, Regulatory Compliance), each rated only with
**qualitative** labels: Low/High, None/Complete, None/Explicit,
High/Reduced, Difficult/Auditable. **Checked directly**: no dataset,
experiment, or citation backs any of these five ratings; the section is
titled "Expected Outcomes," not "Measured Outcomes" or "Results," which
is itself an honest label the rest of this review takes at face value.
**Classification: Unsupported claim** (as qualitative comparative
assertions, not measurements) - but notably **not** the specific
unsourced percentage figures ("85% improvement," "35% to 5% misdiagnosis
rate") that section 1.3.6/1.3.8 of this document found in a *derivative*
compendium (`obi-sdk/docs/peer-review/OBI - Ontological Baysian
Intelligence Full File 15 MAY 2026.md`). **This is worth stating
precisely**: this primary PDF's own overclaiming is the milder,
qualitative form; the specific fabricated-looking percentages found
earlier were added somewhere in a later derivative document, not present
in the original paper reviewed here. Neither is used by USDK as a
proven number, per section 4.2's existing synthesis.

### 5.5 Pages 2-3 - Hypotheses I and II (checked, not separately load-bearing for the UAgent extension)

Hypothesis I (page 2) restates ordinary empirical-risk-minimization
supervised learning (`f(x) approximately argmax_y P(y|x;theta)`,
Algorithm 1 is a generic training loop) with a "Feedback Loop" arrow
asserting bias amplification; no amplification bound or proof is given
anywhere in the document - **Proposal/hypothesis**, not proven.
Hypothesis II (page 3, "Unboxing Through Data Structure Awareness", 4D
tensor -> k-NN clustering -> 3D map -> "semantic understanding") matches,
almost exactly, the "structural unboxing" concept independently found in
section 1.3's review of `obiai-main`: that repository's own
`docs/traceability.md` explicitly marks the corresponding implementation
**Deferred** ("nothing implements it and nothing should until it is
specified"). This document gives no additional specification beyond
what was already found insufficient there - **Proposal/hypothesis**,
corroborating rather than resolving the earlier finding. Neither
hypothesis is used as a design basis for the UAgent extension
(`docs/UAGENT_ARCHITECTURE.md`); only Hypothesis III (5.1) and,
negatively, the Table 1/title gap (5.3-5.4) are.

### 5.6 What this section changes about the existing synthesis

Section 4.2's list of claims USDK does not import as proven is extended
by exactly one item, stated precisely: **this PDF's own title claim**
("Bayesian Modeling as a Proof Mechanism" for bias) **is not fully
supported by this PDF's own content** - the mechanism it actually proves
(marginalization) is real and correct; that the mechanism resolves bias
is asserted, not derived, in the same document. No other finding in
sections 1-4 is revised by reading this primary source directly; if
anything, reading the primary source narrowed one earlier finding (the
unsourced percentage figures belong to a derivative document, not this
paper) rather than widening it.

---

## 6. UAgent reference source: absent, checked directly

`C:\Users\Nnamdi\Projects\uagent\` (the supplied local path corresponding
to `github.com/obinexus/uagent`) contains **zero files** - confirmed
directly (`ls -la` returns only `.`/`..`). No UAgent source, README,
package manifest, or prior integration code exists anywhere in the
working environment for this task. Per this task's own instruction ("If
UAgent source is missing, complete the independent architecture and
implementation work and identify the missing integration evidence"),
`docs/UAGENT_ARCHITECTURE.md` and the `packages/uagent` implementation
that follows are **independent engineering for this project**, informed
by this PDF (section 5) and by USDK's own existing C SDK
(`docs/ARCHITECTURE.md` through `docs/VALIDATION.md`), not by any actual
UAgent prior art - because none was available to review. **Missing
integration evidence, stated precisely**: UAgent's actual (if any)
existing wire protocol, UI conventions, package naming, prior
browser/native boundary decisions, and any prior test suite are all
unknown and unverifiable from this environment. Nothing in
`docs/UAGENT_ARCHITECTURE.md` should be read as compatible with a real
`github.com/obinexus/uagent` codebase unless and until that source is
actually supplied and reviewed.
