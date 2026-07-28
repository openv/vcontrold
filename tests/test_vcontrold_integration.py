#!/usr/bin/env python3

import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import os


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


def wait_for_pidfile(path):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            text = path.read_text(encoding="ascii").strip()
            if text:
                return int(text)
        except (FileNotFoundError, ValueError):
            pass
        time.sleep(0.05)
    raise AssertionError(f"PID file was not created: {path}")


def main():
    vcontrold = str(pathlib.Path(sys.argv[1]).resolve())
    vsim = str(pathlib.Path(sys.argv[2]).resolve())
    source_xml = pathlib.Path(sys.argv[3]).resolve()
    control_port = free_port()

    version = subprocess.run(
        [vcontrold, "-V"], capture_output=True, text=True, timeout=5)
    if version.returncode != 0 or "vcontrold version" not in version.stdout:
        raise AssertionError(f"invalid -V result: {version}")
    help_result = subprocess.run(
        [vcontrold, "--help"], capture_output=True, text=True, timeout=5)
    if help_result.returncode == 0 or "usage: vcontrold" not in help_result.stdout:
        raise AssertionError(f"invalid --help result: {help_result}")
    invalid_port = subprocess.run(
        [vcontrold, "-p", "65536"], capture_output=True, text=True, timeout=5)
    if invalid_port.returncode != 2 or "Invalid TCP port" not in invalid_port.stderr:
        raise AssertionError(f"invalid port was accepted: {invalid_port}")
    option_probe = subprocess.run(
        [
            vcontrold,
            "-g", "-i", "-s", "-6", "-v",
            "-c", "unused", "-P", "unused",
            "-U", "unused", "-G", "unused", "-V",
        ],
        capture_output=True,
        text=True,
        timeout=5,
    )
    if option_probe.returncode != 0 or "vcontrold version" not in option_probe.stdout:
        raise AssertionError(f"short option parsing failed: {option_probe}")
    long_option_probe = subprocess.run(
        [
            vcontrold,
            "--debug", "--vsim", "--nodaemon", "--syslog",
            "--inet6", "--verbose", "--Version", "-V",
        ],
        capture_output=True,
        text=True,
        timeout=5,
    )
    if (
        long_option_probe.returncode != 0
        or "option Version" not in long_option_probe.stdout
        or "vcontrold version" not in long_option_probe.stdout
    ):
        raise AssertionError(f"long option parsing failed: {long_option_probe}")

    with tempfile.TemporaryDirectory(prefix="vcontrold-integration-") as directory:
        work = pathlib.Path(directory)
        config = work / "vcontrold.xml"
        shutil.copy2(source_xml / "vito.xml", work / "vito.xml")
        config_text = (source_xml / "vcontrold.xml").read_text(encoding="utf-8")
        config_text = config_text.replace(
            "<tty>/dev/ttySAC1</tty>", "<tty>127.0.0.1:6578</tty>")
        config_text = config_text.replace(
            "<config>", "<config>\n      <readonly>y</readonly>", 1)
        config.write_text(config_text, encoding="utf-8")
        valid_config = config.read_bytes()
        invalid_xml = work / "invalid.xml"
        invalid_xml.write_text("<V-Control>", encoding="utf-8")
        invalid_config = subprocess.run(
            [vcontrold, "-n", "-x", str(invalid_xml)],
            capture_output=True, text=True, timeout=5)
        if invalid_config.returncode == 0 or "Error loading" not in invalid_config.stderr:
            raise AssertionError(f"invalid XML was accepted: {invalid_config}")

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
                    "help": b"commands",
                    "version": b"Version:",
                    "device": b"ID=20CB",
                    "protocol": b"P300",
                    "commands": b"getTempA",
                    "detail getTempA": b"getTempA",
                    "detail getBetriebArt": b"Enum Bytes:",
                    "detail no-such-command": b"ERR: command",
                    "does-not-exist": b"ERR:",
                    "raw": b"raw mode disabled",
                    "close": b"127.0.0.1:6578 closed",
                }
                for request, expected in checks.items():
                    response = command(client, request)
                    if expected not in response:
                        raise AssertionError(
                            f"{request!r}: expected {expected!r}, got {response!r}")

                # This is a simulated read. It exercises the complete parser,
                # semaphore, socket-device, P300 framer and unit pipeline.
                command(client, "debug on")
                command(client, "debug off")
                command(client, "unit off")
                response = command(client, "getTempA")
                if b"07 01" not in response:
                    raise AssertionError(f"unexpected raw getTempA response: {response!r}")
                command(client, "unit on")
                response = command(client, "getTempA")
                if b"26.300000 Grad Celsius" not in response:
                    raise AssertionError(f"unexpected getTempA response: {response!r}")
                command(client, "unit off")
                response = command(client, "getTempA not-hex")
                if b"ERR:" not in response:
                    raise AssertionError(f"invalid hex parameter accepted: {response!r}")
                command(client, "unit on")
                if b"26.300000" not in command(client, "getTempA ignored"):
                    raise AssertionError("unit-bearing parameter path failed")
                command(client, "close")

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

            terminate(daemon, "vcontrold")
            if daemon.returncode != 0:
                raise AssertionError(
                    f"foreground vcontrold exited with {daemon.returncode}: "
                    f"{daemon.stderr.read()}")
            daemon = None

            # Exercise command-file mode without starting a listener. This is
            # only the P300 reset/read handshake, never a controller write.
            command_file = work / "commands.txt"
            command_file.write_text("SEND 04\nRECV 1", encoding="ascii")
            batch_port = free_port()
            batch = subprocess.Popen(
                [
                    vcontrold,
                    "-n",
                    "-4",
                    "-p", str(batch_port),
                    "-L", "127.0.0.1",
                    "-c", str(command_file),
                    "-d", "127.0.0.1:6578",
                    "-l", str(work / "batch.log"),
                    "-x", str(config),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            with connect_with_retry(batch_port, batch, "batch vcontrold"):
                pass
            terminate(batch, "batch vcontrold")
            if batch.returncode != 0:
                raise AssertionError(
                    f"command-file mode failed: {batch.stderr.read()}")

            # Start once using values exclusively from XML. This covers the
            # configuration-default path without touching the production port.
            defaults_port = free_port()
            defaults_config = work / "defaults.xml"
            defaults_text = valid_config.decode("utf-8")
            defaults_text = defaults_text.replace(
                "<port>3002</port>",
                f"<listen>127.0.0.1</listen><port>{defaults_port}</port>",
            )
            defaults_text = defaults_text.replace(
                "<file>vcontrold.log</file>",
                f"<file>{work / 'defaults.log'}</file>",
            )
            defaults_config.write_text(defaults_text, encoding="utf-8")
            defaults = subprocess.Popen(
                [vcontrold, "-n", "-x", str(defaults_config)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            with connect_with_retry(defaults_port, defaults, "XML-default vcontrold"):
                pass
            terminate(defaults, "XML-default vcontrold")
            if defaults.returncode != 0:
                raise AssertionError(
                    f"XML-default mode failed: {defaults.stderr.read()}")

            # Exercise the real daemon/PID/SIGHUP lifecycle on another
            # loopback-only port. The accepted session is metadata-only.
            daemon_port = free_port()
            pidfile = work / "vcontrold.pid"
            launcher = subprocess.run(
                [
                    vcontrold,
                    "-4",
                    "-p", str(daemon_port),
                    "-L", "127.0.0.1",
                    "-P", str(pidfile),
                    "-d", "127.0.0.1:6578",
                    "-l", str(work / "daemon.log"),
                    "-x", str(config),
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if launcher.returncode != 0:
                raise AssertionError(f"daemon launcher failed: {launcher.stderr}")
            daemon_pid = wait_for_pidfile(pidfile)
            try:
                os.kill(daemon_pid, signal.SIGHUP)
                time.sleep(0.1)
                with socket.create_connection(
                    ("127.0.0.1", daemon_port), timeout=5
                ) as daemon_client:
                    receive_until(daemon_client)
                    if b"Version:" not in command(daemon_client, "version"):
                        raise AssertionError("daemon session did not answer")
                    daemon_client.sendall(b"quit\n")
                    if b"good bye" not in daemon_client.recv(4096).lower():
                        raise AssertionError("daemon session did not close cleanly")
            finally:
                try:
                    os.kill(daemon_pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            deadline = time.monotonic() + 5
            while pidfile.exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            if pidfile.exists():
                raise AssertionError("daemon did not remove its PID file")
        finally:
            if daemon is not None:
                terminate(daemon, "vcontrold")
            terminate(simulator, "vsim")

        if simulator.returncode != 0:
            raise AssertionError(
                f"vsim exited with {simulator.returncode}: "
                f"{simulator.stderr.read()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
