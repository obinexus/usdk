import { createCandidate, UsdkError, Status } from '@usdk/contracts';
import { Core, RoundState } from '@usdk/core';
import { ManifestRegistry, loadPlan } from '@usdk/loader';
import * as perceiveRole from '@usdk/perceive';
import * as deliberateRole from '@usdk/deliberate';
import * as verifyRole from '@usdk/verify';
import { validateRoboticsAction } from '@usdk/capability-robotics';

const ROBOTICS_CONSTRAINT = 'robotics:bounded-move';
const TEXT_CONSTRAINT = 'workspace:temp-only';

/**
 * The browser-local execution profile's host. Constructed once per
 * conversation session. See docs/UAGENT_ARCHITECTURE.md "Browser and
 * native boundaries" for how this differs from @usdk/host-local's
 * connected-mode transport.
 */
export class ConversationSession {
  /**
   * @param {Object} opts
   * @param {string} opts.sessionId
   * @param {object[]} opts.manifests - raw manifest objects to register (see @usdk/contracts parseManifest)
   * @param {string} opts.baseUrl - resolved against each manifest's `entry`
   * @param {string} opts.llmManifestName - which registered manifest to use for the LLM capability
   * @param {string} [opts.voiceManifestName]
   * @param {string} [opts.roboticsManifestName]
   * @param {{announce: Function, setStatus: Function, isAvailable: Function}} opts.a11y -
   *   constructed and injected directly by the caller, per
   *   docs/UAGENT_ARCHITECTURE.md: accessibility must not depend on any
   *   capability's successful load.
   * @param {string[]} [opts.allowedConstraints]
   * @param {number} [opts.maxUncertainty]
   * @param {number} [opts.roundTimeoutMs]
   * @param {boolean} [opts.speakApprovedText] default true if a voice driver loaded
   */
  constructor(opts) {
    this._sessionId = opts.sessionId;
    this._manifestsRaw = opts.manifests;
    this._baseUrl = opts.baseUrl;
    this._llmManifestName = opts.llmManifestName;
    this._voiceManifestName = opts.voiceManifestName ?? null;
    this._roboticsManifestName = opts.roboticsManifestName ?? null;
    this._a11y = opts.a11y;
    this._allowedConstraints = opts.allowedConstraints ?? [TEXT_CONSTRAINT, ROBOTICS_CONSTRAINT];
    this._maxUncertainty = opts.maxUncertainty ?? 0.5;
    this._roundTimeoutMs = opts.roundTimeoutMs ?? 8000;
    this._speakApprovedText = opts.speakApprovedText ?? true;

    this._nextRoundId = 1;
    /** @type {string | null} */
    this._currentTurnId = null;
    this._initialized = false;
  }

  /** Registers manifests, resolves + dynamically loads the requested
   * capability set, and constructs the three roles + core. Must be
   * called (and awaited) before sendText()/proposeRoboticsAction(). This
   * is the "at least one conversation path genuinely uses dynamic
   * loading" path this task's acceptance criteria require - the LLM
   * (and, if configured, voice/robotics) driver modules are located and
   * `import()`-ed here, not statically imported anywhere in this file. */
  async init() {
    const registry = new ManifestRegistry();
    for (const m of this._manifestsRaw) registry.register(m);

    const requested = [this._llmManifestName];
    if (this._voiceManifestName) requested.push(this._voiceManifestName);
    if (this._roboticsManifestName) requested.push(this._roboticsManifestName);

    const plan = registry.resolve(requested);
    const loaded = await loadPlan(plan, this._baseUrl);

    const byName = new Map(loaded.map((l) => [l.manifest.name, l]));
    const llmLoaded = byName.get(this._llmManifestName);
    this._llmDriver = llmLoaded.create();

    this._voiceDriver = this._voiceManifestName ? byName.get(this._voiceManifestName).create() : null;
    this._roboticsDriver = this._roboticsManifestName ? byName.get(this._roboticsManifestName).create() : null;

    this._perceive = perceiveRole.create();
    this._deliberate = deliberateRole.create({ llmDriver: this._llmDriver });
    this._verify = verifyRole.create({ allowedConstraints: this._allowedConstraints, maxUncertainty: this._maxUncertainty });

    this._core = new Core();
    this._core.setDispatchFn(async (candidate, idempotencyKey) => {
      const isRobotics = candidate.constraints.some((c) => c.constraintId === ROBOTICS_CONSTRAINT);
      if (isRobotics) {
        if (!this._roboticsDriver) throw new UsdkError(Status.CAPABILITY_NOT_FOUND, 'no robotics driver loaded');
        // Approval and execution-completion are different concerns: the
        // round can legitimately commit (three parties approved the
        // PROPOSAL) while the driver still reports completed:false (e.g.
        // emergencyStop() interrupted it mid-movement) - both facts need
        // to reach the caller, not just "did dispatch throw." Stashed
        // here and read by proposeRoboticsAction() right after decide()
        // resolves, since usdk_core_round_decide's own return value
        // (docs/CONSENSUS_PROTOCOL.md) is only ever the round STATE, by
        // design - it does not carry an arbitrary dispatch return value.
        this._lastDispatchResult = await this._roboticsDriver.execute(JSON.parse(candidate.proposedResponse), idempotencyKey);
      } else if (this._speakApprovedText && this._voiceDriver && this._voiceDriver.isAvailable()) {
        await new Promise((resolve) => this._voiceDriver.speak(candidate.proposedResponse, idempotencyKey, resolve));
      }
    });

    this._initialized = true;
    return { loadedCapabilities: loaded.map((l) => l.manifest.name) };
  }

  _requireInit() {
    if (!this._initialized) throw new UsdkError(Status.INVALID_ARGUMENT, 'ConversationSession.init() must be awaited before use');
  }

  /** True only if a voice driver was configured AND it reports itself
   * available (see @usdk/driver-voice - false outside a browser, or in
   * a browser without SpeechRecognition/SpeechSynthesis). The UI should
   * use this to decide whether to show voice controls at all, rather
   * than showing a control that will immediately fail. */
  get voiceAvailable() {
    return Boolean(this._voiceDriver && this._voiceDriver.isAvailable());
  }

  /** Runtime mute toggle for the "mute spoken responses" control - text
   * display and the approval pipeline are unaffected either way; this
   * only gates the dispatch-time speak() call. */
  setSpeakApprovedText(enabled) {
    this._speakApprovedText = enabled;
  }

  /** Explicit "stop speaking now" - used by the mute control to also cut
   * off an already-in-progress utterance, not just prevent future ones. */
  stopSpeaking() {
    if (this._voiceDriver) this._voiceDriver.stopSpeaking();
  }

  /**
   * Starts microphone capture through the loaded voice driver and,
   * once a final transcript is produced, feeds it through the exact
   * same sendText() turn lifecycle a typed message uses - so a spoken
   * message is subject to the same evidence/candidate/three-vote
   * pipeline as typed text, not a separate path.
   * @param {(interimText: string) => void} onInterim - called with each
   *   in-progress (non-final) transcript fragment, for a live captions display
   * @param {(result: object) => void} onTurnResult - called with the
   *   same shape sendText() resolves to, once the turn completes
   * @param {(reason: string) => void} onError
   * @returns {() => void} stop function - also usable as the explicit
   *   "stop listening"/mute control
   */
  startVoiceInput(onInterim, onTurnResult, onError) {
    this._requireInit();
    if (!this.voiceAvailable) {
      onError('no voice input capability is available in this browser');
      return () => {};
    }
    return this._voiceDriver.startListening(async (text, isFinal) => {
      if (!isFinal) { onInterim(text); return; }
      const result = await this.sendText(text);
      onTurnResult(result);
    }, onError);
  }

  /** Invalidates any turn currently in flight - a superseding call to
   * sendText()/proposeRoboticsAction(), or an explicit cancelTurn(),
   * both funnel through this so a late-arriving result from the
   * superseded turn is discarded rather than spoken/executed/displayed
   * - see docs/UAGENT_ARCHITECTURE.md "Prevention of stale speech." */
  cancelTurn() {
    this._currentTurnId = null;
  }

  /** Bypasses deliberation and any pending round entirely - wired to a
   * dedicated UI control that never waits for the three-party vote. */
  emergencyStop() {
    if (this._roboticsDriver) this._roboticsDriver.emergencyStop();
  }

  /**
   * @param {string} text
   * @returns {Promise<{status: string, turnId: string, text?: string, votes?: object[]}>}
   */
  async sendText(text) {
    this._requireInit();
    const turnId = `turn-${this._sessionId}-${this._nextRoundId}-${Math.random().toString(36).slice(2, 8)}`;
    this._currentTurnId = turnId;
    this._a11y.setStatus('processing');

    const evidenceRef = await this._perceive.observe(text, 0.1);
    const response = await this._deliberate.propose(text, [evidenceRef]);

    if (this._currentTurnId !== turnId) return { status: 'superseded', turnId };

    const roundId = this._nextRoundId++;
    const candidate = await createCandidate({
      policyVersion: 1,
      sessionId: this._sessionId,
      roundId,
      candidateId: turnId,
      evidenceRefs: [evidenceRef],
      proposedResponse: response,
      constraints: [{ constraintId: TEXT_CONSTRAINT }],
      deadlineMs: Date.now() + this._roundTimeoutMs,
    });

    const result = await this._runRound(candidate, turnId);
    if (result.status === 'committed') {
      this._a11y.announce(response);
      this._a11y.setStatus('idle');
      return { ...result, text: response };
    }
    this._a11y.announce(`Not approved: ${result.status}`);
    this._a11y.setStatus('idle');
    return result;
  }

  /**
   * @param {import('@usdk/capability-robotics').RoboticsAction} action
   */
  async proposeRoboticsAction(action) {
    this._requireInit();
    const validated = validateRoboticsAction(action); // fail fast on a malformed/out-of-bounds action, before any candidate exists
    const turnId = `turn-${this._sessionId}-${this._nextRoundId}-${Math.random().toString(36).slice(2, 8)}`;
    this._currentTurnId = turnId;
    this._a11y.setStatus('processing');

    const actionJson = JSON.stringify(validated);
    // perceive observes the proposed action itself as evidence (low
    // uncertainty: it is a structured, already-bounds-checked action,
    // not a noisy sensor reading) - this is what gives @usdk/verify's
    // evidence-sufficiency check something real to evaluate for a
    // robotics candidate, using the exact same evidence_ref mechanism
    // as a text turn rather than a special case.
    const evidenceRef = await this._perceive.observe(actionJson, 0.05);
    await this._deliberate.proposeDirect(actionJson);

    if (this._currentTurnId !== turnId) return { status: 'superseded', turnId };

    const roundId = this._nextRoundId++;
    const candidate = await createCandidate({
      policyVersion: 1,
      sessionId: this._sessionId,
      roundId,
      candidateId: turnId,
      evidenceRefs: [evidenceRef],
      proposedResponse: actionJson,
      constraints: [{ constraintId: ROBOTICS_CONSTRAINT }],
      deadlineMs: Date.now() + this._roundTimeoutMs,
    });

    this._lastDispatchResult = null;
    const result = await this._runRound(candidate, turnId);
    this._a11y.setStatus('idle');
    if (result.status === 'committed') {
      result.executionCompleted = this._lastDispatchResult?.completed ?? false;
    }
    return result;
  }

  async _runRound(candidate, turnId) {
    const round = await this._core.openRound(candidate);
    const votes = [];
    for (const role of [this._perceive, this._deliberate, this._verify]) {
      const vote = await role.vote(candidate);
      votes.push(vote);
      try {
        await this._core.submitVote(round, vote);
      } catch (e) {
        // A deadline-exceeded submitVote throw still leaves useful
        // information in `votes` for the caller/UI to display.
        if (!(e instanceof UsdkError)) throw e;
      }
    }
    const state = await this._core.decide(round);
    if (this._currentTurnId !== turnId) {
      // This turn was cancelled/superseded while the round was being
      // decided - a committed result must not be spoken/executed after
      // the fact (the dispatch already ran as part of decide(), so this
      // only affects what is reported back and announced, not a
      // guarantee against an already-in-flight dispatch side effect -
      // see docs/CONSENSUS_PROTOCOL.md "Commit records and side-effect
      // dispatch" for why dispatch cannot be perfectly cancellable).
      return { status: 'superseded', turnId, votes };
    }
    return { status: state === RoundState.COMMITTED ? 'committed' : state, turnId, votes };
  }
}
