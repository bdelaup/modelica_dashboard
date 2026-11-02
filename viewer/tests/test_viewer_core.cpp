// Tests du viewer_core (sans WebView2) : UiSink mémoire, vrai pivot loopback,
// client sim socket de test. Couvre le routage UI->pivot->sim et pivot->UI.
#include "catch_amalgamated.hpp"
#include "viewer_core.hpp"
#include "dashboard/dashboard_file.hpp"

#include <filesystem>
#include <fstream>

#include <atomic>
#include <chrono>
#include <mutex>
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
struct MemSink : UiSink {
    std::mutex mtx;
    std::vector<json> out;
    void send(const json& msg) override { std::lock_guard<std::mutex> lk(mtx); out.push_back(msg); }
    std::vector<json> snapshot() { std::lock_guard<std::mutex> lk(mtx); return out; }
};

socket_t connectSim() {
    socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

void sendAll(socket_t s, const std::string& d) {
    REQUIRE(send(s, d.data(), static_cast<int>(d.size()), 0) == static_cast<int>(d.size()));
}

std::vector<json> readN(socket_t s, std::size_t n, std::string& pending) {
    std::vector<json> out;
    char buf[8192];
    auto deadline = std::chrono::steady_clock::now() + 2s;
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
} // namespace

TEST_CASE("viewer_core : handshake sim -> simEvent, data -> tick routé au sink", "[viewer_core]") {
    auto sink = std::make_unique<MemSink>();
    MemSink* raw = sink.get();
    ViewerCore core(std::move(sink));
    REQUIRE(core.start());
    for (int i = 0; i < 100 && !core.pivot().isListening(); ++i) std::this_thread::sleep_for(10ms);

    socket_t sim = connectSim();
    REQUIRE(sim != kInv);
    sendAll(sim, encodeFrame(makeHello({"a.b"})));
    std::string pending;
    auto ack = readN(sim, 1, pending);
    REQUIRE(ack.size() == 1);
    REQUIRE(ack[0]["type"] == "helloAck");

    sendAll(sim, encodeFrame(makeData(1, 0.5, json{{"a.b", 42.0}})));
    sendAll(sim, encodeFrame(makeState("running")));

    for (int i = 0; i < 100; ++i) {
        auto out = raw->snapshot();
        bool hasTick = false, hasState = false, hasConn = false;
        for (const auto& m : out) {
            if (m["type"] == "tick" && m["values"]["a.b"] == 42.0) hasTick = true;
            if (m["type"] == "simState" && m["state"] == "running") hasState = true;
            if (m["type"] == "simEvent" && m["connected"].get<bool>() == true) hasConn = true;
        }
        if (hasTick && hasState && hasConn) break;
        std::this_thread::sleep_for(10ms);
    }
    auto out = raw->snapshot();
    bool hasTick = false, hasState = false, hasConn = false;
    for (const auto& m : out) {
        if (m["type"] == "tick" && m["values"]["a.b"] == 42.0) hasTick = true;
        if (m["type"] == "simState" && m["state"] == "running") hasState = true;
        if (m["type"] == "simEvent" && m["connected"].get<bool>() == true) hasConn = true;
    }
    CHECK(hasConn);
    CHECK(hasTick);
    CHECK(hasState);

    // UI -> core -> pivot -> sim : cmd set
    core.onUiMessage(makeCmd("set", json{{"variable", "a.b"}, {"value", 7}}));
    auto cmds = readN(sim, 1, pending);
    REQUIRE(cmds.size() == 1);
    CHECK(cmds[0]["cmd"] == "set");
    CHECK(cmds[0]["value"] == 7);

    closeS(sim);
    core.stop();
}

TEST_CASE("viewer_core : loadDashboard via message UI (mode manuel)", "[viewer_core]") {
    auto sink = std::make_unique<MemSink>();
    MemSink* raw = sink.get();
    ViewerCore core(std::move(sink));
    REQUIRE(core.start());
    for (int i = 0; i < 100 && !core.pivot().isListening(); ++i) std::this_thread::sleep_for(10ms);

    // sim connectée pour que loadDashboardFile calcule unconnected avec variables modèle
    socket_t sim = connectSim();
    REQUIRE(sim != kInv);
    sendAll(sim, encodeFrame(makeHello({"motor.speed"})));
    std::string pending;
    readN(sim, 1, pending);

    // fichier temporaire
    DashboardFile dash;
    dash.dashboardVersion = 0;
    dash.name = "M";
    dash.widgets.push_back({"w1", "gauge", "motor.speed", "V", 0, 0, 100, 60});
    dash.widgets.push_back({"w2", "button", "ghost", "G", 0, 80, 100, 40});
    std::string path = (std::filesystem::temp_directory_path() / "viewer_core_test.drawio").string();
    { std::ofstream o(path); o << serializeDashboardFile(dash); }

    core.onUiMessage(json{{"type", "loadDashboard"}, {"path", path}});
    for (int i = 0; i < 100; ++i) {
        bool hasDash = false;
        for (const auto& m : raw->snapshot()) if (m["type"] == "dashboard") hasDash = true;
        if (hasDash) break;
        std::this_thread::sleep_for(10ms);
    }
    bool hasDash = false, hasUnc = false;
    for (const auto& m : raw->snapshot()) {
        if (m["type"] == "dashboard" && m["source"] == "auto") hasDash = true;
        if (m["type"] == "unconnected" && m["variables"] == json::array({"ghost"})) hasUnc = true;
    }
    CHECK(hasDash);
    CHECK(hasUnc);
    closeS(sim);
    core.stop();
}
