within Dashboard.Internal;
package Functions "Fonctions externes C de DashboardExternal.dll (contrat : dashboard_client.h)"
  extends Modelica.Icons.FunctionsPackage;

  annotation(
    Documentation(info = "<html>
<p>Chaque fonction est <code>external \"C\"</code> vers l'export correspondant de DashboardExternal.dll (préfixe <code>dashboard_client_</code>). L'en-tête est copié dans <code>Resources/Include</code>, la DLL est installée dans <code>Resources/Library/&lt;plateforme&gt;</code> par le build (scripts/build.ps1).</p>
</html>"));
end Functions;
