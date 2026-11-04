within Dashboard.Blocks;
partial block PartialInput "Interface commune des blocs Input : paramètres showPort / variableName / defaultValue / dashboardFile"
  parameter Boolean showPort = true "Masque ou affiche le port (EF-B2)";
  parameter String variableName = "" "Nom de la variable du modèle associée (EF-B3)";
  parameter String dashboardFile = "" "Fichier .drawio poussé au pivot en mode auto (D11)";
  annotation(
    Documentation(info = "<html>
<p>Paramètres communs aux blocs <code>Dashboard.Blocks.Input</code>.</p>
</html>"));
end PartialInput;
