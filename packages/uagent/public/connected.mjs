import { ConnectedSession } from '@usdk/binding-javascript';

const el = {
  status: document.getElementById('status-line'),
  announcer: document.getElementById('announcer'),
  transcript: document.getElementById('transcript'),
  connectForm: document.getElementById('connect-form'),
  hostUrlInput: document.getElementById('host-url-input'),
  pairingInput: document.getElementById('pairing-input'),
  connectButton: document.getElementById('connect-button'),
  disconnectButton: document.getElementById('disconnect-button'),
  form: document.getElementById('text-form'),
  input: document.getElementById('text-input'),
  sendButton: document.getElementById('send-button'),
  roboticsControls: document.getElementById('robotics-controls'),
  estopButton: document.getElementById('estop-button'),
  roboticsStatus: document.getElementById('robotics-status'),
};

/** Wired directly, same as app.mjs - accessibility must not depend on
 * connection state any more than on capability-load state. See
 * docs/UAGENT_ARCHITECTURE.md "Accessibility is always available at the
 * interface level." */
const a11y = {
  announce(text) {
    el.announcer.textContent = '';
    requestAnimationFrame(() => { el.announcer.textContent = text; });
  },
  setStatus(text) { el.status.textContent = text; },
};

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
}

/** @type {ConnectedSession | null} */
let session = null;

function setConnectedUI(connected) {
  el.connectButton.disabled = connected;
  el.disconnectButton.disabled = !connected;
  el.hostUrlInput.disabled = connected;
  el.pairingInput.disabled = connected;
  el.input.disabled = !connected;
  el.sendButton.disabled = !connected;
  el.estopButton.disabled = !connected;
  for (const btn of el.roboticsControls.querySelectorAll('button[data-direction]')) btn.disabled = !connected;
}

el.connectForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  const hostUrl = el.hostUrlInput.value.trim();
  const pairingSecret = el.pairingInput.value.trim();
  if (!hostUrl || !pairingSecret) return;
  a11y.setStatus('Connecting…');
  session = new ConnectedSession({ hostUrl, pairingSecret });
  try {
    const { loadedCapabilities } = await session.pair();
    setConnectedUI(true);
    a11y.setStatus(`Connected. Loaded: ${loadedCapabilities.join(', ')}`);
    addTranscriptEntry('system', `Connected to ${hostUrl}. Loaded capabilities: ${loadedCapabilities.join(', ')}`);
  } catch (e) {
    session = null;
    a11y.setStatus('Connection failed.');
    addTranscriptEntry('system', `Connection failed: ${e && e.message}`, 'refused');
  }
});

el.disconnectButton.addEventListener('click', async () => {
  if (!session) return;
  await session.disconnect();
  session = null;
  setConnectedUI(false);
  a11y.setStatus('Disconnected.');
  addTranscriptEntry('system', 'Disconnected.');
});

el.form.addEventListener('submit', async (event) => {
  event.preventDefault();
  if (!session) return;
  const text = el.input.value.trim();
  if (!text) return;
  el.input.value = '';
  addTranscriptEntry('user', text);
  a11y.setStatus('processing');
  try {
    const result = await session.sendText(text);
    if (result.a11y?.status) a11y.setStatus(result.a11y.status);
    if (result.status === 'committed') {
      addTranscriptEntry('agent', result.text);
    } else {
      addTranscriptEntry('agent', `Not approved (${result.status})`, 'refused');
    }
  } catch (e) {
    addTranscriptEntry('system', `Request failed: ${e && e.message}`, 'refused');
  }
  a11y.setStatus('idle');
});

el.roboticsControls.addEventListener('click', async (event) => {
  const button = event.target.closest('button[data-direction]');
  if (!button || !session) return;
  const direction = button.dataset.direction;
  el.roboticsStatus.textContent = `Requesting approval for: ${direction}…`;
  try {
    const result = await session.proposeRoboticsAction({ direction, distanceM: 0.3, speedMS: 0.2 });
    if (result.status === 'committed' && result.executionCompleted) {
      el.roboticsStatus.textContent = `Approved and executed: ${direction}`;
    } else if (result.status === 'committed') {
      el.roboticsStatus.textContent = `Approved, but did not complete (stopped or interrupted): ${direction}`;
    } else {
      el.roboticsStatus.textContent = `Not approved (${result.status}): ${direction}`;
    }
  } catch (e) {
    el.roboticsStatus.textContent = `Rejected before approval: ${e && e.message}`;
  }
});

el.estopButton.addEventListener('click', async () => {
  if (!session) return;
  await session.emergencyStop();
  el.roboticsStatus.textContent = 'EMERGENCY STOP engaged - all pending movement cancelled.';
  a11y.announce('Emergency stop engaged.');
});
