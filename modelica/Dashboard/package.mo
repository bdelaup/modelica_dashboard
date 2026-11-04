within ;
package Dashboard "Bibliothèque de blocs d'interface dashboard pour OpenModelica (EF-B1..B4).
  Chaque bloc s'appuie sur la DLL externe DashboardExternal.dll (D38) dont les
  fonctions C (modelica/csrc/dashboard_client.c, contrat dans dashboard_client.h)
  résolvent la variable par nom à l'exécution (D14, EF-I1)."
  extends Modelica.Icons.Package;

  annotation(
    Documentation(info = "<html>
<p>Blocs <code>Dashboard.Blocks.Input</code> / <code>Dashboard.Blocks.Output</code> pour Real, Integer, Boolean.</p>
<ul>
<li><code>showPort</code> : case à cocher, masque/affiche le port (EF-B2, mécanisme à valider au POC, D39).</li>
<li><code>variableName</code> : nom de la variable du modèle associée au port (EF-B3).</li>
<li><code>defaultValue</code> : valeur utilisée si le dashboard ne fournit jamais la variable (EF-B4, D15).</li>
<li><code>dashboardFile</code> : chemin du .drawio poussé au pivot en début de simulation, mode auto (D11).</li>
</ul>
</html>"),
    uses(Modelica(version = "4.1.0")));
end Dashboard;
