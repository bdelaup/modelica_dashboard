# ADR-0007 — Livraison : exe autonome unique (L2) + package Modelica standard

**Statut** : acté
**Références** : [options techniques](../option_technique.md) §5/§10, [spec technique](../spec_technique.md) D16/D17, §11

## Contexte

Forme du livrable (axe L) : script interprété (L1), exe compilé autonome (L2), hybride (L3). Distribution de la bibliothèque Modelica : package standard vs installeur vs enregistrement automatique.

## Décision

- **Viewer + pivot** : **un seul exe autonome**, C++/Win32 + WebView2, IHM (`app://`) et draw.io embarqués (L2). Zéro installation (EF-E2). Publié en release GitHub (artifact CI sur tag `v*.*.*`).
- **Bibliothèque Modelica** : **package standard** (`Dashboard/` + DLL C externe) à copier dans les libraries OMEdit, chemin de la DLL déclaré dans la configuration OMEdit. Aucun installeur.
- **Éditeur** : embarqué dans l'exe (build statique draw.io servi en local) — rien à installer séparément.

## Conséquences

- Un seul fichier `.exe` à distribuer côté affichage ; procédure manuelle documentée côté bibliothèque.
- PyInstaller/Python embarqué écarté : exe lourd (50–100 Mo+), démarrage lent, intégration WebView2 moins native.
- L'exe + dossier d'assets à côté écarté : permet de patcher l'UI sans recompiler, mais deux éléments à distribuer et risque de désynchro.
- L'enregistrement automatique de la config OMEdit par le viewer est écarté (fragile si OMEdit change sa configuration).
