#include "dashboard/protocol.hpp"

#include <stdexcept>

namespace dashboard {

namespace {
struct TypeEntry { MsgType t; std::string_view s; };
constexpr TypeEntry kTypeTable[] = {
    {MsgType::Hello, "hello"}, {MsgType::HelloAck, "helloAck"},
    {MsgType::Data, "data"}, {MsgType::State, "state"},
    {MsgType::Tick, "tick"}, {MsgType::Unconnected, "unconnected"},
    {MsgType::Dashboard, "dashboard"}, {MsgType::SimState, "simState"},
    {MsgType::Cmd, "cmd"}, {MsgType::Ack, "ack"}, {MsgType::Error, "error"},
};
constexpr std::string_view kStateValues[] = {"running", "paused", "stopped", "finished"};
constexpr std::string_view kCmdValues[] = {"run", "pause", "runToEnd", "stop", "set"};
constexpr std::string_view kErrorCodes[] = {
    "badJson", "frameTooLarge", "badVersion", "busy", "badRole", "unknownType", "badCmd"
};

bool isState(std::string_view s) {
    for (auto v : kStateValues) if (s == v) return true;
    return false;
}
bool isCmd(std::string_view s) {
    for (auto v : kCmdValues) if (s == v) return true;
    return false;
}
} // namespace

MsgType msgTypeFromString(std::string_view s) {
    for (const auto& e : kTypeTable) if (s == e.s) return e.t;
    return MsgType::Unknown;
}

std::string_view msgTypeToString(MsgType t) {
    for (const auto& e : kTypeTable) if (t == e.t) return e.s;
    return "unknown";
}

std::string_view errorCodeToString(ErrorCode c) {
    auto i = static_cast<std::size_t>(c);
    if (i < std::size(kErrorCodes)) return kErrorCodes[i];
    return "unknownType";
}

json makeHello(const std::vector<std::string>& variables, const std::string& dashboardFile) {
    json m;
    m["type"] = "hello";
    m["version"] = kProtocolVersion;
    m["role"] = "simulation";
    m["variables"] = variables;
    if (!dashboardFile.empty()) m["dashboardFile"] = dashboardFile;
    return m;
}

json makeHelloAck() {
    return json{{"type", "helloAck"}, {"version", kProtocolVersion}, {"status", "ok"}};
}

json makeError(ErrorCode code, std::string message) {
    return json{{"type", "error"}, {"version", kProtocolVersion},
                {"code", errorCodeToString(code)}, {"message", std::move(message)}};
}

json makeData(std::uint64_t seq, double time, json values) {
    return json{{"type", "data"}, {"version", kProtocolVersion}, {"seq", seq},
                {"time", time}, {"values", std::move(values)}};
}

json makeState(std::string_view state) {
    return json{{"type", "state"}, {"version", kProtocolVersion}, {"state", state}};
}

json makeCmd(std::string_view cmd, const json& extra) {
    json m{{"type", "cmd"}, {"version", kProtocolVersion}, {"cmd", cmd}};
    if (extra.is_object()) {
        for (auto it = extra.begin(); it != extra.end(); ++it) m[it.key()] = it.value();
    }
    return m;
}

json makeTick(std::uint64_t seq, double simTime, double ratio, json values) {
    return json{{"type", "tick"}, {"version", kProtocolVersion}, {"seq", seq},
                {"simTime", simTime}, {"ratio", ratio}, {"values", std::move(values)}};
}

json makeUnconnected(const std::vector<std::string>& variables) {
    return json{{"type", "unconnected"}, {"version", kProtocolVersion}, {"variables", variables}};
}

json makeDashboard(std::string_view source, std::string file, std::string xml) {
    return json{{"type", "dashboard"}, {"version", kProtocolVersion},
                {"source", source}, {"file", std::move(file)}, {"xml", std::move(xml)}};
}

json makeSimState(std::string_view state) {
    return json{{"type", "simState"}, {"version", kProtocolVersion}, {"state", state}};
}

json makeAck(std::uint64_t seq) {
    json m{{"type", "ack"}, {"version", kProtocolVersion}};
    if (seq != 0) m["seq"] = seq;
    return m;
}

bool validateMessage(const json& msg, ErrorCode& err) {
    if (!msg.is_object()) { err = ErrorCode::BadJson; return false; }
    if (!msg.contains("type") || !msg["type"].is_string()) { err = ErrorCode::BadJson; return false; }
    if (!msg.contains("version") || !msg["version"].is_number_integer()) { err = ErrorCode::BadVersion; return false; }
    int v = msg["version"].get<int>();
    if (!versionSupported(v)) { err = ErrorCode::BadVersion; return false; }

    switch (msgTypeFromString(msg["type"].get<std::string>())) {
        case MsgType::Hello:
            if (msg.value("role", "") != "simulation") { err = ErrorCode::BadRole; return false; }
            if (!msg.contains("variables") || !msg["variables"].is_array()) { err = ErrorCode::BadJson; return false; }
            break;
        case MsgType::Data:
            if (!msg.contains("seq") || !msg.contains("values") || !msg["values"].is_object()) {
                err = ErrorCode::BadJson; return false;
            }
            break;
        case MsgType::State:
        case MsgType::SimState:
            if (!msg.contains("state") || !isState(msg.value("state", ""))) { err = ErrorCode::BadJson; return false; }
            break;
        case MsgType::Cmd:
            if (!msg.contains("cmd") || !isCmd(msg.value("cmd", ""))) { err = ErrorCode::BadCmd; return false; }
            break;
        case MsgType::Error:
            if (!msg.contains("code")) { err = ErrorCode::BadJson; return false; }
            break;
        case MsgType::Unknown:
            err = ErrorCode::UnknownType; return false;
        default:
            break; // helloAck, tick, unconnected, dashboard, ack : champs optionnels
    }
    return true;
}

std::string encodeFrame(const json& msg) {
    std::string line = msg.dump();
    line.push_back('\n');
    if (line.size() > kMaxFrameBytes) throw std::length_error("frame too large");
    return line;
}

DecodeResult decodeFrames(std::string_view buffer) {
    DecodeResult r;
    std::size_t pos = 0;
    while (pos < buffer.size()) {
        auto nl = buffer.find('\n', pos);
        if (nl == std::string_view::npos) {
            r.remainder = std::string(buffer.substr(pos));
            break;
        }
        auto line = buffer.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty()) continue;
        if (line.size() > kMaxFrameBytes) {
            r.ok = false;
            r.error = ErrorCode::FrameTooLarge;
            return r;
        }
        try {
            r.messages.push_back(json::parse(line));
        } catch (...) {
            r.ok = false;
            r.error = ErrorCode::BadJson;
            return r;
        }
    }
    return r;
}

std::vector<std::string> readStringArray(const json& msg, std::string_view key) {
    std::vector<std::string> out;
    if (auto it = msg.find(key); it != msg.end() && it->is_array()) {
        for (const auto& v : *it) if (v.is_string()) out.push_back(v.get<std::string>());
    }
    return out;
}

} // namespace dashboard
