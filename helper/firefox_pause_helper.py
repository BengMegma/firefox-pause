#!/usr/bin/env python3
"""
Native Linux helper for the Firefox Pause Geode mod.
Listens for UDP "pause" / "play" on 127.0.0.1 and controls Firefox via MPRIS (playerctl).

It only resumes players that IT paused, so it never starts music you weren't playing.
"""
import socket
import subprocess
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 48620
paused_by_us: set[str] = set()


def playerctl(*args: str) -> str:
    try:
        return subprocess.run(
            ["playerctl", *args], capture_output=True, text=True, timeout=3
        ).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return ""


def firefox_players() -> list[str]:
    return [p for p in playerctl("-l").splitlines() if p.startswith("firefox")]


def pause() -> None:
    for p in firefox_players():
        if playerctl("-p", p, "status") == "Playing":
            playerctl("-p", p, "pause")
            paused_by_us.add(p)
            print(f"paused {p}", flush=True)


def play() -> None:
    for p in list(paused_by_us):
        playerctl("-p", p, "play")
        print(f"resumed {p}", flush=True)
    paused_by_us.clear()


def main() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", PORT))
    print(f"Listening on 127.0.0.1:{PORT}", flush=True)
    while True:
        data, _ = sock.recvfrom(64)
        cmd = data.decode(errors="ignore").strip()
        if cmd == "pause":
            pause()
        elif cmd == "play":
            play()


if __name__ == "__main__":
    main()
