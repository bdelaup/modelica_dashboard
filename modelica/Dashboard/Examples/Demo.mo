within Dashboard.Examples;
model Demo "Modèle de démo pour le dashboard (tests d'intégration, doc utilisateur).
  Nécessite DashboardExternal.dll (D17) et le dashboard manager lancé sur 127.0.0.1:51234 AVANT la simulation."
  extends Modelica.Icons.Example;

  Real speed(start = 0) "vitesse (poussée au dashboard)";
  Boolean on(start = true) "marche (pilotée depuis le dashboard)";
  Real throttle "consigne (pilotée depuis le dashboard, défaut 0.25)";

  Dashboard.Blocks.Initialize connect(variablesCsv = "motor.speed,motor.on,motor.throttle",
    dashboardFile = Modelica.Utilities.Files.loadResource("modelica://Dashboard/Resources/Dashboards/Demo.dashboard.drawio")) annotation(
    Placement(transformation(extent = {{-60, 40}, {-40, 60}})));
  Dashboard.Blocks.Output.Real outSpeed(variableName = "motor.speed") annotation(
    Placement(transformation(extent = {{-10, -10}, {10, 10}})));
  Dashboard.Blocks.Output.Boolean outOn(variableName = "motor.on") annotation(
    Placement(transformation(extent = {{-10, -40}, {10, -20}})));
  Dashboard.Blocks.Input.Real inThrottle(variableName = "motor.throttle", defaultValue = 0.25) annotation(
    Placement(transformation(extent = {{-10, -70}, {10, -50}})));
  Dashboard.Blocks.Input.Boolean inOn(variableName = "motor.on", defaultValue = true) annotation(
    Placement(transformation(extent = {{-40, -70}, {-20, -50}})));

equation
  outSpeed.u = speed;
  outOn.u = on;
  throttle = inThrottle.y;
  on = inOn.y;
  der(speed) = if on then throttle else 0.0;

  annotation(experiment(StopTime = 10, Interval = 0.01));
end Demo;
