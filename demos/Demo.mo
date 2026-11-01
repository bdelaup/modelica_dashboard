model Demo
  "Modèle de démo pour le dashboard (tests d'intégration, doc utilisateur).
   Utilise Dashboard.Blocks.Output/Input — nécessite le package Dashboard dans
   les libraries OMEdit + DashboardExternal.dll (D17)."
  Real speed(start=0) "vitesse (poussée au dashboard)";
  Boolean on(start=true) "marche (pilotée depuis le dashboard)";
  Real throttle "consigne (pilotée depuis le dashboard, défaut 0.25)";
  parameter Real stopTime = 10.0;

  Dashboard.Blocks.Output.Real outSpeed(variableName="motor.speed", dashboardFile="Demo.dashboard.drawio")
    annotation (Placement(transformation(extent={{-10,-10},{10,10}})));
  Dashboard.Blocks.Output.Boolean outOn(variableName="motor.on", dashboardFile="Demo.dashboard.drawio");
  Dashboard.Blocks.Input.Real inThrottle(variableName="motor.throttle", defaultValue=0.25, dashboardFile="Demo.dashboard.drawio");

equation
  outSpeed.u = speed;
  outOn.u = on;
  throttle = inThrottle.y;
  der(speed) = if on then throttle else 0.0;

  annotation (experiment(StopTime=10, Interval=0.01));
end Demo;
