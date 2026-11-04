within Dashboard.Internal.Functions;
function OutputReal "Pousse la valeur Real d'une variable du modèle vers le dashboard"
  input String variableName "Nom de la variable du modèle associée (EF-B3)";
  input Real u "Valeur de la variable du modèle à afficher";
  input String dashboardFile "Fichier .drawio poussé au pivot en mode auto (D11)";

  external "C" dashboard_client_output_real(variableName, u, dashboardFile) annotation(
    Include = "#include \"dashboard_client.h\"",
    Library = "DashboardExternal",
    IncludeDirectory = "modelica://Dashboard/Resources/Include",
    LibraryDirectory = "modelica://Dashboard/Resources/Library");
end OutputReal;
