// Test d'intégration de bout en bout (ADR-0008) :
// vrai pivot (shared/) sur 127.0.0.1:51234 + client C (modelica/csrc) appelé
// IN-PROCESS dans un thread (pas de process externe -> robuste Windows/Linux) :
// handshake, data -> tick, state, cmd set -> input (EF-B4/D15).
extern "C" int harness_run(const char* variablesCsv, const char* dashFile, int delaiInputMs, double* outThrottle);

#include "dashboard/pivot.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace dashboard;
using namespace std::chrono_literals;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++failures; } \
    else { std::printf("ok : %s\n", #cond); } } while (0)

int main() {
    std::mutex mtx;
    std::vector<json> ticks;
    std::vector<std::string> states;
    std::atomic<int> connected{0}, disconnected{0};

    Pivot pivot;
    PivotEvents ev;
    ev.onTick = [&](const json& t) { std::lock_guard<std::mutex> lk(mtx); ticks.push_back(t); };
    ev.onSimState = [&](std::string_view s) { std::lock_guard<std::mutex> lk(mtx); states.emplace_back(s); };
    ev.onSimConnected = [&] { connected++; };
    ev.onSimDisconnected = [&] { disconnected++; };
    if (!pivot.start(std::move(ev))) { std::printf("ECHEC: pivot.start\n"); return 1; }
    for (int i = 0; i < 100 && !pivot.isListening(); ++i) std::this_thread::sleep_for(10ms);

    // Lance le client sim (harness) dans un thread du même process
    std::atomic<double> throttle{0.25};
    std::atomic<int> simRc{-1};
    std::thread simThread([&] {
        double v = 0.25;
        int r = harness_run("motor.speed,motor.on,motor.gear,motor.throttle", "", 500, &v);
        throttle = v;
        simRc = r;
        std::printf("harness rc=%d throttle=%g\n", r, v);
        std::fflush(stdout);
    });

    // attend la connexion puis envoie cmd set motor.throttle = 0.9 (viewer -> pivot -> sim)
    for (int i = 0; i < 200 && connected == 0; ++i) std::this_thread::sleep_for(10ms);
    CHECK(connected == 1);
    std::this_thread::sleep_for(200ms);
    CHECK(pivot.sendCommand(makeCmd("set", json{{"variable", "motor.throttle"}, {"value", 0.9}})));

    simThread.join();
    std::printf("après join: simRc=%d\n", simRc.load());

    // attend les 3 ticks et le state
    {
        std::lock_guard<std::mutex> lk(mtx);
        CHECK(ticks.size() == 3);
        bool foundSpeed = false, foundOn = false, foundGear = false;
        for (const auto& t : ticks) {
            if (t["values"].contains("motor.speed") && t["values"]["motor.speed"] == 12.5) foundSpeed = true;
            if (t["values"].contains("motor.on") && t["values"]["motor.on"] == true) foundOn = true;
            if (t["values"].contains("motor.gear") && t["values"]["motor.gear"] == 3) foundGear = true;
        }
        CHECK(foundSpeed);
        CHECK(foundOn);
        CHECK(foundGear);
        if (!ticks.empty()) {
            CHECK(ticks[0]["type"] == "tick");
            CHECK(ticks[0].contains("ratio"));
        }
        CHECK(states.size() >= 1);
        if (!states.empty()) CHECK(states[0] == "running");
    }
    // EF-B4/D15 : le set a été appliqué côté sim (0.9 et non le défaut 0.25)
    CHECK(throttle == 0.9);
    for (int i = 0; i < 100 && disconnected == 0; ++i) std::this_thread::sleep_for(10ms);
    CHECK(disconnected == 1);
    CHECK(!pivot.sendCommand(makeCmd("run")));

    pivot.stop();
    if (failures == 0) { std::printf("INTEGRATION OK\n"); return 0; }
    std::printf("INTEGRATION ECHOUEE (%d)\n", failures);
    return 1;
}
