// Test d'intégration de bout en bout (ADR-0008) :
// vrai pivot (shared/) sur 127.0.0.1:51234 + vrai client C (modelica/csrc harness)
// qui joue la simulation : handshake, data -> tick, state, cmd set -> input (EF-B4).
#include "dashboard/pivot.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace dashboard;
using namespace std::chrono_literals;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++failures; } \
    else { std::printf("ok : %s\n", #cond); } } while (0)

int main(int argc, char** argv) {
    // argv[1] = chemin du harness (dashboard_harness)
    std::string harness = argc > 1 ? argv[1] : "./dashboard_harness";

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

    // Lance le harness (client sim) : handshake + 3 outputs + state running
#if defined(_WIN32)
    int rc = _spawnl(_P_WAIT, harness.c_str(), harness.c_str(), "motor.speed,motor.on,motor.gear", "", "500", nullptr);
#else
    // délai de 500 ms : le test envoie un cmd set pendant ce délai (EF-B4/D15)
    std::string cmd = harness + " \"motor.speed,motor.on,motor.gear\" \"\" 500 & sleep 0.2; wait";
    (void)cmd;
    pid_t pid = fork();
    if (pid == 0) {
        execl(harness.c_str(), harness.c_str(), "motor.speed,motor.on,motor.gear", "", "500", (char*)nullptr);
        _exit(127);
    }
#endif
    // attend la connexion puis envoie cmd set motor.throttle = 0.9 (viewer -> pivot -> sim)
    for (int i = 0; i < 200 && connected == 0; ++i) std::this_thread::sleep_for(10ms);
    CHECK(connected == 1);
    std::this_thread::sleep_for(200ms); // laisse le harness poser ses outputs et entrer en attente
    CHECK(pivot.sendCommand(makeCmd("set", json{{"variable", "motor.throttle"}, {"value", 0.9}})));
    std::this_thread::sleep_for(50ms);
#if !defined(_WIN32)
    int status = 0;
    waitpid(pid, &status, 0);
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    CHECK(rc == 0);

    // attend les 3 ticks et le state
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (ticks.size() >= 3 && states.size() >= 1) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        std::lock_guard<std::mutex> lk(mtx);
        CHECK(ticks.size() == 3);
        if (ticks.size() == 3) {
            // chaque output est un data batch ; le harness envoie 3 messages séparés
            bool foundSpeed = false, foundOn = false, foundGear = false;
            for (const auto& t : ticks) {
                if (t["values"].contains("motor.speed") && t["values"]["motor.speed"] == 12.5) foundSpeed = true;
                if (t["values"].contains("motor.on") && t["values"]["motor.on"] == true) foundOn = true;
                if (t["values"].contains("motor.gear") && t["values"]["motor.gear"] == 3) foundGear = true;
            }
            CHECK(foundSpeed);
            CHECK(foundOn);
            CHECK(foundGear);
            CHECK(ticks[0]["type"] == "tick");
            CHECK(ticks[0].contains("ratio"));
        }
        CHECK(states.size() >= 1 && states[0] == "running");
    }
    CHECK(connected == 1);
    for (int i = 0; i < 100 && disconnected == 0; ++i) std::this_thread::sleep_for(10ms);
    CHECK(disconnected == 1);

    // EF-B4 : commande set depuis le viewer -> appliquée côté sim (test du harness en deux temps)
    // Ici on vérifie juste que sendCommand fonctionne avec une sim connectée :
    // (le harness s'est déjà déconnecté, donc on attend false)
    CHECK(!pivot.sendCommand(makeCmd("run")));

    pivot.stop();
    if (failures == 0) { std::printf("INTEGRATION OK\n"); return 0; }
    std::printf("INTEGRATION ECHOUEE (%d)\n", failures);
    return 1;
}
