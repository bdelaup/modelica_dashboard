within Dashboard.Blocks.Input;
block Integer "Le dashboard pilote une variable Integer du modèle"
  extends Dashboard.Blocks.PartialInput;
  parameter Integer defaultValue = 0 "Valeur si jamais de valeur reçue du dashboard (EF-B4)";
  Modelica.Blocks.Interfaces.IntegerOutput y "Sortie du bloc = valeur appliquée au modèle";

equation
  y = Dashboard.Internal.Functions.InputInteger(variableName, defaultValue, dashboardFile);

  annotation(
    Documentation(info = "<html>
<p>Interroge le dashboard à chaque pas de solveur ; retourne <code>defaultValue</code> si le dashboard n'a jamais fourni la variable (EF-B4, D15).</p>
</html>"));
end Integer;
