within ;
package Dashboard
  "Bibliothèque de blocs d'interface dashboard pour OpenModelica (EF-B1..B4).
   Chaque bloc s'appuie sur la fonction C externe DashboardExternal.dll (D38)
   qui résout la variable par nom à l'exécution (D14, EF-I1)."
  annotation (Documentation(info="<html>
<p>Blocs <code>Dashboard.Input</code> / <code>Dashboard.Output</code> pour Real, Integer, Boolean.</p>
<ul>
<li><code>showPort</code> : case à cocher, masque/affiche le port (EF-B2, mécanisme à valider au POC, D39).</li>
<li><code>variableName</code> : nom de la variable du modèle associée au port (EF-B3).</li>
<li><code>defaultValue</code> : valeur utilisée si le dashboard ne fournit jamais la variable (EF-B4, D15).</li>
<li><code>dashboardFile</code> : chemin du .drawio poussé au pivot en début de simulation, mode auto (D11).</li>
</ul>
</html>"));
end Dashboard;
