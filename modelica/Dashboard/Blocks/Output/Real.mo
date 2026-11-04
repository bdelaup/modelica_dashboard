within Dashboard.Blocks.Output;
block Real "La variable Real du modèle est poussée vers le dashboard"
  extends Dashboard.Blocks.PartialOutput;
  Modelica.Blocks.Interfaces.RealInput u "Entrée du bloc = variable du modèle à afficher";

equation
  Dashboard.Internal.Functions.OutputReal(variableName, u, dashboardFile);

  annotation(
    Documentation(info = "<html>
<p>Pousse <code>u</code> vers le dashboard à chaque pas de solveur ; sans dashboard manager connecté, l'appel est sans effet.</p>
</html>"));
end Real;
