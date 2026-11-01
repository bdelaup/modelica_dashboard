// Contrat du canal, côté logique JS du viewer (docs/protocole.md, version 0).
// Aucune dépendance WebView2 : le pont natif est injecté (testable en Node, D19/D33).

export const PROTOCOL_VERSION = 0;
export const MAX_FRAME_BYTES = 1 << 20; // 1 MiB (D35)
export const LOOPBACK_HOST = '127.0.0.1'; // informatif côté JS

export const MSG = {
  HELLO: 'hello', HELLO_ACK: 'helloAck', DATA: 'data', STATE: 'state',
  TICK: 'tick', UNCONNECTED: 'unconnected', DASHBOARD: 'dashboard',
  SIM_STATE: 'simState', CMD: 'cmd', ACK: 'ack', ERROR: 'error',
};

export const ERROR_CODES = {
  BAD_JSON: 'badJson', FRAME_TOO_LARGE: 'frameTooLarge', BAD_VERSION: 'badVersion',
  BUSY: 'busy', BAD_ROLE: 'badRole', UNKNOWN_TYPE: 'unknownType', BAD_CMD: 'badCmd',
};

const STATES = ['running', 'paused', 'stopped', 'finished'];
const CMDS = ['run', 'pause', 'runToEnd', 'stop', 'set'];

// ADR-0009 : un récepteur de version N lit N et N-1.
export function versionSupported(v) {
  return v === PROTOCOL_VERSION || v === PROTOCOL_VERSION - 1;
}

// Retourne true si valide, sinon { code }.
export function validateMessage(msg) {
  if (msg === null || typeof msg !== 'object' || Array.isArray(msg)) return { code: ERROR_CODES.BAD_JSON };
  if (typeof msg.type !== 'string') return { code: ERROR_CODES.BAD_JSON };
  if (!Number.isInteger(msg.version)) return { code: ERROR_CODES.BAD_VERSION };
  if (!versionSupported(msg.version)) return { code: ERROR_CODES.BAD_VERSION };
  switch (msg.type) {
    case MSG.HELLO:
      if (msg.role !== 'simulation') return { code: ERROR_CODES.BAD_ROLE };
      if (!Array.isArray(msg.variables)) return { code: ERROR_CODES.BAD_JSON };
      return true;
    case MSG.DATA:
      if (!('seq' in msg) || msg.values === null || typeof msg.values !== 'object') return { code: ERROR_CODES.BAD_JSON };
      return true;
    case MSG.STATE:
    case MSG.SIM_STATE:
      if (!STATES.includes(msg.state)) return { code: ERROR_CODES.BAD_JSON };
      return true;
    case MSG.CMD:
      if (!CMDS.includes(msg.cmd)) return { code: ERROR_CODES.BAD_CMD };
      return true;
    case MSG.ERROR:
      if (typeof msg.code !== 'string') return { code: ERROR_CODES.BAD_JSON };
      return true;
    case MSG.HELLO_ACK: case MSG.TICK: case MSG.UNCONNECTED:
    case MSG.DASHBOARD: case MSG.ACK:
      return true;
    default:
      return { code: ERROR_CODES.UNKNOWN_TYPE };
  }
}

export function makeCmd(cmd, extra = {}) {
  return { type: MSG.CMD, version: PROTOCOL_VERSION, cmd, ...extra };
}

// Encode un message en ligne NDJSON (lève si > MAX_FRAME_BYTES).
export function encodeFrame(msg) {
  const line = JSON.stringify(msg) + '\n';
  if (line.length > MAX_FRAME_BYTES) throw new Error(ERROR_CODES.FRAME_TOO_LARGE);
  return line;
}

// Découpe un buffer en messages ; retourne { messages, remainder, error }.
export function decodeFrames(buffer) {
  const messages = [];
  let start = 0;
  for (let i = 0; i < buffer.length; i++) {
    if (buffer.charCodeAt(i) === 10 /* \n */) {
      const line = buffer.slice(start, i);
      start = i + 1;
      if (line.length === 0) continue;
      if (line.length > MAX_FRAME_BYTES) return { messages, remainder: buffer.slice(start), error: ERROR_CODES.FRAME_TOO_LARGE };
      try {
        messages.push(JSON.parse(line));
      } catch {
        return { messages, remainder: buffer.slice(start), error: ERROR_CODES.BAD_JSON };
      }
    }
  }
  return { messages, remainder: buffer.slice(start), error: null };
}
