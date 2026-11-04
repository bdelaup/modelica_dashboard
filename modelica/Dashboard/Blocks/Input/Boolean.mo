within Dashboard.Blocks.Input;
block Boolean "Le dashboard pilote une variable Boolean du modèle"
  extends Dashboard.Blocks.PartialInput;
  parameter Boolean defaultValue = false "Valeur si jamais de valeur reçue du dashboard (EF-B4)";
  Modelica.Blocks.Interfaces.BooleanOutput y "Sortie du bloc = valeur appliquée au modèle";

equation
  y = Dashboard.Internal.Functions.InputBoolean(variableName, defaultValue, dashboardFile);

  annotation(
    Documentation(info = "<html>
<p>Interroge le dashboard à chaque pas de solveur ; retourne <code>defaultValue</code> si le dashboard n'a jamais fourni la variable (EF-B4, D15).</p>
</html>"));
end Boolean;
