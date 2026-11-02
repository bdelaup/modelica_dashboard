// Tests unitaires du contrat de protocole (docs/protocole.md, version 0).
#include "catch_amalgamated.hpp"
#include "dashboard/protocol.hpp"

using namespace dashboard;

TEST_CASE("constantes du contrat", "[protocol]") {
    REQUIRE(kPort == 51234);
    REQUIRE(kProtocolVersion == 0);
    REQUIRE(kMaxFrameBytes == 1u << 20);
    REQUIRE(kLoopbackHost == "127.0.0.1");
}

TEST_CASE("version N lit N et N-1, refuse le reste (ADR-0009)", "[protocol]") {
    REQUIRE(versionSupported(0));
    REQUIRE(versionSupported(-1));
    REQUIRE_FALSE(versionSupported(1));
    REQUIRE_FALSE(versionSupported(-2));
}

TEST_CASE("hello avec et sans dashboardFile (mode auto/manuel)", "[protocol]") {
    auto h = makeHello({"a.b", "c.d"}, "C:\\m\\D.dashboard.drawio");
    REQUIRE(h["type"] == "hello");
    REQUIRE(h["version"] == 0);
    REQUIRE(h["role"] == "simulation");
    REQUIRE(h["variables"] == json::array({"a.b", "c.d"}));
    REQUIRE(h["dashboardFile"] == "C:\\m\\D.dashboard.drawio");

    auto h2 = makeHello({"a.b"});
    REQUIRE_FALSE(h2.contains("dashboardFile"));

    ErrorCode err;
    REQUIRE(validateMessage(h, err));
    REQUIRE(validateMessage(h2, err));
}

TEST_CASE("data / tick / state / cmd / unconnected / dashboard / simState", "[protocol]") {
    ErrorCode err;
    auto data = makeData(1, 0.01, json{{"motor.speed", 0.0}, {"motor.on", true}});
    REQUIRE(validateMessage(data, err));
    REQUIRE(data["values"]["motor.on"] == true);

    auto tick = makeTick(1, 0.01, 1.0, json{{"motor.speed", 0.0}});
    REQUIRE(tick["type"] == "tick");
    REQUIRE(tick["ratio"] == 1.0);

    REQUIRE(validateMessage(makeState("running"), err));
    REQUIRE(validateMessage(makeState("finished"), err));
    REQUIRE_FALSE(validateMessage(makeState("flying"), err));

    REQUIRE(validateMessage(makeCmd("run"), err));
    REQUIRE(validateMessage(makeCmd("runToEnd", json{{"duration", 10.0}}), err));
    REQUIRE(validateMessage(makeCmd("set", json{{"variable", "a.b"}, {"value", 2.0}}), err));
    REQUIRE_FALSE(validateMessage(makeCmd("explode"), err));

    REQUIRE(validateMessage(makeUnconnected({"x.y"}), err));
    REQUIRE(validateMessage(makeDashboard("auto", "f", "<mxfile/>"), err));
    REQUIRE(validateMessage(makeSimState("paused"), err));
    REQUIRE(validateMessage(makeHelloAck(), err));
    REQUIRE(validateMessage(makeError(ErrorCode::Busy, "..."), err));
}

TEST_CASE("validation : mauvais role, mauvaise version, type inconnu", "[protocol]") {
    ErrorCode err;
    json badRole = makeHello({"a"});
    badRole["role"] = "viewer";
    REQUIRE_FALSE(validateMessage(badRole, err));
    REQUIRE(err == ErrorCode::BadRole);

    json badVer = makeHello({"a"});
    badVer["version"] = 42;
    REQUIRE_FALSE(validateMessage(badVer, err));
    REQUIRE(err == ErrorCode::BadVersion);

    json unk = json{{"type", "nope"}, {"version", 0}};
    REQUIRE_FALSE(validateMessage(unk, err));
    REQUIRE(err == ErrorCode::UnknownType);

    json notObj = json::array({1, 2});
    REQUIRE_FALSE(validateMessage(notObj, err));
    REQUIRE(err == ErrorCode::BadJson);
}

TEST_CASE("framing NDJSON : encode/decode multi-lignes + reste partiel", "[protocol]") {
    std::string buf;
    buf += encodeFrame(makeHello({"a"}));
    buf += encodeFrame(makeData(1, 0.1, json{{"a", 1.0}}));
    buf += "{\"type\":\"data\",\"vers"; // ligne incomplète
    auto res = decodeFrames(buf);
    REQUIRE(res.ok);
    REQUIRE(res.messages.size() == 2);
    REQUIRE(res.messages[0]["type"] == "hello");
    REQUIRE(res.remainder == "{\"type\":\"data\",\"vers");
}

TEST_CASE("framing : ligne trop grande -> frameTooLarge (D35)", "[protocol]") {
    std::string huge(kMaxFrameBytes + 10, 'x');
    huge.push_back('\n');
    auto res = decodeFrames(huge);
    REQUIRE_FALSE(res.ok);
    REQUIRE(res.error == ErrorCode::FrameTooLarge);
}

TEST_CASE("framing : JSON malformé -> badJson", "[protocol]") {
    auto res = decodeFrames("{pas du json}\n");
    REQUIRE_FALSE(res.ok);
    REQUIRE(res.error == ErrorCode::BadJson);
}

TEST_CASE("encodeFrame refuse une trame > 1 MiB", "[protocol]") {
    json big;
    big["type"] = "data";
    big["version"] = 0;
    big["seq"] = 1;
    big["values"] = json::object();
    big["values"]["pad"] = std::string(kMaxFrameBytes, 'x');
    REQUIRE_THROWS_AS(encodeFrame(big), std::length_error);
}

TEST_CASE("codes d'erreur : table complète (protocole.md §8)", "[protocol]") {
    REQUIRE(errorCodeToString(ErrorCode::BadJson) == "badJson");
    REQUIRE(errorCodeToString(ErrorCode::FrameTooLarge) == "frameTooLarge");
    REQUIRE(errorCodeToString(ErrorCode::BadVersion) == "badVersion");
    REQUIRE(errorCodeToString(ErrorCode::Busy) == "busy");
    REQUIRE(errorCodeToString(ErrorCode::BadRole) == "badRole");
    REQUIRE(errorCodeToString(ErrorCode::UnknownType) == "unknownType");
    REQUIRE(errorCodeToString(ErrorCode::BadCmd) == "badCmd");
}
