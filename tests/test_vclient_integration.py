#!/usr/bin/env python3

import json
import pathlib
import socket
import subprocess
import sys
import tempfile
import threading


PROMPT = b"vctrld>"
BYE = b"good bye!\n"


class FakeServer:
    def __init__(self):
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]
        self.error = None
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def _serve(self):
        try:
            connection, _ = self.listener.accept()
            with connection:
                connection.sendall(PROMPT)
                stream = connection.makefile("rb")
                for raw_command in stream:
                    command = raw_command.rstrip(b"\r\n").decode()
                    if command == "quit":
                        connection.sendall(BYE)
                        return
                    responses = {
                        "temp": b"12.5 C\n",
                        "bad": b"ERR: unavailable\n",
                        "quoted": b"7.0 quote\\\"\\\\tab\\t\n",
                    }
                    connection.sendall(responses.get(
                        command, b"ERR: command unknown\n") + PROMPT)
        except Exception as error:  # surfaced in finish()
            self.error = error
        finally:
            self.listener.close()

    def start(self):
        self.thread.start()

    def finish(self):
        self.thread.join(timeout=5)
        if self.thread.is_alive():
            raise AssertionError("fake server did not terminate")
        if self.error:
            raise self.error


def invoke(binary, arguments, expected, cwd=None):
    server = FakeServer()
    server.start()
    completed = subprocess.run(
        [binary, "-h", f"127.0.0.1:{server.port}", *arguments],
        cwd=cwd,
        text=True,
        capture_output=True,
        timeout=10,
        check=False,
    )
    server.finish()
    if completed.returncode != expected:
        raise AssertionError(
            f"{arguments}: rc={completed.returncode}, "
            f"stdout={completed.stdout!r}, stderr={completed.stderr!r}")
    return completed


def main():
    binary = str(pathlib.Path(sys.argv[1]).resolve())

    plain = invoke(binary, ["-c", "temp"], 0)
    assert "temp:\n12.5 C" in plain.stdout

    short_json = invoke(binary, ["-j", "-c", "temp,bad"], 3)
    assert json.loads(short_json.stdout) == {"temp": 12.5, "bad": None}

    long_json = invoke(binary, ["-J", "-c", "quoted,bad"], 3)
    parsed = json.loads(long_json.stdout)
    assert parsed[0]["command"] == "quoted"
    assert parsed[0]["value"] == 7.0
    assert parsed[0]["error"] is None
    assert parsed[1]["value"] is None
    assert parsed[1]["error"].startswith("ERR:")

    munin = invoke(binary, ["-m", "-c", "temp,bad"], 3)
    assert "temp.value 12.500000" in munin.stdout
    assert "bad.value U" in munin.stdout

    cacti = invoke(binary, ["-k", "-c", "temp,bad"], 3)
    assert cacti.stdout == "v1:12.500000 v2:U \n"

    verbose = invoke(binary, ["-v", "-c", "temp"], 0)
    assert "Host: 127.0.0.1 Port:" in verbose.stdout

    positional = invoke(binary, ["temp"], 0)
    assert "12.5 C" in positional.stdout

    with tempfile.TemporaryDirectory() as directory:
        directory = pathlib.Path(directory)
        command_file = directory / "commands.txt"
        command_file.write_text("temp\n", encoding="utf-8")
        from_file = invoke(binary, ["-f", str(command_file)], 0)
        assert "12.5 C" in from_file.stdout

        csv_file = directory / "values.csv"
        invoke(binary, ["-s", str(csv_file), "-c", "temp"], 0)
        assert csv_file.read_text(encoding="utf-8") == "12.500000\n"

        template = directory / "template.txt"
        template.write_text(
            "$C1=$1 raw=$R1 err=$E1; $C2 err=$E2; escaped=\\$1\n",
            encoding="utf-8",
        )
        rendered = invoke(
            binary, ["-t", str(template), "-c", "temp,bad"], 3)
        assert "temp=12.500000 raw=12.5 C err=OK" in rendered.stdout
        assert "bad err=ERR: unavailable" in rendered.stdout
        assert "escaped=$1" in rendered.stdout

        output = directory / "output.txt"
        invoke(binary, ["-o", str(output), "-c", "temp"], 0)
        assert "temp:\n12.5 C" in output.read_text(encoding="utf-8")

    version = subprocess.run(
        [binary, "-V"], text=True, capture_output=True, timeout=5, check=False)
    assert version.returncode == 0 and "vclient" in version.stdout

    invalid_port = subprocess.run(
        [binary, "-h", "127.0.0.1:70000", "-c", "temp"],
        text=True, capture_output=True, timeout=5, check=False)
    assert invalid_port.returncode == 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
