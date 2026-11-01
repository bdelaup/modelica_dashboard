# ADR-0005 — WebView2 charge l'IHM via ressources embarquées + protocole custom app://

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) D9, §4.1

## Contexte

Le binaire viewer embarque un WebView2 (Windows-only, ADR-0001). Trois façons de servir l'IHM : ressources embarquées + schéma custom, mini-serveur HTTP 127.0.0.1, file:// depuis %TEMP%.

## Décision

- **Ressources embarquées + protocole custom `app://`** : assets HTML/JS/CSS compilés dans l'exe, servis via un gestionnaire de schéma enregistré dans WebView2 ; échanges C++ ↔ JS via le pont natif (`PostWebMessageAsString` / `WebMessageReceived`).
- Flag `--dev` au lancement pour ouvrir le `remote-debugging-port` (débogage devtools).

## Conséquences

- **Aucun port HTTP ouvert pour l'UI** : le seul port ouvert est celui du canal (ENF-2 renforcé). Pas de vérification Host/Origin nécessaire, pas de DNS rebinding.
- Le WebView ne peut naviguer que vers les ressources embarquées — pas de navigation externe.
- Un seul exe autonome, rien d'écrit sur disque (L2 pur, EF-E2).
- Débogage moins direct qu'un serveur HTTP, compensé par le flag `--dev` et les messages du pont.
- file:// écarté (WebSocket/fetch restreints sous WebView2).
