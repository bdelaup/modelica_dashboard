within Dashboard.Blocks;
block Initialize "Connecte la simulation au viewer : hello (variables exposées) + état running + dashboard en mode auto (D11/D26). À placer une fois dans le modèle, appelé lors de l'initialisation"
  parameter String variablesCsv = "" "Variables exposées au dashboard, séparées par des virgules (ex. \"motor.speed,motor.on\")";
  parameter String dashboardFile = "" "Fichier .drawio poussé au viewer en mode auto (D11), vide si aucun";
  Integer status "0 = connecté au viewer, négatif = erreur (voir le journal de simulation : viewer absent ?)";

equation
  when initial() then
    status := Dashboard.Internal.Functions.Connect(variablesCsv, dashboardFile);
    Dashboard.Internal.Functions.SetState("running");
  end when;

  annotation(
    Documentation(info = "<html>
<p>À placer <b>une fois</b> dans le modèle. À l'initialisation : connexion au pivot du viewer (127.0.0.1:51234), envoi du hello avec la liste <code>variablesCsv</code>, poussée du <code>dashboardFile</code> en mode auto (D11) et passage à l'état running.</p>
<p>Sans viewer lancé, la simulation continue en mode dégradé (EF-B4/D15 : defaultValue des Input, Output perdus) ; le journal de simulation contient les lignes <code>[dashboard ...]</code> expliquant la panne. <code>status</code> vaut 0 si connecté, négatif sinon.</p>
</html>"));
end Initialize;
