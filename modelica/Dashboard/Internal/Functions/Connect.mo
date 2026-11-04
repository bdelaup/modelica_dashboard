within Dashboard.Internal.Functions;
function Connect "Connexion au viewer + handshake hello (liste des variables, dashboard en mode auto) — code 0 si OK"
  input String variablesCsv "Variables exposées au dashboard, séparées par des virgules (ex. \"motor.speed,motor.on\")";
  input String dashboardFile "Fichier .drawio poussé au pivot en mode auto (D11), vide si aucun";
  output Integer status "0 = connecté, négatif = erreur (détail dans le journal de simulation)";

  external "C" status = dashboard_client_connect(variablesCsv, dashboardFile) annotation(
    Include = "#include \"dashboard_client.h\"",
    Library = "DashboardExternal",
    IncludeDirectory = "modelica://Dashboard/Resources/Include",
    LibraryDirectory = "modelica://Dashboard/Resources/Library");
end Connect;
