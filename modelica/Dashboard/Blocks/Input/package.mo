within Dashboard.Blocks;
package Input "Blocs d'entrée : le dashboard pilote la variable du modèle (actionneurs)"
  extends Modelica.Icons.VariantsPackage;
  annotation(
    Documentation(info = "<html>
<p>Chaque bloc interroge DashboardExternal.dll à chaque pas ; sans dashboard manager connecté, la <code>defaultValue</code> est retournée (EF-B4, D15).</p>
</html>"));
end Input;
