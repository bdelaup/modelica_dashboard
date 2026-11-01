#include "dashboard/pivot.hpp"
#include "dashboard/dashboard_file.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
static int closeSocket(socket_t s) { return ::closesocket(s); }
static int socketError() { return WSAGetLastError(); }
struct WsaInit {
    WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
};
static WsaInit g_wsa;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
static int closeSocket(socket_t s) { return ::close(s); }
static int socketError() { return errno; }
#endif

namespace dashboard {

namespace {
double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool setNoDelay(socket_t s) {
    int flag = 1;
    return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}
} // namespace

Pivot::Pivot() = default;

Pivot::~Pivot() { stop(); }

bool Pivot::start(PivotEvents events) {
    if (running_.exchange(true)) return false;
    events_ = std::move(events);
    listenFd_ = static_cast<socket_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (listenFd_ == kInvalidSocket) { running_ = false; return false; }
    int one = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    inet_pton(AF_INET, std::string(kLoopbackHost).c_str(), &addr.sin_addr); // 127.0.0.1 uniquement (ENF-2)
    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listenFd_, 1) != 0) {
        closeSocket(listenFd_);
        listenFd_ = -1;
        running_ = false;
        return false;
    }
    listening_ = true;
    serverThread_ = std::thread([this] { serverLoop(); });
    return true;
}

void Pivot::stop() {
    if (!running_.exchange(false)) return;
    listening_ = false;
    if (listenFd_ != -1 && listenFd_ != kInvalidSocket) {
        shutdown(listenFd_, 2 /*SD_BOTH*/);
        closeSocket(listenFd_);
        listenFd_ = -1;
    }
    if (clientFd_ != -1 && clientFd_ != kInvalidSocket) {
        shutdown(clientFd_, 2);
        closeSocket(clientFd_);
        clientFd_ = -1;
    }
    connected_ = false;
    if (serverThread_.joinable()) serverThread_.join();
    if (clientThread_.joinable()) clientThread_.join();
}

void Pivot::serverLoop() {
    while (running_) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        socket_t c = accept(listenFd_, reinterpret_cast<sockaddr*>(&peer), &len);
        if (c == kInvalidSocket) {
            if (!running_) break;
            continue;
        }
        if (connected_.load()) {
            // D25 : une seule sim — on refuse la 2e (error/busy puis fermeture)
            json e = makeError(ErrorCode::Busy, "une simulation est déjà connectée");
            std::string frame = encodeFrame(e);
            send(c, frame.data(), static_cast<int>(frame.size()), 0);
            closeSocket(c);
            if (events_.onProtocolError) events_.onProtocolError(e);
            continue;
        }
        clientFd_ = static_cast<int>(c);
        setNoDelay(c);
        connected_ = true;
        pendingBuffer_.clear();
        clientThread_ = std::thread([this] { clientLoop(); });
    }
}

void Pivot::clientLoop() {
    socket_t c = static_cast<socket_t>(clientFd_);
    char buf[64 * 1024];
    bool handshaken = false;
    while (running_ && connected_) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) break;
        pendingBuffer_.append(buf, static_cast<std::size_t>(n));
        auto res = decodeFrames(pendingBuffer_);
        pendingBuffer_ = std::move(res.remainder);
        if (!res.ok) {
            json e = makeError(res.error, "trame invalide");
            sendJson(e);
            setError(e);
            break;
        }
        for (const auto& m : res.messages) {
            ErrorCode verr;
            if (!validateMessage(m, verr)) {
                json e = makeError(verr, "message invalide : " + m.dump());
                sendJson(e);
                setError(e);
                if (verr == ErrorCode::BadVersion || verr == ErrorCode::BadJson) goto disconnect;
                continue;
            }
            if (!handshaken) {
                if (msgTypeFromString(m["type"].get<std::string>()) == MsgType::Hello) {
                    handshaken = true;
                    modelVariables_ = readStringArray(m, "variables");
                    sendJson(makeHelloAck());
                    if (events_.onSimConnected) events_.onSimConnected();
                    // Mode auto (D11) : la sim pousse le chemin du fichier dashboard
                    if (m.contains("dashboardFile") && m["dashboardFile"].is_string()) {
                        loadDashboardFile(m["dashboardFile"].get<std::string>(), modelVariables_);
                    }
                } else {
                    json e = makeError(ErrorCode::BadJson, "hello attendu en premier");
                    sendJson(e);
                    setError(e);
                    goto disconnect;
                }
                continue;
            }
            handleMessage(m);
        }
    }
disconnect:
    {
        std::lock_guard<std::mutex> lk(sendMutex_);
        if (clientFd_ != -1 && clientFd_ != kInvalidSocket) {
            closeSocket(static_cast<socket_t>(clientFd_));
            clientFd_ = -1;
        }
    }
    connected_ = false;
    if (events_.onSimDisconnected) events_.onSimDisconnected();
}

void Pivot::handleMessage(const json& msg) {
    switch (msgTypeFromString(msg["type"].get<std::string>())) {
        case MsgType::Data: {
            if (events_.onData) events_.onData(msg);
            // tick = data + ratio temps réel (EF-V5, D24 : le pivot pousse toujours)
            double simTime = msg.value("time", 0.0);
            double t = nowSeconds();
            if (lastRealTime_ > 0.0 && simTime > lastSimTime_) {
                double dr = t - lastRealTime_;
                double ds = simTime - lastSimTime_;
                if (dr > 0) ratio_ = 0.9 * ratio_ + 0.1 * (ds / dr); // moyenne glissante
            }
            lastSimTime_ = simTime;
            lastRealTime_ = t;
            std::uint64_t seq = msg.value("seq", ++lastSeq_);
            lastSeq_ = seq;
            json tick = makeTick(seq, simTime, ratio_, msg["values"]);
            if (events_.onTick) events_.onTick(tick);
            break;
        }
        case MsgType::State:
            if (events_.onSimState) events_.onSimState(msg.value("state", ""));
            break;
        default:
            break;
    }
}

bool Pivot::sendJson(const json& msg) {
    std::string frame;
    try {
        frame = encodeFrame(msg);
    } catch (...) {
        return false;
    }
    std::lock_guard<std::mutex> lk(sendMutex_);
    if (clientFd_ == -1 || clientFd_ == kInvalidSocket) return false;
    int sent = send(static_cast<socket_t>(clientFd_), frame.data(), static_cast<int>(frame.size()), 0);
    return sent == static_cast<int>(frame.size());
}

bool Pivot::sendCommand(const json& cmd) {
    ErrorCode verr;
    if (!validateMessage(cmd, verr)) { lastError_ = "commande invalide"; return false; }
    if (!connected_) { lastError_ = "aucune simulation connectée"; return false; }
    return sendJson(cmd);
}

bool Pivot::loadDashboardFile(const std::string& path, const std::vector<std::string>& modelVariables) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        lastError_ = "fichier illisible : " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    auto dash = parseDashboardFile(ss.str());
    if (!dash.valid) {
        lastError_ = dash.error;
        return false;
    }
    modelVariables_ = modelVariables;
    if (events_.onUnconnected) {
        auto unc = unconnectedVariables(modelVariables, dash);
        if (!unc.empty()) events_.onUnconnected(unc);
    }
    std::string xml = ss.str();
    if (events_.onDashboard) events_.onDashboard(makeDashboard("auto", path, std::move(xml)));
    return true;
}

void Pivot::setModelVariables(std::vector<std::string> variables) {
    modelVariables_ = std::move(variables);
}

} // namespace dashboard
