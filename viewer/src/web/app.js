// Entrée du viewer (WebView2) : branche le pont natif sur la logique testable (store.js).
// Le pont natif est injecté ici — store.js/protocol.js restent testables en Node (D19).
import { createStore, createMockBridge } from './store.js';
import { createI18n } from './i18n.js';
import { parseDashboardFile } from './dashboard_file.js';

const i18n = createI18n(localStorage.getItem('lang') || 'fr');

// Pont natif WebView2 (D9) ; en dehors de WebView2 (dev navigateur), pont simulé.
const bridge = (typeof window.chrome !== 'undefined' && window.chrome.webview)
  ? {
      sendToNative: (m) => window.chrome.webview.postMessage(JSON.stringify(m)),
      onNativeMessage: (h) => window.chrome.webview.addEventListener('message', (e) => h(e.data)),
    }
  : (() => { const mock = createMockBridge(); mock.onNativeMessage((m) => store.onPivotMessage(m)); return mock; })();

const store = createStore(bridge, i18n);

bridge.onNativeMessage((data) => {
  const msg = typeof data === 'string' ? JSON.parse(data) : data;
  if (msg.type === 'simEvent') { msg.connected ? store.onSimConnected() : store.onSimDisconnected(); return; }
  store.onPivotMessage(msg);
});

// --- Rendu minimal (la structure draw.io R3 arrive avec le moteur embarqué ; ici rendu DOM) ---
const $ = (id) => document.getElementById(id);
function refresh() {
  $('sim-state').textContent = i18n.t('state')[store.state.simState] ?? store.state.simState;
  $('sim-time').textContent = store.state.simTime.toFixed(2) + ' s';
  $('ratio').textContent = 'ratio ' + store.state.ratio.toFixed(2);
  $('btn-edit').disabled = !store.canEdit();
  renderBoard();
  renderLog();
}

function renderBoard() {
  const board = $('board');
  board.innerHTML = '';
  const dash = store.state.dashboard?.dash;
  if (!dash) { board.innerHTML = '<p>Aucun dashboard chargé.</p>'; return; }
  for (const w of dash.widgets) {
    const el = document.createElement('div');
    el.className = 'widget widget-' + w.type + (store.state.unconnected.includes(w.variable) ? ' unconnected' : '');
    el.style.cssText = `position:absolute;left:${w.x}px;top:${w.y}px;width:${w.width}px;height:${w.height}px`;
    const val = store.state.values[w.variable];
    el.innerHTML = `<span class="label">${w.label || w.variable}</span><span class="val">${val !== undefined ? val : '—'}</span>`;
    if (w.type === 'button' && !store.state.unconnected.includes(w.variable)) {
      el.onclick = () => store.set(w.variable, !(store.state.values[w.variable]));
    }
    if (w.type === 'slider' && !store.state.unconnected.includes(w.variable)) {
      const inp = document.createElement('input');
      inp.type = 'range'; inp.min = 0; inp.max = 100; inp.value = val ?? 0;
      inp.onchange = () => store.set(w.variable, inp.value / 100);
      el.appendChild(inp);
    }
    board.appendChild(el);
  }
}

function renderLog() {
  $('log').innerHTML = store.state.log.slice(-100)
    .map((l) => `<li>${new Date(l.t).toLocaleTimeString()} — ${l.text}</li>`).join('');
}

$('btn-run').onclick = () => store.run();
$('btn-pause').onclick = () => store.pause();
$('btn-stop').onclick = () => store.stop();
$('btn-run-to-end').onclick = () => store.runToEnd();
$('btn-reload').onclick = () => store.reload();
$('btn-edit').onclick = () => { $('editor-pane').hidden = false; };
$('btn-open').onclick = () => store.loadDashboard(prompt('Chemin du fichier .drawio :'));
$('lang').onchange = (e) => { i18n.setLocale(e.target.value); localStorage.setItem('lang', e.target.value); refresh(); };

store.subscribe(refresh);
refresh();
