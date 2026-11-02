#!/usr/bin/env python3
"""Tests de contrat (golden-file, ADR-0008/D19) :
- session_reference.ndjson : chaque ligne doit être un message valide du protocole v0
  (types, champs obligatoires, codes d'erreur) — toute évolution volontaire du format
  fait échouer ce test (versionnage explicite, ADR-0009).
- dashboard_reference.drawio : parse valide, dashboardVersion=0, 4 widgets de types V1.

Usage : python3 tests/contract/test_contract.py   (exit 0 si OK)
"""
import json
import os
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))

PROTOCOL_VERSION = 0
TYPES = {"hello", "helloAck", "data", "state", "tick", "unconnected",
         "dashboard", "simState", "cmd", "ack", "error"}
ERROR_CODES = {"badJson", "frameTooLarge", "badVersion", "busy", "badRole",
               "unknownType", "badCmd"}
STATES = {"running", "paused", "stopped", "finished"}
CMDS = {"run", "pause", "runToEnd", "stop", "set"}
WIDGET_TYPES_V1 = {"value", "gauge", "curve", "button", "slider", "log"}

failures = []


def check(cond, label):
    if cond:
        print(f"ok : {label}")
    else:
        print(f"ECHEC : {label}")
        failures.append(label)


def version_supported(v):
    return v == PROTOCOL_VERSION or v == PROTOCOL_VERSION - 1


def validate(msg):
    if not isinstance(msg, dict):
        return False
    if msg.get("type") not in TYPES:
        return False
    if not isinstance(msg.get("version"), int) or not version_supported(msg["version"]):
        return False
    t = msg["type"]
    if t == "hello":
        return msg.get("role") == "simulation" and isinstance(msg.get("variables"), list)
    if t == "data":
        return "seq" in msg and isinstance(msg.get("values"), dict)
    if t in ("state", "simState"):
        return msg.get("state") in STATES
    if t == "cmd":
        return msg.get("cmd") in CMDS
    if t == "error":
        return msg.get("code") in ERROR_CODES
    return True


def main():
    # 1) golden NDJSON
    with open(os.path.join(HERE, "session_reference.ndjson"), encoding="utf-8") as f:
        lines = [l for l in f.read().splitlines() if l.strip()]
    check(len(lines) == 11, "session_reference.ndjson : 11 lignes")
    for i, line in enumerate(lines, 1):
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            check(False, f"ligne {i} : JSON valide")
            continue
        check(validate(msg), f"ligne {i} : message valide ({msg.get('type')})")
    # le golden doit contenir au moins un cycle complet
    types = [json.loads(l)["type"] for l in lines]
    check(types[0] == "hello" and types[1] == "helloAck", "début : hello -> helloAck")
    check("tick" in types and "simState" in types and "cmd" in types, "présence de tick/simState/cmd")

    # 2) golden .drawio
    tree = ET.parse(os.path.join(HERE, "dashboard_reference.drawio"))
    root = tree.getroot()
    check(root.tag == "mxfile", "racine mxfile")
    check(root.get("dashboardVersion") == "0", "dashboardVersion=0 (D31)")
    cells = root.findall(".//mxCell")
    widgets = [c for c in cells if c.get("type") in WIDGET_TYPES_V1 or c.get("UserObject")]
    check(len(widgets) == 4, f"4 widgets (trouvé {len(widgets)})")
    for w in widgets:
        check(w.get("type") in WIDGET_TYPES_V1, f"widget {w.get('id')} : type V1 ({w.get('type')})")

    if failures:
        print(f"CONTRAT ECHOUE ({len(failures)} erreurs)")
        return 1
    print("CONTRAT OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
