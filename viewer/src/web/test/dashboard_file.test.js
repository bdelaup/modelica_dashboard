// Tests Node du parsing .drawio côté logique JS (miroir de shared/).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { parseDashboardFile, unconnectedVariables, DASHBOARD_VERSION, WIDGET_TYPES } from '../dashboard_file.js';

const SAMPLE = `<?xml version="1.0" encoding="UTF-8"?>
<mxfile dashboardVersion="0" host="modelica_dashboard">
  <diagram name="Demo" id="dashboard">
    <mxGraphModel>
      <root>
        <mxCell id="0"/>
        <mxCell id="1" parent="0"/>
        <mxCell id="w1" type="gauge" variable="motor.speed" label="Vitesse" UserObject="1" vertex="1" parent="1">
          <mxGeometry x="40" y="40" width="120" height="80" as="geometry"/>
        </mxCell>
        <mxCell id="w2" type="button" variable="motor.on" label="Marche" UserObject="1" vertex="1" parent="1">
          <mxGeometry x="200" y="40" width="100" height="40" as="geometry"/>
        </mxCell>
      </root>
    </mxGraphModel>
  </diagram>
</mxfile>`;

test('parse un .drawio valide', () => {
  const d = parseDashboardFile(SAMPLE);
  assert.ok(d.valid);
  assert.equal(d.dashboardVersion, 0);
  assert.equal(d.name, 'Demo');
  assert.equal(d.widgets.length, 2);
  assert.deepEqual([d.widgets[0].type, d.widgets[0].variable], ['gauge', 'motor.speed']);
  assert.equal(d.widgets[0].width, 120);
});

test('widget de type inconnu -> erreur explicite', () => {
  const d = parseDashboardFile(SAMPLE.replace('type="gauge"', 'type="hologram"'));
  assert.ok(!d.valid);
  assert.match(d.error, /hologram/);
});

test('dashboardVersion trop récente -> erreur (ADR-0009)', () => {
  const d = parseDashboardFile(SAMPLE.replace('dashboardVersion="0"', 'dashboardVersion="5"'));
  assert.ok(!d.valid);
  assert.match(d.error, /dashboardVersion/);
});

test('XML malformé -> erreur sans exception', () => {
  assert.ok(!parseDashboardFile('<mxfile><diagram>').valid);
  assert.ok(!parseDashboardFile('rien').valid);
});

test('les 6 types V1 sont acceptés (D32)', () => {
  assert.deepEqual(WIDGET_TYPES, ['value', 'gauge', 'curve', 'button', 'slider', 'log']);
  assert.equal(DASHBOARD_VERSION, 0);
});

test('unconnectedVariables (EF-V6)', () => {
  const d = parseDashboardFile(SAMPLE);
  assert.deepEqual(unconnectedVariables(['motor.speed', 'other'], d), ['motor.on']);
  assert.deepEqual(unconnectedVariables(['motor.speed', 'motor.on'], d), []);
});
