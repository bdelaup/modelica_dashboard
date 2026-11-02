// Store du viewer : état dérivé des messages du pivot (testable en Node, pont injecté).
// Backpressure D24 : on écrase — seule la valeur la plus récente compte.
import { MSG, validateMessage } from './protocol.js';
import { parseDashboardFile, unconnectedVariables } from './dashboard_file.js';

export function createStore(bridge, i18n) {
  const state = {
    simConnected: false,
    simState: 'stopped',
    simTime: 0,
    ratio: 1,
    values: {},          // variable -> dernière valeur reçue
    unconnected: [],     // variables du dashboard absentes du modèle (EF-V6)
    dashboard: null,     // { source, file, dash }
    log: [],             // journal de messages (EF-V6, D22)
    editing: false,
  };
  const listeners = new Set();

  function emit(kind) { for (const l of listeners) l(kind, state); }
  function logMsg(text) {
    state.log.push({ t: Date.now(), text });
    if (state.log.length > 500) state.log.shift();
    emit('log');
  }

  // Appelé par le pont natif (WebView2) ou le mock de test avec un message JSON du pivot.
  function onPivotMessage(raw) {
    const msg = typeof raw === 'string' ? JSON.parse(raw) : raw;
    const v = validateMessage(msg);
    if (v !== true) { logMsg(i18n.t('error', v.code)); return; }
    switch (msg.type) {
      case MSG.TICK:
        state.simTime = msg.simTime;
        state.ratio = msg.ratio;
        Object.assign(state.values, msg.values); // écrase (D24)
        emit('tick');
        break;
      case MSG.SIM_STATE:
        state.simState = msg.state;
        emit('simState');
        break;
      case MSG.UNCONNECTED:
        state.unconnected = msg.variables;
        for (const name of msg.variables) logMsg(i18n.t('unconnectedVar', name));
        emit('unconnected');
        break;
      case MSG.DASHBOARD: {
        const dash = parseDashboardFile(msg.xml);
        if (!dash.valid) { logMsg(i18n.t('error', dash.error)); return; }
        state.dashboard = { source: msg.source, file: msg.file, dash };
        logMsg(i18n.t('dashboardLoaded', msg.file));
        emit('dashboard');
        break;
      }
      case MSG.ERROR:
        logMsg(i18n.t('error', msg.message ?? msg.code));
        break;
      default:
        break;
    }
  }

  function onSimConnected() {
    state.simConnected = true;
    logMsg(i18n.t('simConnected'));
    emit('sim');
  }
  function onSimDisconnected() {
    state.simConnected = false;
    logMsg(i18n.t('simDisconnected'));
    emit('sim');
  }

  // Commandes viewer -> pivot -> sim (§8.5). Le pont injecté les transporte.
  function sendCmd(cmd, extra) {
    bridge.sendToNative({ type: 'cmd', ...cmd, ...extra });
  }
  const api = {
    run: () => sendCmd({ cmd: 'run' }),
    pause: () => sendCmd({ cmd: 'pause' }),
    stop: () => sendCmd({ cmd: 'stop' }),
    runToEnd: (duration) => sendCmd({ cmd: 'runToEnd', ...(duration !== undefined ? { duration } : {}) }),
    set: (variable, value) => sendCmd({ cmd: 'set', variable, value }),
    reload: () => bridge.sendToNative({ type: 'reload' }),
    openEditor: () => bridge.sendToNative({ type: 'openEditor' }),
    loadDashboard: (path) => bridge.sendToNative({ type: 'loadDashboard', path }),
  };

  // Rechargement entre deux runs (EF-E4) : interdit pendant un run.
  function canEdit() { return !state.editing && state.simState !== 'running'; }

  return {
    state,
    onPivotMessage,
    onSimConnected,
    onSimDisconnected,
    subscribe: (l) => { listeners.add(l); return () => listeners.delete(l); },
    ...api,
    canEdit,
    logMsg,
  };
}

// Pont simulé pour les tests Node (D19) : enregistre ce que le viewer envoie au natif.
export function createMockBridge() {
  const sent = [];
  const nativeHandlers = [];
  return {
    sent,
    sendToNative: (m) => sent.push(m),
    onNativeMessage: (h) => nativeHandlers.push(h),
    emitFromNative: (m) => nativeHandlers.forEach((h) => h(m)),
  };
}
