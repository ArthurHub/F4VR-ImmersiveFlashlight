"""The running game, through devbench's REST port: its tools, its events and its files.

One kept-alive connection serves every call: a connection per call uses up the local ports in seconds when a test
polls. Kept close to Mods Config's tests/e2e/devbench.py, so a fix carries over.
"""

import base64
import http.client
import json
import os
import re
import time
from pathlib import Path

DEFAULT_PORT = 8931

# the mod's devbench tool, and the prefix of its events
MOD_TOOL = "immersiveflashlightvr"

# the number a console command prints after its '>>', as GetItemCount and GetInCell print it
NUMBER = re.compile(r">>\s*(-?\d+(?:\.\d+)?)")


class DevbenchError(Exception):
    """A call that devbench or a tool refused, with the error it gave."""


def find_port():
    """The port devbench bound, from the runtime.json it writes, or its Fallout 4 VR default."""
    local = os.environ.get("LOCALAPPDATA")
    runtime = Path(local) / "devbench" / "fallout4vr" / "runtime.json" if local else None
    if runtime and runtime.is_file():
        try:
            return int(json.loads(runtime.read_text(encoding="utf-8"))["port"])
        except (ValueError, KeyError):
            pass
    return DEFAULT_PORT


def wait_until(check, timeout, interval=0.1, what="the condition"):
    """Call check until it returns a true value, and return that value; fail after timeout seconds."""
    deadline = time.monotonic() + timeout
    while True:
        value = check()
        if value:
            return value
        if time.monotonic() > deadline:
            raise TimeoutError(f"{what} did not hold within {timeout} s")
        time.sleep(interval)


class Devbench:
    """A client of devbench's REST port."""

    def __init__(self, port):
        self.port = port
        self._connection = None

    def close(self):
        if self._connection:
            self._connection.close()
            self._connection = None

    def _request(self, method, path, body=None):
        """Send one request on the kept-alive connection, opening it again once when the game closed it."""
        payload = json.dumps(body).encode("utf-8") if body is not None else None
        headers = {"Content-Type": "application/json"} if payload is not None else {}
        for attempt in range(2):
            if not self._connection:
                self._connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=60)
            try:
                self._connection.request(method, path, body=payload, headers=headers)
                response = self._connection.getresponse()
                return response.status, json.loads(response.read() or b"{}")
            except (ConnectionError, http.client.HTTPException, OSError):
                self.close()
                if attempt == 1:
                    raise
        raise AssertionError("unreachable")

    def tool(self, tool_name, /, **args):
        """Call a tool with its arguments, and return its answer; a refusal raises DevbenchError."""
        status, answer = self._request("POST", f"/api/tool/{tool_name}", args)
        if status != 200 or answer.get("ok") is False:
            raise DevbenchError(f"{tool_name} {args.get('action', '')}: {answer.get('error', answer)}")
        return answer

    def mod(self, action, **args):
        """Call an action of the mod's own devbench tool."""
        return self.tool(MOD_TOOL, action=action, **args)

    def head_seq(self):
        """The sequence number of the last event devbench published."""
        status, answer = self._request("GET", "/api/events?since=0")
        return answer["headSeq"]

    def events_since(self, seq):
        """The events published after seq, oldest first, each with its seq, frame, topic and data."""
        status, answer = self._request("GET", f"/api/events?since={seq}")
        return answer["events"]

    def console(self, command):
        """Run a console command and return the lines it printed; one that does not compile raises DevbenchError."""
        answer = self.tool("console", command=command)
        if not answer.get("compiled", True):
            raise DevbenchError(f"console: '{command}' did not compile: {answer.get('lines')}")
        return answer.get("lines", [])

    def console_number(self, command):
        """The number a console command prints after its '>>'."""
        for line in reversed(self.console(command)):
            if match := NUMBER.search(line):
                return float(match.group(1))
        raise AssertionError(f"'{command}' printed no number")

    def open_menus(self):
        return self.tool("menu", action="list")["openMenus"]

    def list_files(self, root, glob):
        return self.tool("files", action="list", root=root, path=glob)["files"]

    def read_bytes(self, root, path):
        """The bytes of a file as the game sees it, or None when there is none."""
        try:
            answer = self.tool("files", action="read", root=root, path=path, encoding="base64")
        except DevbenchError as error:
            if "no file" in str(error):
                return None
            raise
        return base64.b64decode(answer["content"])

    def write_bytes(self, root, path, data):
        self.tool("files", action="write", root=root, path=path, encoding="base64", content=base64.b64encode(data).decode("ascii"))

    def delete_file(self, root, path):
        return self.tool("files", action="delete", root=root, path=path)["deleted"]


class Events:
    """The events devbench publishes from now on: start one before an action, and read what the action caused."""

    def __init__(self, bench):
        self.bench = bench
        self.start = bench.head_seq()
        self.seen = []

    def _read(self):
        last = self.seen[-1]["seq"] if self.seen else self.start
        self.seen.extend(self.bench.events_since(last))
        return self.seen

    def all(self, topic=None):
        """The events since the start, of one topic or of all; a topic of the mod is named without its prefix."""
        return [event for event in self._read() if topic is None or event["topic"] in (topic, f"{MOD_TOOL}.{topic}")]

    def wait(self, topic, timeout=5.0, where=None):
        """The first event of a topic since the start whose data where accepts, waiting up to timeout seconds."""

        def found():
            return next((event for event in self.all(topic) if where is None or where(event.get("data"))), None)

        return wait_until(found, timeout, what=f"the event {topic}")
