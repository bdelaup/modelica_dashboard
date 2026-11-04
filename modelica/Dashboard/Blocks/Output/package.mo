within Dashboard.Blocks;
package Output "Blocs de sortie : la variable du modèle est poussée vers le dashboard"
  extends Modelica.Icons.VariantsPackage;
  annotation(
    Documentation(info = "<html>
<p>Chaque bloc pousse la valeur vers le dashboard manager (127.0.0.1:51234) à chaque pas ; sans dashboard manager connecté, l'appel est sans effet.</p>
</html>"));
end Output;
