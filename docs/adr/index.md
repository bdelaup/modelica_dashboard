# ADR — Architecture Decision Records

Une fiche par décision tranchée. Les décisions de mise en œuvre de la session de cadrage dev figurent dans l'[ADR-0013](0013-decisions-mise-en-oeuvre.md).

| ADR | Sujet |
|---|---|
| [0001](0001-architecture-a4b.md) | Architecture A4b retenue, Windows-only V1, pivot embarqué |
| [0002](0002-pivot-embarque-port-fixe.md) | Port fixe par défaut, lancement viewer/sim, bouton Recharger, une sim par viewer |
| [0003](0003-canal-sockets-loopback.md) | Canal : sockets TCP 127.0.0.1, framing JSON (repli binaire) |
| [0004](0004-editeur-drawio.md) | Éditeur draw.io self-hosté embed, format `.drawio` natif, rendu R3 hybride, C2 |
| [0005](0005-webview2-app-embarque.md) | WebView2 charge l'IHM via ressources embarquées + protocole custom `app://` |
| [0006](0006-binding-runtime.md) | Binding par nom via l'API runtime ; valeur par défaut hybride |
| [0007](0007-livraison.md) | Livraison : exe autonome unique (L2) + package Modelica standard |
| [0008](0008-ci-tests.md) | CI GitHub Actions (job Windows) + tests unitaires/intégration/contrat |
| [0009](0009-versioning.md) | Versioning : SemVer + champ version dans le fichier et le protocole, lecture N-1 |
| [0010](0010-i18n-logs.md) | I18n FR+EN dès la V1 ; logs fichier par composant + journal UI |
| [0011](0011-securite.md) | Sécurité : loopback + validation de taille max uniquement (risque accepté) |
| [0012](0012-perf.md) | Performance : cible 100–1000 var × 10–100 Hz, backpressure « pivot pousse / viewer écrase » |
| [0013](0013-decisions-mise-en-oeuvre.md) | Décisions de mise en œuvre (port 51234, OM v1.27.1, draw.io v32.3.0, éditeur iframe WebView2, stack C++20/Catch2/nlohmann, widgets V1, versions v0, POC parallèle, ordre de dev) |
