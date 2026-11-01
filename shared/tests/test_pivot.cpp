// Tests du pivot : vrai serveur loopback 127.0.0.1:51234 + client socket de test
// jouant le rôle de la simulation (handshake, data, state, commandes, D25).
#include "catch_amalgamated.hpp"
#include "dashboard/pivot.hpp"
#include "dashboard/dashboard_file.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInv = INVALID_SOCKET;
static void closeS(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInv = -1;
static void closeS(socket_t s) { ::close(s); }
#endif

using namespace dashboard;
using namespace std::chrono_literals;

namespace {

socket_t connectToPivot() {
    socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    REQUIRE(s != kInv);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    for (int i = 0; i < 50; ++i) {
        if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) return s;
        std::this_thread::sleep_for(20ms);
    }
    closeS(s);
    return kInv;
}

void sendAll(socket_t s, const std::string& data) {
    REQUIRE(send(s, data.data(), static_cast<int>(data.size()), 0) == static_cast<int>(data.size()));
}

// Lit des lignes NDJSON jusqu'à obtenir n messages ou timeout.
std::vector<json> readMessages(socket_t s, std::size_t n, std::string& pending, int timeoutMs = 2000) {
    std::vector<json> out;
    char buf[8192];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (out.size() < n && std::chrono::steady_clock::now() < deadline) {
        int r = recv(s, buf, sizeof(buf), 0);
        if (r <= 0) break;
        pending.append(buf, static_cast<std::size_t>(r));
        auto res = decodeFrames(pending);
        pending = std::move(res.remainder);
        for (auto& m : res.messages) out.push_back(std::move(m));
    }
    return out;
}

struct Fixture {
    Pivot pivot;
    std::mutex mtx;
    std::vector<json> ticks;
    std::vector<std::string> simStates;
    std::vector<std::string> unconnected;
    std::vector<json> dashboards;
    std::atomic<int> connectedCount{0};
    std::atomic<int> disconnectedCount{0};
    std::atomic<int> errors{0};

    Fixture() {
        PivotEvents ev;
        ev.onTick = [this](const json& t) { std::lock_guard<std::mutex> lk(mtx); ticks.push_back(t); };
        ev.onSimState = [this](std::string_view s) { std::lock_guard<std::mutex> lk(mtx); simStates.emplace_back(s); };
        ev.onUnconnected = [this](const std::vector<std::string>& v) { std::lock_guard<std::mutex> lk(mtx); unconnected = v; };
        ev.onDashboard = [this](const json& d) { std::lock_guard<std::mutex> lk(mtx); dashboards.push_back(d); };
        ev.onSimConnected = [this] { connectedCount++; };
        ev.onSimDisconnected = [this] { disconnectedCount++; };
        ev.onProtocolError = [this](const json&) { errors++; };
        REQUIRE(pivot.start(std::move(ev)));
        // attend que le port écoute
        for (int i = 0; i < 100 && !pivot.isListening(); ++i) std::this_thread::sleep_for(10ms);
        REQUIRE(pivot.isListening());
    }
    ~Fixture() { pivot.stop(); }
};

} // namespace

TEST_CASE("handshake + data -> tick avec ratio (EF-V5)", "[pivot]") {
    Fixture f;
    socket_t sim = connectToPivot();
    REQUIRE(sim != kInv);
    sendAll(sim, encodeFrame(makeHello({"motor.speed", "motor.on"}, "")));
    std::string pending;
    auto msgs = readMessages(sim, 1, pending);
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0]["type"] == "helloAck");
    REQUIRE(f.connectedCount == 1);

    sendAll(sim, encodeFrame(makeData(1, 0.01, json{{"motor.speed", 0.0}})));
    sendAll(sim, encodeFrame(makeData(2, 0.02, json{{"motor.speed", 12.5}})));
    for (int i = 0; i < 100 && f.ticks.size() < 2; ++i) std::this_thread::sleep_for(10ms);
    REQUIRE(f.ticks.size() == 2);
    REQUIRE(f.ticks[0]["type"] == "tick");
    REQUIRE(f.ticks[0]["simTime"] == 0.01);
    REQUIRE(f.ticks[0]["values"]["motor.speed"] == 0.0);
    REQUIRE(f.ticks[1]["values"]["motor.speed"] == 12.5);
    REQUIRE(f.ticks[0].contains("ratio"));
    closeS(sim);
    for (int i = 0; i < 100 && f.disconnectedCount == 0; ++i) std::this_thread::sleep_for(10ms);
    REQUIRE(f.disconnectedCount == 1);
}

TEST_CASE("state sim -> simState (D40) et commande set relayée à la sim", "[pivot]") {
    Fixture f;
    socket_t sim = connectToPivot();
    REQUIRE(sim != kInv);
    sendAll(sim, encodeFrame(makeHello({"a"})));
    std::string pending;
    readMessages(sim, 1, pending);

    sendAll(sim, encodeFrame(makeState("running")));
    sendAll(sim, encodeFrame(makeState("paused")));
    for (int i = 0; i < 100 && f.simStates.size() < 2; ++i) std::this_thread::sleep_for(10ms);
    REQUIRE(f.simStates == std::vector<std::string>{"running", "paused"});

    // viewer -> pivot -> sim : cmd set
    REQUIRE(f.pivot.sendCommand(makeCmd("set", json{{"variable", "a"}, {"value", 2.0}})));
    REQUIRE(f.pivot.sendCommand(makeCmd("run")));
    auto cmds = readMessages(sim, 2, pending);
    REQUIRE(cmds.size() == 2);
    REQUIRE(cmds[0]["cmd"] == "set");
    REQUIRE(cmds[0]["value"] == 2.0);
    REQUIRE(cmds[1]["cmd"] == "run");
    closeS(sim);
}

TEST_CASE("D25 : une 2e sim est refusée (busy)", "[pivot]") {
    Fixture f;
    socket_t sim1 = connectToPivot();
    REQUIRE(sim1 != kInv);
    sendAll(sim1, encodeFrame(makeHello({"a"})));
    std::string pending;
    readMessages(sim1, 1, pending);
    REQUIRE(f.pivot.simConnected());

    socket_t sim2 = connectToPivot();
    REQUIRE(sim2 != kInv);
    auto msgs = readMessages(sim2, 1, pending);
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0]["type"] == "error");
    REQUIRE(msgs[0]["code"] == "busy");
    closeS(sim2);
    closeS(sim1);
}

TEST_CASE("badVersion au handshake -> error + fermeture", "[pivot]") {
    Fixture f;
    socket_t sim = connectToPivot();
    REQUIRE(sim != kInv);
    json bad = makeHello({"a"});
    bad["version"] = 99;
    sendAll(sim, encodeFrame(bad));
    std::string pending;
    auto msgs = readMessages(sim, 1, pending);
    REQUIRE(msgs.size() == 1);
    REQUIRE(msgs[0]["code"] == "badVersion");
    closeS(sim);
}

TEST_CASE("mode auto : dashboardFile poussé -> dashboard + unconnected (EF-V6)", "[pivot]") {
    // fichier dashboard de test
    DashboardFile dash;
    dash.dashboardVersion = 0;
    dash.name = "T";
    dash.widgets.push_back({"w1", "gauge", "motor.speed", "V", 0, 0, 100, 60});
    dash.widgets.push_back({"w2", "button", "ghost.var", "G", 0, 80, 100, 40});
    std::string path = "/tmp/pivot_test_dashboard.drawio";
    {
        std::ofstream o(path);
        o << serializeDashboardFile(dash);
    }

    Fixture f;
    socket_t sim = connectToPivot();
    REQUIRE(sim != kInv);
    sendAll(sim, encodeFrame(makeHello({"motor.speed"}, path)));
    std::string pending;
    auto msgs = readMessages(sim, 1, pending);
    REQUIRE(msgs[0]["type"] == "helloAck");
    for (int i = 0; i < 100 && f.dashboards.empty(); ++i) std::this_thread::sleep_for(10ms);
    REQUIRE(f.dashboards.size() == 1);
    REQUIRE(f.dashboards[0]["type"] == "dashboard");
    REQUIRE(f.dashboards[0]["source"] == "auto");
    REQUIRE(f.dashboards[0]["xml"].get<std::string>().find("motor.speed") != std::string::npos);
    REQUIRE(f.unconnected == std::vector<std::string>{"ghost.var"});
    closeS(sim);
}

TEST_CASE("sendCommand sans sim connectée -> false", "[pivot]") {
    Fixture f;
    REQUIRE_FALSE(f.pivot.sendCommand(makeCmd("run")));
}
