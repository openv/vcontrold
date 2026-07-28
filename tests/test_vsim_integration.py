#!/usr/bin/env python3

import pathlib
import socket
import subprocess
import sys
import time


def connect_with_retry():
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", 6578), timeout=1)
        except OSError:
            time.sleep(0.05)
    raise AssertionError("vsim did not open port 6578")


def exchange(connection, request, expected):
    connection.sendall(request)
    received = bytearray()
    while len(received) < len(expected):
        chunk = connection.recv(len(expected) - len(received))
        if not chunk:
            raise AssertionError("vsim closed connection early")
        received.extend(chunk)
    if bytes(received) != expected:
        raise AssertionError(
            f"request {request.hex()}: {received.hex()} != {expected.hex()}")


def main():
    binary = str(pathlib.Path(sys.argv[1]).resolve())
    process = subprocess.Popen(
        [binary], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    try:
        with connect_with_retry() as connection:
            exchange(connection, b"\x04", b"\x05")
            exchange(connection, b"\xff\xaa\x04", b"\x05")
            exchange(
                connection,
                bytes.fromhex("41 05 00 01 55 25 02 82"),
                bytes.fromhex("06 41 07 01 01 55 25 02 07 01 8D"),
            )
            # A broken prefix followed immediately by a valid first byte must
            # resynchronize without requiring a reconnect.
            exchange(connection, b"\x16\x99\x04", b"\x05")
    finally:
        process.terminate()
        try:
            return_code = process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
            raise AssertionError("vsim did not terminate after SIGTERM")
    if return_code != 0:
        raise AssertionError(
            f"vsim exited with {return_code}: {process.stderr.read()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
