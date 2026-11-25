#!/usr/bin/env python3
"""Network smoke test: simultaneous salvos and dynamic observer connections."""
import re
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def connect_observer(port):
    sock = socket.create_connection(("127.0.0.1", port), timeout=3)
    sock.sendall(b"HELLO OBSERVER\n")
    data = b""
    while b"ENDSNAPSHOT\n" not in data:
        data += sock.recv(4096)
    assert b"SNAPSHOT" in data and data.count(b"GRID fleet=") == 16
    return sock


def run_case(rounds, dynamic, extra=(), strategy="sequential"):
    port = free_port()
    temporary_log = tempfile.NamedTemporaryFile(prefix="hw4-integration-", suffix=".log", delete=False)
    log = Path(temporary_log.name)
    temporary_log.close()
    server = subprocess.Popen([str(ROOT / "naval_server"), "0.0.0.0", str(port),
                               "--size", "8", "--fleet", "3,2,1", "--rounds", str(rounds),
                               "--delay", "100", "--seed", "42", "--log", str(log), *extra],
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    ships = []
    observers = []
    try:
        time.sleep(.2)
        observers.append(connect_observer(port))
        for fleet in (1, 2):
            for ship in (1, 2, 3):
                ships.append(subprocess.Popen([str(ROOT / "naval_ship"), "127.0.0.1",
                                               str(port), str(fleet), str(ship), strategy],
                                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE))
        if dynamic:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if log.exists() and "ROUND 2 BEGIN" in log.read_text():
                    break
                time.sleep(.03)
            observers[0].close()
            observers.append(connect_observer(port))
            observers.append(connect_observer(port))
        server.wait(timeout=15)
        for child in ships:
            assert child.wait(timeout=3) == 0, child.stderr.read().decode()
        events = log.read_text()
        assert "GAME_OVER" in events
        assert "reason=round_limit" in events or "reason=fleet_destroyed" in events
        if dynamic:
            assert events.count("OBSERVER_JOINED") >= 3
            assert "OBSERVER_LEFT" in events
        lines = events.splitlines()
        hit_cells = set()
        round_health = {1: [3, 2, 1], 2: [3, 2, 1]}
        for i, line in enumerate(lines):
            if re.match(r"ROUND \d+ APPLY", line):
                segment = lines[:i]
                round_number = int(line.split()[1])
                current = [x for x in segment if x.startswith(f"TARGET round={round_number} ")]
                assert any("fleet=1" in x for x in current)
                assert any("fleet=2" in x for x in current)
                assert len(current) <= 12
            if line.startswith("HIT "):
                fleet = int(re.search(r"target_fleet=(\d+)", line).group(1))
                ship = int(re.search(r"target_ship=(\d+)", line).group(1))
                x = int(re.search(r"x=(\d+)", line).group(1))
                y = int(re.search(r"y=(\d+)", line).group(1))
                key = (fleet, x, y)
                assert key not in hit_cells, "a deck was destroyed twice"
                hit_cells.add(key)
                round_health[fleet][ship - 1] -= 1
                remaining = int(re.search(r"remaining=(\d+)", line).group(1))
                assert remaining == round_health[fleet][ship - 1]
        print(f"PASS rounds={rounds} dynamic={dynamic} strategy={strategy} options={extra} observers={events.count('OBSERVER_JOINED')}")
    finally:
        for sock in observers:
            sock.close()
        for child in ships:
            if child.poll() is None:
                child.terminate()
        if server.poll() is None:
            server.terminate()
        server.wait(timeout=3)
        log.unlink(missing_ok=True)


if __name__ == "__main__":
    run_case(5, True, strategy="random")
    run_case(1, False)
    run_case(2, False, ("--placement", "rows", "--shots", "one", "--repeat"))
