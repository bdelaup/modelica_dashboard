// Contrat du canal modèle ↔ pivot (docs/protocole.md, version 0).
// Un seul endroit définit les constantes et la sérialisation/validation des messages.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace dashboard {

// D26 — port fixe du canal, loopback uniquement (ENF-2).
inline constexpr std::string_view kLoopbackHost = "127.0.0.1";
inline constexpr std::uint16_t kPort = 51234;

// D31 — version du protocole en V1.
inline constexpr int kProtocolVersion = 0;

// D35 — taille maximale d'une ligne NDJSON (1 MiB).
inline constexpr std::size_t kMaxFrameBytes = 1u << 20; // 1 048 576

// D20 / ADR-0009 — un récepteur de version N lit la version N-1 (champs inconnus ignorés).
inline bool versionSupported(int version) {
    return version == kProtocolVersion || version == kProtocolVersion - 1;
}

using json = nlohmann::json;

enum class MsgType {
    Hello, HelloAck, Data, State, Tick, Unconnected, Dashboard, SimState, Cmd, Ack, Error,
    Unknown
};

MsgType msgTypeFromString(std::string_view s);
std::string_view msgTypeToString(MsgType t);

// Codes d'erreur (protocole.md §8).
enum class ErrorCode {
    BadJson, FrameTooLarge, BadVersion, Busy, BadRole, UnknownType, BadCmd
};

std::string_view errorCodeToString(ErrorCode c);

// --- Construction des messages ---
json makeHello(const std::vector<std::string>& variables, const std::string& dashboardFile = {});
json makeHelloAck();
json makeError(ErrorCode code, std::string message);
json makeData(std::uint64_t seq, double time, json values);
json makeState(std::string_view state);            // sim -> pivot (D40)
json makeCmd(std::string_view cmd, const json& extra = json::object());
json makeTick(std::uint64_t seq, double simTime, double ratio, json values);
json makeUnconnected(const std::vector<std::string>& variables);
json makeDashboard(std::string_view source, std::string file, std::string xml);
json makeSimState(std::string_view state);
json makeAck(std::uint64_t seq = 0);

// --- Validation sémantique (protocole.md §3, §8) ---
// Retourne true si le message est valide ; sinon false et err contient le code d'erreur.
bool validateMessage(const json& msg, ErrorCode& err);

// --- Framing NDJSON ---
// Sérialise un message en une ligne terminée par '\n' (lève si > kMaxFrameBytes).
std::string encodeFrame(const json& msg);
// Découpe un buffer en lignes complètes ; retourne false + errorCode si une ligne
// dépasse kMaxFrameBytes ou n'est pas du JSON valide.
struct DecodeResult {
    bool ok = true;
    ErrorCode error = ErrorCode::BadJson;
    std::vector<json> messages;
    std::string remainder; // fin incomplète à conserver pour le prochain buffer
};
DecodeResult decodeFrames(std::string_view buffer);

// --- Helpers de lecture typée ---
std::vector<std::string> readStringArray(const json& msg, std::string_view key);

} // namespace dashboard
