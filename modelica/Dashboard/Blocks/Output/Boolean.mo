within Dashboard.Blocks.Output;
block Boolean "La variable Boolean du modèle est poussée vers le dashboard"
  extends Dashboard.Blocks.PartialOutput;
  Modelica.Blocks.Interfaces.BooleanInput u "Entrée du bloc = variable du modèle à afficher";

equation
  Dashboard.Internal.Functions.OutputBoolean(variableName, u, dashboardFile);

  annotation(
    Documentation(info = "<html>
<p>Pousse <code>u</code> vers le dashboard à chaque pas de solveur ; sans dashboard manager connecté, l'appel est sans effet.</p>
</html>"));
end Boolean;
