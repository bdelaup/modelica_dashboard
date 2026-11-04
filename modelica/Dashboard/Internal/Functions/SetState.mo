within Dashboard.Internal.Functions;
function SetState "Signale un changement d'état de la simulation au viewer (running|paused|stopped|finished)"
  input String state "running|paused|stopped|finished (D40)";

  external "C" dashboard_client_state(state) annotation(
    Include = "#include \"dashboard_client.h\"",
    Library = "DashboardExternal",
    IncludeDirectory = "modelica://Dashboard/Resources/Include",
    LibraryDirectory = "modelica://Dashboard/Resources/Library");
end SetState;
