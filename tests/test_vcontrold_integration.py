#!/usr/bin/env python3

import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time


PROMPT = b"vctrld>"


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def connect_with_retry(port, process, name):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            stderr = process.stderr.read()
            raise AssertionError(f"{name} exited early ({process.returncode}): {stderr}")
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.05)
    raise AssertionError(f"{name} did not open port {port}")


def receive_until(connection, marker=PROMPT):
    received = bytearray()
    connection.settimeout(5)
    while marker not in received:
        chunk = connection.recv(4096)
        if not chunk:
            raise AssertionError(
                f"connection closed before {marker!r}: {bytes(received)!r}")
        received.extend(chunk)
    return bytes(received)


def command(connection, text):
    connection.sendall(text.encode("ascii") + b"\n")
    return receive_until(connection)


def terminate(process, name):
    if process.poll() is not None:
        return
    process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
        raise AssertionError(f"{name} did not terminate after SIGTERM")


def main():
    vcontrold = str(pathlib.Path(sys.argv[1]).resolve())
    vsim = str(pathlib.Path(sys.argv[2]).resolve())
    source_xml = pathlib.Path(sys.argv[3]).resolve()
    control_port = free_port()

    version = subprocess.run(
        [vcontrold, "-V"], capture_output=True, text=True, timeout=5)
    if version.returncode != 0 or "vcontrold version" not in version.stdout:
        raise AssertionError(f"invalid -V result: {version}")

    with tempfile.TemporaryDirectory(prefix="vcontrold-integration-") as directory:
        work = pathlib.Path(directory)
        config = work / "vcontrold.xml"
        shutil.copy2(source_xml / "vito.xml", work / "vito.xml")
        config_text = (source_xml / "vcontrold.xml").read_text(encoding="utf-8")
        config_text = config_text.replace(
            "<tty>/dev/ttySAC1</tty>", "<tty>127.0.0.1:6578</tty>")
        config.write_text(config_text, encoding="utf-8")
        valid_config = config.read_bytes()

        simulator = subprocess.Popen(
            [vsim], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        daemon = None
        try:
            # Ensure the simulator is ready, then release the probe connection.
            with connect_with_retry(6578, simulator, "vsim"):
                pass

            daemon = subprocess.Popen(
                [
                    vcontrold,
                    "-n",
                    "-4",
                    "-p", str(control_port),
                    "-L", "127.0.0.1",
                    "-d", "127.0.0.1:6578",
                    "-l", str(work / "vcontrold.log"),
                    "-x", str(config),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            with connect_with_retry(control_port, daemon, "vcontrold") as client:
                receive_until(client)

                # Empty and whitespace-only commands must be harmless.
                if PROMPT not in command(client, ""):
                    raise AssertionError("empty command did not return the prompt")
                if PROMPT not in command(client, "   "):
                    raise AssertionError("whitespace command did not return the prompt")

                checks = {
                    "version": b"Version:",
                    "device": b"ID=20CB",
                    "protocol": b"P300",
                    "commands": b"getTempA",
                    "detail getTempA": b"getTempA",
                    "does-not-exist": b"ERR:",
                }
                for request, expected in checks.items():
                    response = command(client, request)
                    if expected not in response:
                        raise AssertionError(
                            f"{request!r}: expected {expected!r}, got {response!r}")

                # This is a simulated read. It exercises the complete parser,
                # semaphore, socket-device, P300 framer and unit pipeline.
                response = command(client, "getTempA")
                if b"26.300000 Grad Celsius" not in response:
                    raise AssertionError(f"unexpected getTempA response: {response!r}")

                # A failed reload must preserve the active object graph.
                config.write_text("<V-Control>", encoding="utf-8")
                response = command(client, "reload")
                if b"failed, using old configuration" not in response:
                    raise AssertionError(f"invalid reload was not rejected: {response!r}")
                if b"getTempA" not in command(client, "commands"):
                    raise AssertionError("failed reload replaced the active configuration")

                # Restore a valid file and prove that a subsequent reload works.
                config.write_bytes(valid_config)
                response = command(client, "reload")
                if b"reloaded" not in response:
                    raise AssertionError(f"valid reload failed: {response!r}")
                if b"26.300000 Grad Celsius" not in command(client, "getTempA"):
                    raise AssertionError("read failed after successful reload")

                client.sendall(b"quit\n")
                goodbye = client.recv(4096)
                if b"good bye" not in goodbye.lower():
                    raise AssertionError(f"missing goodbye response: {goodbye!r}")
        finally:
            if daemon is not None:
                terminate(daemon, "vcontrold")
            terminate(simulator, "vsim")

        if daemon is None or daemon.returncode != 0:
            error = "" if daemon is None else daemon.stderr.read()
            raise AssertionError(
                f"vcontrold exited with "
                f"{None if daemon is None else daemon.returncode}: {error}")
        if simulator.returncode != 0:
            raise AssertionError(
                f"vsim exited with {simulator.returncode}: "
                f"{simulator.stderr.read()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
