#!/usr/bin/env python3
"""E2E headless (portage Linux/CI) :
lance viewer_headless (vrai process, pivot embarqué sur 127.0.0.1:51234),
connecte un client sim (socket) qui fait handshake/data/state, envoie une
commande depuis le shell (stdin du viewer) et vérifie les messages reçus
en sortie (stdout NDJSON) : simEvent, tick, simState, et le cmd relayé à la sim.

Usage : test_headless_e2e.py <chemin viewer_headless>   (exit 0 si OK)
"""
import json
import os
import socket
import subprocess
import sys
import time

PORT = 51234


def readline_json(pipe, buf, timeout=5.0):
    """Lit une ligne JSON sur un pipe texte (Windows : pas de select sur pipe -> readline bloquant
    avec timeout via thread, ou lecture directe). buf = reste non consommé."""
    import threading
    if sys.platform == "win32":
        # Windows : select ne marche pas sur les pipes ; on lit en thread avec timeout
        result = {}
        def _read():
            try:
                line = pipe.readline()
                result["line"] = line
            except Exception as e:
                result["err"] = e
        t = threading.Thread(target=_read, daemon=True)
        t.start()
        t.join(timeout)
        if "line" not in result:
            return None
        line = result["line"]
        return json.loads(line) if line.strip() else None
    import select
    fd = pipe.fileno()
    while b"\n" not in buf[0]:
        r, _, _ = select.select([fd], [], [], timeout)
        if not r:
            return None
        chunk = os.read(fd, 65536)
        if not chunk:
            return None
        buf[0] += chunk
    line, rest = buf[0].split(b"\n", 1)
    buf[0] = rest
    return json.loads(line) if line.strip() else None


def main():
    viewer = sys.argv[1] if len(sys.argv) > 1 else "./build/viewer/viewer_headless"
    proc = subprocess.Popen([viewer], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, bufsize=1)
    try:
        # attend que le pivot écoute
        sim = None
        for _ in range(100):
            try:
                sim = socket.create_connection(("127.0.0.1", PORT), timeout=1)
                break
            except OSError:
                time.sleep(0.05)
        assert sim is not None, "pivot non joignable"
        sim.settimeout(5)

        # handshake
        sim.sendall((json.dumps({"type": "hello", "version": 0, "role": "simulation",
                                 "variables": ["motor.speed", "motor.on"]}) + "\n").encode())
        ack = json.loads(sim.recv(65536).decode().splitlines()[0])
        assert ack["type"] == "helloAck", ack

        # data + state depuis la sim
        sim.sendall((json.dumps({"type": "data", "version": 0, "seq": 1, "time": 0.01,
                                 "values": {"motor.speed": 3.5}}) + "\n").encode())
        sim.sendall((json.dumps({"type": "state", "version": 0, "state": "running"}) + "\n").encode())

        # commande depuis le shell du viewer (stdin) -> doit arriver à la sim
        proc.stdin.write(json.dumps({"type": "cmd", "version": 0, "cmd": "set",
                                     "variable": "motor.on", "value": False}) + "\n")
        proc.stdin.flush()

        got_sim = []
        sim.settimeout(2)
        try:
            while len(got_sim) < 1:
                got_sim.append(json.loads(sim.recv(65536).decode().splitlines()[0]))
        except (socket.timeout, IndexError):
            pass
        assert any(m.get("cmd") == "set" and m.get("value") is False for m in got_sim), got_sim

        # sorties du viewer (stdout) : simEvent, tick, simState
        out = []
        buf = [b""]
        pipe = proc.stdout
        deadline = time.time() + 3
        while time.time() < deadline and not (any(m["type"] == "tick" for m in out)
                                               and any(m["type"] == "simState" for m in out)
                                               and any(m["type"] == "simEvent" for m in out)):
            m = readline_json(pipe, buf, timeout=max(0.1, deadline - time.time()))
            if m:
                out.append(m)
        types = [m["type"] for m in out]
        assert "simEvent" in types, out
        assert "tick" in types and any(m.get("values", {}).get("motor.speed") == 3.5 for m in out if m["type"] == "tick"), out
        assert "simState" in types and any(m.get("state") == "running" for m in out), out

        print("E2E HEADLESS OK :", types)
        return 0
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    sys.exit(main())
