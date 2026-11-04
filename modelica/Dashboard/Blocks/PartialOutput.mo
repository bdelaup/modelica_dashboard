within Dashboard.Blocks;
partial block PartialOutput "Interface commune des blocs Output : paramètres showPort / variableName / dashboardFile"
  parameter Boolean showPort = true "Masque ou affiche le port (EF-B2)";
  parameter String variableName = "" "Nom de la variable du modèle associée (EF-B3)";
  parameter String dashboardFile = "" "Fichier .drawio poussé au pivot en mode auto (D11)";
  annotation(
    Documentation(info = "<html>
<p>Paramètres communs aux blocs <code>Dashboard.Blocks.Output</code>.</p>
</html>"));
end PartialOutput;
