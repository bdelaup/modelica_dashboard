// Parsing du fichier .drawio côté logique JS du viewer (miroir de shared/, spec §6).
// DOMParser en navigateur / mini-parseur en Node (pas de dépendance externe).

export const DASHBOARD_VERSION = 0; // D31
export const WIDGET_TYPES = ['value', 'gauge', 'curve', 'button', 'slider', 'log']; // D32

// Parse un contenu XML .drawio. Retourne { valid, dashboardVersion, name, widgets, error }.
// En Node (tests), on utilise un parseur XML minimal embarqué ci-dessous.
export function parseDashboardFile(xml, doc = null) {
  const d = doc ?? parseXmlDoc(xml);
  if (!d || d.name !== 'mxfile') return { valid: false, error: "XML invalide : racine <mxfile> attendue", widgets: [] };
  const versionAttr = d.attrs.get('dashboardVersion');
  const dashboardVersion = versionAttr === undefined ? DASHBOARD_VERSION : parseInt(versionAttr, 10);
  if (Number.isNaN(dashboardVersion)) return { valid: false, error: 'dashboardVersion non entier', widgets: [] };
  if (dashboardVersion > DASHBOARD_VERSION || dashboardVersion < DASHBOARD_VERSION - 1) {
    return { valid: false, error: `dashboardVersion ${dashboardVersion} non supportée (attendu <= ${DASHBOARD_VERSION})`, widgets: [] };
  }
  const diagram = d.children.find((c) => c.name === 'diagram');
  if (!diagram) return { valid: false, error: '<diagram> absent', widgets: [] };
  const model = diagram.children.find((c) => c.name === 'mxGraphModel');
  const root = model && model.children.find((c) => c.name === 'root');
  if (!root) return { valid: false, error: 'mxGraphModel/root absent', widgets: [] };
  const widgets = [];
  for (const cell of root.children) {
    if (cell.name !== 'mxCell') continue;
    const type = cell.attrs.get('type');
    const hasUserObject = cell.attrs.has('UserObject');
    if (!type && !hasUserObject) continue;
    const w = {
      id: cell.attrs.get('id') ?? '',
      type: type ?? '',
      variable: cell.attrs.get('variable') ?? '',
      label: cell.attrs.get('label') ?? '',
      x: 0, y: 0, width: 0, height: 0,
    };
    const geo = cell.children.find((c) => c.name === 'mxGeometry');
    if (geo) {
      w.x = parseFloat(geo.attrs.get('x') ?? '0') || 0;
      w.y = parseFloat(geo.attrs.get('y') ?? '0') || 0;
      w.width = parseFloat(geo.attrs.get('width') ?? '0') || 0;
      w.height = parseFloat(geo.attrs.get('height') ?? '0') || 0;
    }
    if (w.type && !WIDGET_TYPES.includes(w.type)) {
      return { valid: false, error: `type de widget inconnu : ${w.type} (widget ${w.id})`, widgets: [] };
    }
    widgets.push(w);
  }
  return { valid: true, dashboardVersion, name: diagram.attrs.get('name') ?? '', widgets, error: null };
}

// Noms du dashboard absents du modèle -> message "unconnected" (EF-V6).
export function unconnectedVariables(modelVariables, dash) {
  return dash.widgets.map((w) => w.variable).filter((v) => v && !modelVariables.includes(v));
}

// --- Mini parseur XML (Node, sans dépendance) : éléments, attributs, texte ---
function parseXmlDoc(xml) {
  let i = 0;
  const s = xml;
  const skipWs = () => { while (i < s.length && /\s/.test(s[i])) i++; };
  function parseNode() {
    skipWs();
    if (!s.startsWith('<', i)) return null;
    if (s.startsWith('<?', i)) { const e = s.indexOf('?>', i); i = e + 2; return parseNode(); }
    if (s.startsWith('<!--', i)) { const e = s.indexOf('-->', i); i = e + 3; return parseNode(); }
    if (s.startsWith('</', i) || s.startsWith('<!', i)) return null;
    i++;
    const m = /^[^\s/>]+/.exec(s.slice(i));
    if (!m) return null;
    const node = { name: m[0], attrs: new Map(), children: [], text: '' };
    i += m[0].length;
    for (;;) {
      skipWs();
      if (s.startsWith('/>', i)) { i += 2; return node; }
      if (s.startsWith('>', i)) { i++; break; }
      const am = /^([^\s=/>]+)\s*=\s*"([^"]*)"/.exec(s.slice(i));
      if (!am) return null;
      node.attrs.set(am[1], decodeEntities(am[2]));
      i += am[0].length;
    }
    let text = '';
    while (i < s.length) {
      if (s.startsWith('</', i)) {
        const e = s.indexOf('>', i);
        const close = s.slice(i + 2, e).trim();
        if (close !== node.name) return null;
        i = e + 1;
        node.text = decodeEntities(text.trim());
        return node;
      }
      if (s.startsWith('<!--', i)) { i = s.indexOf('-->', i) + 3; continue; }
      if (s.startsWith('<', i)) { const c = parseNode(); if (!c) return null; node.children.push(c); continue; }
      text += s[i]; i++;
    }
    return null;
  }
  return parseNode();
}

function decodeEntities(str) {
  return str.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"')
    .replace(/&apos;/g, "'").replace(/&amp;/g, '&');
}
