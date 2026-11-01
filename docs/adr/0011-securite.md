# ADR-0011 — Sécurité : loopback + validation de taille max uniquement (risque accepté, à réévaluer)

**Statut** : acté avec risque accepté documenté
**Références** : [spec technique](../spec_technique.md) D23, §10 ; ENF-2

## Contexte

ENF-2 impose le loopback strict. Au-delà, trois niveaux de défense étaient possibles : (1) CSP stricte + validation sémantique des trames + pas d'innerHTML sur le XML, (2) CSP seule, (3) loopback + taille max seulement.

## Décision

- **Niveau retenu en V1 : (3) + protections structurelles** — loopback strict (seul port : le canal ; draw.io en bind 127.0.0.1 ; IHM en `app://` sans port), validation de **taille maximale** par trame, et **jamais d'injection du XML via `innerHTML`** (rendu via le moteur draw.io ou parser propre — cette dernière mesure est structurelle et conservée quel que soit le niveau).
- **Risque accepté** : aucune validation sémantique des trames (pas de vérification du type enum, du jeu de caractères des noms, ni d'authentification de la connexion) — tout processus local peut envoyer des données forgées sur le port fixe.

## Conséquences

- Acceptable en V1 : la menace se limite aux processus de confiance sur le poste Windows mono-utilisateur.
- **Point de vigilance / à réévaluer** si le périmètre change : multi-utilisateur, port configurable, ouverture au-delà du loopback, ou distribution à des postes non maîtrisés. Dans ce cas, passer au niveau (1) : validation sémantique complète des trames + éventuellement un token partagé au handshake + CSP `script-src 'self'` explicite.
