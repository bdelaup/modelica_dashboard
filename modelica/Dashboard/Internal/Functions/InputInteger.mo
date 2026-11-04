within Dashboard.Internal.Functions;
function InputInteger "Dernière valeur Integer reçue du dashboard pour la variable, ou defaultValue (EF-B4, D15)"
  input String variableName "Nom de la variable du modèle associée (EF-B3)";
  input Integer defaultValue "Valeur si jamais de valeur reçue du dashboard (EF-B4)";
  input String dashboardFile "Fichier .drawio poussé au pivot en mode auto (D11)";
  output Integer y "Valeur appliquée au modèle";

  external "C" y = dashboard_client_input_integer(variableName, defaultValue, dashboardFile) annotation(
    Include = "#include \"dashboard_client.h\"",
    Library = "DashboardExternal",
    IncludeDirectory = "modelica://Dashboard/Resources/Include",
    LibraryDirectory = "modelica://Dashboard/Resources/Library");
end InputInteger;
