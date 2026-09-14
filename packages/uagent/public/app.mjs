import { ConversationSession } from '@usdk/host-browser';

const el = {
  status: document.getElementById('status-line'),
  announcer: document.getElementById('announcer'),
  transcript: document.getElementById('transcript'),
  form: document.getElementById('text-form'),
  input: document.getElementById('text-input'),
  sendButton: document.getElementById('send-button'),
  micButton: document.getElementById('mic-button'),
  muteButton: document.getElementById('mute-button'),
  cancelButton: document.getElementById('cancel-button'),
  voiceFallbackNote: document.getElementById('voice-fallback-note'),
  roboticsControls: document.getElementById('robotics-controls'),
  estopButton: document.getElementById('estop-button'),
  roboticsStatus: document.getElementById('robotics-status'),
  capabilityList: document.getElementById('capability-list'),
};

/** capability-a11y, wired directly (not through @usdk/loader) so it
 * never depends on any other capability's successful load - see
 * docs/UAGENT_ARCHITECTURE.md "Voice, accessibility, and conversation":
 * "Accessibility is always available at the interface level." */
const a11y = Object.freeze({
  announce(text) {
    // Clearing first, then setting on the next frame, forces screen
    // readers to re-announce even if the text is identical to the
    // previous announcement (a live region only fires on a DOM mutation).
    el.announcer.textContent = '';
    requestAnimationFrame(() => { el.announcer.textContent = text; });
  },
  setStatus(text) {
    el.status.textContent = text;
  },
  isAvailable() {
    return true;
  },
});

function addTranscriptEntry(who, text, cls = '') {
  const div = document.createElement('div');
  div.className = `msg ${who} ${cls}`.trim();
  const whoSpan = document.createElement('span');
  whoSpan.className = 'who';
  whoSpan.textContent = who === 'user' ? 'You: ' : who === 'agent' ? 'UAgent: ' : 'System: ';
  div.appendChild(whoSpan);
  div.appendChild(document.createTextNode(text));
  el.transcript.appendChild(div);
  el.transcript.scrollTop = el.transcript.scrollHeight;
  return div;
}

const manifests = [
  { name: 'driver-llm-fixture', version: '0.1', kind: 'llm', entry: '../../driver-llm-fixture/src/index.mjs', dependencies: [] },
  { name: 'driver-voice', version: '0.1', kind: 'voice', entry: '../../driver-voice/src/index.mjs', dependencies: [] },
  { name: 'driver-robotics-sim', version: '0.1', kind: 'robotics', entry: '../../driver-robotics-sim/src/index.mjs', dependencies: [] },
];

const session = new ConversationSession({
  sessionId: `uagent-${Date.now()}`,
  manifests,
  baseUrl: import.meta.url,
  llmManifestName: 'driver-llm-fixture',
  voiceManifestName: 'driver-voice',
  roboticsManifestName: 'driver-robotics-sim',
  a11y,
  allowedConstraints: ['workspace:temp-only', 'robotics:bounded-move'],
  maxUncertainty: 0.5,
  roundTimeoutMs: 8000,
});

let listening = false;
let stopListening = null;
let muted = false;

async function boot() {
  a11y.setStatus('Loading capabilities…');
  let loadedCapabilities = [];
  try {
    ({ loadedCapabilities } = await session.init());
  } catch (e) {
    a11y.setStatus('Failed to load capabilities');
    addTranscriptEntry('system', `Startup failed: ${e && e.message}`, 'refused');
    return;
  }

  for (const name of manifests.map((m) => m.name)) {
    const li = document.createElement('li');
    const wasLoaded = loadedCapabilities.includes(name);
    li.textContent = wasLoaded ? `${name}: loaded` : `${name}: not loaded`;
    if (!wasLoaded) li.className = 'cap-unavailable';
    el.capabilityList.appendChild(li);
  }

  if (!session.voiceAvailable) {
    el.micButton.disabled = true;
    el.micButton.title = 'Voice input is not available in this browser';
    el.voiceFallbackNote.hidden = false;
  }

  a11y.setStatus('Ready. Type a message or use voice input.');
}

el.form.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = el.input.value.trim();
  if (!text) return;
  el.input.value = '';
  await handleUserTurn(text);
});

async function handleUserTurn(text) {
  addTranscriptEntry('user', text);
  const pending = addTranscriptEntry('agent', 'Thinking… (not yet approved)', 'pending');
  const result = await session.sendText(text);
  pending.remove();
  if (result.status === 'committed') {
    addTranscriptEntry('agent', result.text);
  } else if (result.status === 'superseded') {
    addTranscriptEntry('system', 'This turn was cancelled or superseded before it could be approved.', 'refused');
  } else {
    const reasons = (result.votes ?? [])
      .filter((v) => v.verdict !== 'accept')
      .map((v) => `${v.role}: ${v.reasonCode}`)
      .join('; ');
    addTranscriptEntry('agent', `Not approved (${result.status})${reasons ? ' - ' + reasons : ''}`, 'refused');
  }
}

el.cancelButton.addEventListener('click', () => {
  session.cancelTurn();
  a11y.announce('Current turn cancelled.');
});

el.muteButton.addEventListener('click', () => {
  muted = !muted;
  session.setSpeakApprovedText(!muted);
  if (muted) session.stopSpeaking();
  el.muteButton.setAttribute('aria-pressed', String(muted));
  el.muteButton.textContent = muted ? 'Unmute spoken responses' : 'Mute spoken responses';
});

el.micButton.addEventListener('click', () => {
  if (listening) {
    stopListening?.();
    listening = false;
    el.micButton.setAttribute('aria-pressed', 'false');
    el.micButton.textContent = 'Start listening';
    a11y.setStatus('Microphone stopped.');
    return;
  }
  listening = true;
  el.micButton.setAttribute('aria-pressed', 'true');
  el.micButton.textContent = 'Stop listening';
  a11y.setStatus('Listening…');
  stopListening = session.startVoiceInput(
    (interim) => { el.input.value = interim; },
    (result) => {
      listening = false;
      el.micButton.setAttribute('aria-pressed', 'false');
      el.micButton.textContent = 'Start listening';
      if (result.status !== 'superseded') el.input.value = '';
    },
    (reason) => {
      listening = false;
      el.micButton.setAttribute('aria-pressed', 'false');
      el.micButton.textContent = 'Start listening';
      addTranscriptEntry('system', `Voice input error: ${reason}`, 'refused');
    }
  );
});

el.roboticsControls.addEventListener('click', async (event) => {
  const button = event.target.closest('button[data-direction]');
  if (!button) return;
  const direction = button.dataset.direction;
  el.roboticsStatus.textContent = `Requesting approval for: ${direction}…`;
  try {
    const result = await session.proposeRoboticsAction({ direction, distanceM: 0.3, speedMS: 0.2 });
    if (result.status === 'committed' && result.executionCompleted) {
      el.roboticsStatus.textContent = `Approved and executed: ${direction}`;
    } else if (result.status === 'committed') {
      // Approval and execution-completion are different facts - the
      // round can legitimately commit while the driver still reports it
      // did not finish (e.g. emergency stop interrupted it mid-move).
      el.roboticsStatus.textContent = `Approved, but did not complete (stopped or interrupted): ${direction}`;
    } else {
      el.roboticsStatus.textContent = `Not approved (${result.status}): ${direction}`;
    }
  } catch (e) {
    el.roboticsStatus.textContent = `Rejected before approval: ${e && e.message}`;
  }
});

el.estopButton.addEventListener('click', () => {
  session.emergencyStop();
  el.roboticsStatus.textContent = 'EMERGENCY STOP engaged - all pending movement cancelled.';
  a11y.announce('Emergency stop engaged.');
});

boot();
