// Tests Node du store du viewer avec pont simulé (D19/D33).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createStore, createMockBridge } from '../store.js';
import { createI18n } from '../i18n.js';
import { MSG } from '../protocol.js';

const DASH_XML = `<mxfile dashboardVersion="0"><diagram name="T"><mxGraphModel><root>
<mxCell id="0"/><mxCell id="1" parent="0"/>
<mxCell id="w1" type="value" variable="motor.speed" UserObject="1" vertex="1" parent="1">
<mxGeometry x="0" y="0" width="100" height="40" as="geometry"/></mxCell>
</root></mxGraphModel></diagram></mxfile>`;

function makeStore() {
  const bridge = createMockBridge();
  const i18n = createI18n('fr');
  const store = createStore(bridge, i18n);
  return { bridge, store, i18n };
}

test('tick met à jour simTime, ratio et écrase les valeurs (D24)', () => {
  const { store } = makeStore();
  store.onPivotMessage({ type: MSG.TICK, version: 0, seq: 1, simTime: 1.5, ratio: 0.9, values: { 'motor.speed': 10 } });
  store.onPivotMessage({ type: MSG.TICK, version: 0, seq: 2, simTime: 1.6, ratio: 0.8, values: { 'motor.speed': 20 } });
  assert.equal(store.state.simTime, 1.6);
  assert.equal(store.state.ratio, 0.8);
  assert.equal(store.state.values['motor.speed'], 20); // dernière valeur gagne
});

test('simState, unconnected (EF-V6) avec message dans le journal', () => {
  const { store } = makeStore();
  store.onPivotMessage({ type: MSG.SIM_STATE, version: 0, state: 'running' });
  store.onPivotMessage({ type: MSG.UNCONNECTED, version: 0, variables: ['ghost.var'] });
  assert.equal(store.state.simState, 'running');
  assert.deepEqual(store.state.unconnected, ['ghost.var']);
  assert.ok(store.state.log.some((l) => l.text.includes('ghost.var')));
});

test('dashboard reçu -> parsé, chargé, journalisé', () => {
  const { store } = makeStore();
  store.onPivotMessage({ type: MSG.DASHBOARD, version: 0, source: 'auto', file: 'C:\\m\\T.drawio', xml: DASH_XML });
  assert.equal(store.state.dashboard.dash.widgets.length, 1);
  assert.equal(store.state.dashboard.dash.widgets[0].type, 'value');
  assert.ok(store.state.log.some((l) => l.text.includes('T.drawio')));
});

test('dashboard invalide -> erreur journalisée, pas de crash', () => {
  const { store } = makeStore();
  store.onPivotMessage({ type: MSG.DASHBOARD, version: 0, source: 'manual', file: 'x', xml: '<mxfile><diagram>' });
  assert.equal(store.state.dashboard, null);
  assert.ok(store.state.log.some((l) => l.text.startsWith('Erreur')));
});

test('commandes Run/Pause/Stop/RunToEnd/set passent par le pont (EF-V2..V4, §8.5)', () => {
  const { store, bridge } = makeStore();
  store.run();
  store.pause();
  store.runToEnd(10);
  store.runToEnd();
  store.stop();
  store.set('motor.on', false);
  assert.deepEqual(bridge.sent, [
    { type: 'cmd', cmd: 'run' },
    { type: 'cmd', cmd: 'pause' },
    { type: 'cmd', cmd: 'runToEnd', duration: 10 },
    { type: 'cmd', cmd: 'runToEnd' },
    { type: 'cmd', cmd: 'stop' },
    { type: 'cmd', cmd: 'set', variable: 'motor.on', value: false },
  ]);
});

test('édition interdite pendant un run (EF-E4)', () => {
  const { store } = makeStore();
  assert.ok(store.canEdit());
  store.onPivotMessage({ type: MSG.SIM_STATE, version: 0, state: 'running' });
  assert.ok(!store.canEdit());
  store.onPivotMessage({ type: MSG.SIM_STATE, version: 0, state: 'paused' });
  assert.ok(store.canEdit());
});

test('message invalide -> erreur journalisée, état inchangé', () => {
  const { store } = makeStore();
  store.onPivotMessage({ type: 'nope', version: 0 });
  assert.ok(store.state.log.some((l) => l.text.includes('unknownType')));
  assert.equal(store.state.simTime, 0);
});

test('i18n : fr par défaut, en disponible, fallback', () => {
  const i18n = createI18n();
  assert.equal(i18n.locale, 'fr');
  assert.equal(i18n.t('run'), 'Run');
  i18n.setLocale('en');
  assert.equal(i18n.t('pause'), 'Pause');
  i18n.setLocale('de'); // non supporté -> conservé
  assert.equal(i18n.locale, 'en');
  assert.equal(i18n.t('cle-inexistante'), 'cle-inexistante');
});
