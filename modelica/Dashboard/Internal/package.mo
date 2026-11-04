within Dashboard;
package Internal "Détails d'implémentation (fonctions externes C de DashboardExternal.dll) - usage interne"
  extends Modelica.Icons.InternalPackage;

  annotation(
    Documentation(info = "<html>
<p>Les fonctions externes C ne sont autorisées que dans des <code>function</code> Modelica (MLS §12.9) : les blocs de <code>Dashboard.Blocks</code> appellent ces fonctions, elles-mêmes déclarées <code>external \"C\"</code> vers DashboardExternal.dll (D38).</p>
</html>"));
end Internal;
