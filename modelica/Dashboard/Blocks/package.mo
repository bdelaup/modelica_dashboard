within Dashboard;
package Blocks "Blocs d'entrée/sortie dashboard (EF-B1) : les modèles échangent avec le dashboard manager via DashboardExternal.dll"
  extends Modelica.Icons.VariantsPackage;

  annotation(
    Documentation(info = "<html>
<ul>
<li><code>Input</code> : le dashboard pilote une variable du modèle (actionneurs).</li>
<li><code>Output</code> : la variable du modèle est poussée vers le dashboard.</li>
</ul>
</html>"));
end Blocks;
