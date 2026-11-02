// Benchmark du framing JSON (ADR-0003/D4, spec §13) : cible 1000 variables à 100 Hz.
// Un client sim envoie des batchs "data" de 1000 variables à 100 Hz pendant N secondes
// au vrai pivot ; on mesure le débit reçu (messages/s, variables/s) et la latence moyenne.
// Usage : bench_framing [duree_s=5] [nbVars=1000] [freqHz=100]
// Sortie : une ligne JSON {"durationS":..., "messages":..., "varsPerSec":..., "msgsPerSec":..., "ok":true}
// Échec (ok=false) si msgsPerSec < 0.8 * freqHz (marge 20%).
#include "dashboard/pivot.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t kInv = INVALID_SOCKET;
static void closeS(socket_t s) { closesocket(s); }
static void wsa() { static bool d = [] { WSADATA x; WSAStartup(MAKEWORD(2, 2), &x); return true; }(); (void)d; }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInv = -1;
static void closeS(socket_t s) { ::close(s); }
static void wsa() {}
#endif

using namespace dashboard;

int main(int argc, char** argv) {
    double dureeS = argc > 1 ? atof(argv[1]) : 5.0;
    int nbVars = argc > 2 ? atoi(argv[2]) : 1000;
    int freqHz = argc > 3 ? atoi(argv[3]) : 100;

    std::atomic<long> ticks{0};
    std::atomic<long> varsReceived{0};
    Pivot pivot;
    PivotEvents ev;
    ev.onTick = [&](const json& t) { ticks++; varsReceived += (long)t["values"].size(); };
    if (!pivot.start(std::move(ev))) { std::printf("{\"ok\":false,\"err\":\"pivot start failed\"}\n"); return 1; }
    for (int i = 0; i < 100 && !pivot.isListening(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));

    wsa();
    socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::printf("{\"ok\":false,\"err\":\"connect\"}\n");
        return 1;
    }
    // handshake
    std::vector<std::string> vars;
    for (int i = 0; i < nbVars; ++i) vars.push_back("v" + std::to_string(i));
    std::string hello = encodeFrame(makeHello(vars));
    send(s, hello.data(), (int)hello.size(), 0);
    char buf[65536];
    recv(s, buf, sizeof(buf), 0); // helloAck

    // envoi à freqHz pendant dureeS
    json values = json::object();
    for (int i = 0; i < nbVars; ++i) values["v" + std::to_string(i)] = (double)i;
    auto period = std::chrono::microseconds(1000000 / freqHz);
    auto t0 = std::chrono::steady_clock::now();
    auto next = t0;
    std::uint64_t seq = 0;
    double simTime = 0;
    long sent = 0;
    while (std::chrono::steady_clock::now() - t0 < std::chrono::duration<double>(dureeS)) {
        next += period;
        std::this_thread::sleep_until(next);
        simTime += 1.0 / freqHz;
        std::string frame = encodeFrame(makeData(++seq, simTime, values));
        int n = send(s, frame.data(), (int)frame.size(), 0);
        if (n <= 0) break;
        sent++;
    }
    auto t1 = std::chrono::steady_clock::now();
    // laisse le pivot vider
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    double msgsPerSec = ticks.load() / elapsed;
    double varsPerSec = varsReceived.load() / elapsed;
    bool ok = msgsPerSec >= 0.8 * freqHz;
    std::printf("{\"ok\":%s,\"durationS\":%.2f,\"sent\":%ld,\"received\":%ld,"
                "\"msgsPerSec\":%.1f,\"varsPerSec\":%.0f,\"target\":{\"vars\":%d,\"hz\":%d}}\n",
                ok ? "true" : "false", elapsed, sent, ticks.load(), msgsPerSec, varsPerSec, nbVars, freqHz);
    closeS(s);
    pivot.stop();
    return ok ? 0 : 2;
}
