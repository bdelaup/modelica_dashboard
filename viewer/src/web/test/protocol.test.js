// Tests Node (node --test) du contrat JS — miroir de docs/protocole.md v0.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  PROTOCOL_VERSION, MAX_FRAME_BYTES, MSG, ERROR_CODES,
  versionSupported, validateMessage, makeCmd, encodeFrame, decodeFrames,
} from '../protocol.js';

test('constantes du contrat', () => {
  assert.equal(PROTOCOL_VERSION, 0);
  assert.equal(MAX_FRAME_BYTES, 1 << 20);
});

test('version N lit N et N-1 (ADR-0009)', () => {
  assert.ok(versionSupported(0));
  assert.ok(versionSupported(-1));
  assert.ok(!versionSupported(1));
  assert.ok(!versionSupported(-2));
});

test('validateMessage : data valide', () => {
  assert.equal(validateMessage({ type: MSG.DATA, version: 0, seq: 1, time: 0.1, values: { a: 1 } }), true);
});

test('validateMessage : state invalide', () => {
  assert.deepEqual(validateMessage({ type: MSG.STATE, version: 0, state: 'flying' }), { code: ERROR_CODES.BAD_JSON });
  assert.equal(validateMessage({ type: MSG.SIM_STATE, version: 0, state: 'finished' }), true);
});

test('validateMessage : cmd inconnu -> badCmd', () => {
  assert.deepEqual(validateMessage(makeCmd('explode')), { code: ERROR_CODES.BAD_CMD });
  assert.equal(validateMessage(makeCmd('runToEnd', { duration: 10 })), true);
});

test('validateMessage : type inconnu / mauvaise version / pas un objet', () => {
  assert.deepEqual(validateMessage({ type: 'nope', version: 0 }), { code: ERROR_CODES.UNKNOWN_TYPE });
  assert.deepEqual(validateMessage({ type: 'data', version: 42, seq: 1, values: {} }), { code: ERROR_CODES.BAD_VERSION });
  assert.deepEqual(validateMessage([1, 2]), { code: ERROR_CODES.BAD_JSON });
});

test('framing NDJSON : multi-lignes + reste partiel', () => {
  const buf = encodeFrame({ type: 'hello', version: 0, role: 'simulation', variables: ['a'] })
    + encodeFrame({ type: 'data', version: 0, seq: 1, values: {} })
    + '{"type":"data","vers';
  const r = decodeFrames(buf);
  assert.equal(r.error, null);
  assert.equal(r.messages.length, 2);
  assert.equal(r.remainder, '{"type":"data","vers');
});

test('framing : ligne > 1 MiB -> frameTooLarge', () => {
  const r = decodeFrames('x'.repeat(MAX_FRAME_BYTES + 1) + '\n');
  assert.equal(r.error, ERROR_CODES.FRAME_TOO_LARGE);
});

test('framing : JSON malformé -> badJson', () => {
  assert.equal(decodeFrames('{oops}\n').error, ERROR_CODES.BAD_JSON);
});

test('encodeFrame refuse > 1 MiB', () => {
  assert.throws(() => encodeFrame({ type: 'data', version: 0, seq: 1, values: { pad: 'x'.repeat(MAX_FRAME_BYTES) } }), /frameTooLarge/);
});
