// I18n FR/EN, chaînes externalisées (D21/D36 : français par défaut).
export const LOCALES = ['fr', 'en'];
export const DEFAULT_LOCALE = 'fr';

export const STRINGS = {
  fr: {
    run: 'Run', runToEnd: "Run jusqu'à la fin", pause: 'Pause', stop: 'Stop',
    reload: 'Recharger', edit: 'Éditer', openDashboard: 'Ouvrir un dashboard…',
    simTime: 'Temps de simulation', ratio: 'Ratio temps réel',
    log: 'Journal', noSim: 'Aucune simulation connectée',
    simConnected: 'Simulation connectée', simDisconnected: 'Simulation déconnectée',
    unconnectedVar: (v) => `Variable non connectée : ${v}`,
    dashboardLoaded: (f) => `Dashboard chargé : ${f}`,
    error: (m) => `Erreur : ${m}`,
    state: { running: 'En cours', paused: 'Pause', stopped: 'Arrêtée', finished: 'Terminée' },
    lang: 'Langue',
  },
  en: {
    run: 'Run', runToEnd: 'Run to end', pause: 'Pause', stop: 'Stop',
    reload: 'Reload', edit: 'Edit', openDashboard: 'Open a dashboard…',
    simTime: 'Simulation time', ratio: 'Real-time ratio',
    log: 'Log', noSim: 'No simulation connected',
    simConnected: 'Simulation connected', simDisconnected: 'Simulation disconnected',
    unconnectedVar: (v) => `Unconnected variable: ${v}`,
    dashboardLoaded: (f) => `Dashboard loaded: ${f}`,
    error: (m) => `Error: ${m}`,
    state: { running: 'Running', paused: 'Paused', stopped: 'Stopped', finished: 'Finished' },
    lang: 'Language',
  },
};

export function createI18n(locale = DEFAULT_LOCALE) {
  let current = LOCALES.includes(locale) ? locale : DEFAULT_LOCALE;
  return {
    get locale() { return current; },
    setLocale(l) { if (LOCALES.includes(l)) current = l; },
    t(key, ...args) {
      const table = STRINGS[current] ?? STRINGS[DEFAULT_LOCALE];
      const v = table[key] ?? STRINGS[DEFAULT_LOCALE][key] ?? key;
      return typeof v === 'function' ? v(...args) : v;
    },
  };
}
