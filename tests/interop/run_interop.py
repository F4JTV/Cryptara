#!/usr/bin/env python3
"""The C++ version against the original Python version, through two
simulated VARA modems.

The Python station is the real MainWindow of cryptara.py (offscreen), with
its dialogs answered automatically; the C++ station is the engine of this
version in tests/headless.cpp.  The scenario covers everything that goes
over the air: CQ, call, chat both ways, encryption request with the
Diffie-Hellman exchange, encrypted chat both ways, encrypted files both
ways, BBS list and download, switching encryption off and on, the older
DHKEY exchange, hanging up, the auto-responder, and the settings file.

    QT_QPA_PLATFORM=offscreen python3 run_interop.py cryptara.py build/cryptara_headless

Needs PyQt6 and cryptography.  Exit code 0 when every check passed.
"""
import configparser
import importlib.util
import os
import random
import shutil
import sys
import tempfile
import time
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt6.QtCore import QProcess, QTimer  # noqa: E402
from PyQt6.QtWidgets import QApplication, QFileDialog, QInputDialog, QMessageBox  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fake_vara import make_pair  # noqa: E402

PASSWORD = "mot de passe partagé 73"
OUT = sys.__stdout__


def say(*parts):
    print(*parts, file=OUT, flush=True)


# ---------------------------------------------------------------- C++ side

class CppStation:
    def __init__(self, executable):
        self.lines = []
        self.state = {}
        self.proc = QProcess()
        self.proc.setProcessChannelMode(QProcess.ProcessChannelMode.SeparateChannels)
        self.proc.readyReadStandardOutput.connect(self._read)
        self.proc.readyReadStandardError.connect(
            lambda: [say("  [c++ stderr]", l) for l in bytes(self.proc.readAllStandardError()).decode(errors="replace").splitlines()])
        self._partial = b""
        self.proc.start(executable, [])

    def _read(self):
        self._partial += bytes(self.proc.readAllStandardOutput())
        while b"\n" in self._partial:
            line, _, self._partial = self._partial.partition(b"\n")
            text = line.decode("utf-8", errors="replace")
            self.lines.append(text)
            if text.startswith("STATE "):
                self.state = dict(kv.split("=", 1) for kv in text[6:].split())

    def send(self, command):
        self.proc.write((command + "\n").encode("utf-8"))

    def has(self, prefix, needle):
        return any(l.startswith(prefix) and needle in l for l in self.lines)

    def chat(self, needle):
        return self.has("CHAT ", needle)

    def log(self, needle):
        return self.has("LOG ", needle)


# ------------------------------------------------------------- Python side

class PythonStation:
    """cryptara.py's MainWindow, its modal dialogs answered automatically."""

    def __init__(self, module_path, home, modem, rx_dir, bbs_dir):
        os.environ["HOME"] = str(home)
        config_dir = Path(home) / ".config" / "CRYPTARA"
        config_dir.mkdir(parents=True, exist_ok=True)
        config = configparser.ConfigParser()
        config["CRYPTARA"] = {
            "modem_type": "VARA HF", "host": "127.0.0.1",
            "cmd_port": str(modem.cmd_port), "data_port": str(modem.data_port),
            "mycall": "N0PYT", "auto_start_vara": "False", "listen": "True",
            "compression": "True", "bbs_enabled": "True", "bbs_folder": str(bbs_dir),
            "auto_disconnect": "False", "file_rx_path": str(rx_dir), "timestamp": "True",
        }
        with open(config_dir / "cryptara_settings.conf", "w", encoding="utf-8") as f:
            config.write(f)

        self.boxes = []
        self.open_file = None
        self.save_dir = str(rx_dir)
        boxes = self.boxes
        station = self
        yes = QMessageBox.StandardButton.Yes
        ok = QMessageBox.StandardButton.Ok
        QMessageBox.question = staticmethod(lambda parent, title, text, *a, **k: boxes.append(("question", title, text)) or yes)
        QMessageBox.information = staticmethod(lambda parent, title, text, *a, **k: boxes.append(("information", title, text)) or ok)
        QMessageBox.warning = staticmethod(lambda parent, title, text, *a, **k: boxes.append(("warning", title, text)) or ok)
        QMessageBox.critical = staticmethod(lambda parent, title, text, *a, **k: boxes.append(("critical", title, text)) or ok)
        QMessageBox.exec = lambda box: boxes.append(("exec", box.windowTitle(), box.text())) or yes
        QInputDialog.getText = staticmethod(lambda *a, **k: (PASSWORD, True))
        QFileDialog.getOpenFileName = staticmethod(lambda *a, **k: (station.open_file, ""))
        QFileDialog.getSaveFileName = staticmethod(
            lambda parent, title, name, *a, **k: (os.path.join(station.save_dir, name), ""))

        spec = importlib.util.spec_from_file_location("cryptara_original", module_path)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.window = self.module.MainWindow()
        self.window.show()

    @property
    def vara(self):
        return self.window.vara

    def connected(self):
        return self.vara is not None and self.vara.connected

    def in_session(self):
        return self.vara is not None and self.vara.session_active

    def chat(self, needle):
        return needle in self.window.chat_display.toPlainText()

    def log(self, needle):
        return needle in self.window.log_display.toPlainText()

    def boxed(self, needle):
        return any(needle in (b[1] + " " + b[2]) for b in self.boxes)


# ----------------------------------------------------------------- scenario

def scenario(py, cpp, dirs, results):
    def check(name, condition):
        results.append((name, bool(condition)))
        say(("  ok   " if condition else "  FAIL ") + name)

    def wait(name, condition, timeout=15):
        yield ("wait", name, condition, timeout)

    # ---- link
    yield from wait("both stations linked to their modem",
                    lambda: py.connected() and cpp.state.get("modem") == "1")

    # ---- CQ, both ways
    cpp.send("cq")
    yield from wait("Python lists the CQ of the C++ station",
                    lambda: any(c["callsign"] == "N0CPP" for c in py.window.received_cqs))
    py.window.send_cq()
    yield from wait("C++ lists the CQ of the Python station", lambda: cpp.log("CQ received from N0PYT"))

    # ---- Python calls C++; the Python BBS beacon arrives
    py.window.station_edit.setText("N0CPP")
    py.window.call_station()
    yield from wait("session open on both sides",
                    lambda: py.in_session() and cpp.state.get("session") == "1")
    check("remote station seen by C++ is N0PYT", cpp.state.get("remote") == "N0PYT")
    check("remote station seen by Python is N0CPP", py.vara.remote_station == "N0CPP")
    yield from wait("C++ sees the Python BBS (BBSON)", lambda: cpp.state.get("bbs") == "1")

    # ---- clear chat
    py.window.msg_input.setText("Bonjour de Python éè \U0001F4FB")
    py.window.send_message()
    yield from wait("Python -> C++ clear text",
                    lambda: cpp.chat("rx ") and cpp.chat("N0PYT: Bonjour de Python éè \U0001F4FB"))
    cpp.send("send Hello from C++ ü")
    yield from wait("C++ -> Python clear text", lambda: py.chat("N0CPP: Hello from C++ ü"))

    # ---- encryption: request from C++, DH, Python accepts
    cpp.send("crypto")
    yield from wait("Diffie-Hellman session on both sides",
                    lambda: py.window.crypto.dh_established and cpp.log("DH session established"), 20)
    check("Python was asked to accept the request", py.boxed("souhaite") or py.boxed("wants to"))

    py.window.msg_input.setText("Message chiffré depuis Python")
    py.window.send_message()
    yield from wait("Python -> C++ encrypted (session key)",
                    lambda: cpp.chat("N0PYT \U0001F512: Message chiffré depuis Python"))
    cpp.send("send Encrypted from C++, with | pipes and <tags>")
    yield from wait("C++ -> Python encrypted (session key)",
                    lambda: py.chat("N0CPP \U0001F512: Encrypted from C++, with | pipes and <tags>"))
    check("Python decrypted without error", "[Message chiffré" not in py.window.chat_display.toPlainText())

    # ---- encrypted files, both ways
    payload = bytes(random.getrandbits(8) for _ in range(60000)) + b"CRYPTARA " * 8000
    py_file = Path(dirs["py_home"]) / "rapport été.bin"
    py_file.write_bytes(payload)
    py.open_file = str(py_file)
    QTimer.singleShot(0, py.window.send_file)   # blocks in its own loop until sent
    yield from wait("Python -> C++ encrypted file saved",
                    lambda: (Path(dirs["cpp_save"]) / py_file.name).exists()
                    and cpp.chat("File received: " + py_file.name), 60)
    check("file content identical (Python -> C++)",
          (Path(dirs["cpp_save"]) / py_file.name).read_bytes() == payload)

    cpp_payload = bytes(random.getrandbits(8) for _ in range(40000)) + b"73 " * 20000
    cpp_file = Path(dirs["cpp_home"]) / "from cpp ça.dat"
    cpp_file.write_bytes(cpp_payload)
    yield ("sleep", 1.0)
    cpp.send("file " + str(cpp_file))
    received = Path(dirs["py_rx"]) / cpp_file.name
    yield from wait("C++ -> Python encrypted file saved", lambda: received.exists() and py.log(str(received)), 60)
    check("file content identical (C++ -> Python)", received.exists() and received.read_bytes() == cpp_payload)
    yield from wait("C++ upload finished", lambda: cpp.chat("File sent: " + cpp_file.name), 20)

    # ---- BBS: list then download from the Python station
    cpp.send("bbs")
    yield from wait("C++ shows the Python BBS list", lambda: cpp.has("ROW ", "shared notes.txt"), 20)
    cpp.send("bbsget shared notes.txt")
    yield from wait("BBS file downloaded by C++",
                    lambda: (Path(dirs["cpp_save"]) / "shared notes.txt").exists()
                    and cpp.chat("File received: shared notes.txt"), 60)
    check("BBS file content identical",
          (Path(dirs["cpp_save"]) / "shared notes.txt").read_bytes() == dirs["bbs_content"])
    yield ("sleep", 1.0)

    # ---- Python switches encryption off, C++ agrees
    py.window.crypto_btn.setChecked(False)
    py.window.toggle_crypto()
    yield from wait("C++ follows the deactivation (CRYPTOFFACK)", lambda: py.boxed("galement désactivé"), 20)
    check("C++ encryption button off", cpp.state.get("crypto") == "0")
    cpp.send("send clear again")
    yield from wait("C++ -> Python clear after CRYPTOFF", lambda: py.chat("N0CPP: clear again"))

    # ---- C++ switches it back on, Python agrees (CRYPTON / CRYPTONACK)
    cpp.send("crypto")
    yield from wait("Python follows the reactivation (CRYPTONACK)", lambda: cpp.log("has also reactivated"), 20)
    check("Python encryption enabled again", py.window.crypto.enabled)
    cpp.send("send encrypted once more")
    yield from wait("C++ -> Python encrypted after CRYPTON",
                    lambda: py.chat("N0CPP \U0001F512: encrypted once more"))
    py.window.msg_input.setText("reçu")
    py.window.send_message()
    yield from wait("Python -> C++ encrypted after CRYPTON", lambda: cpp.chat("N0PYT \U0001F512: reçu"))

    # ---- hang up from C++
    cpp.send("hangup")
    yield from wait("session closed on both sides",
                    lambda: not py.in_session() and cpp.state.get("session") == "0")

    # ---- the older DHKEY exchange, started by Python in a new session
    cpp_lines_before = len(cpp.lines)
    py.window.station_edit.setText("N0CPP")
    py.window.call_station()
    yield from wait("second session open", lambda: py.in_session() and cpp.state.get("session") == "1")
    yield ("sleep", 1.0)
    # Python keeps its old session key across sessions; the new one must
    # come from this exchange.
    old_key = py.window.crypto.session_key
    py.window.initiate_dh_exchange()
    yield from wait("C++ answered the DHKEY exchange",
                    lambda: any("Diffie-Hellman session established" in l for l in cpp.lines[cpp_lines_before:]), 20)
    yield from wait("Python computed a new session from the C++ DHKEY",
                    lambda: py.window.crypto.session_key not in (None, old_key), 20)
    cpp.send("crypto")   # password known: CRYPTON, Python agrees
    yield from wait("encryption back on for the DHKEY session", lambda: py.window.crypto.enabled, 20)
    yield ("sleep", 1.0)
    cpp.send("send after DHKEY")
    yield from wait("C++ -> Python encrypted with the DHKEY session",
                    lambda: py.chat("N0CPP \U0001F512: after DHKEY"))
    check("Python decrypted the DHKEY-session message",
          "[Message chiffré" not in py.window.chat_display.toPlainText())
    py.window.disconnect_session()
    yield from wait("second session closed", lambda: not py.in_session() and cpp.state.get("session") == "0")

    # ---- auto-responder on the C++ station
    for command in ("set auto_message Auto-reply from {MYCALL} to {CALLSIGN}, {DATE}",
                    "set auto_delay 1", "set auto_disconnect 1", "set auto_enabled 1"):
        cpp.send(command)
    yield ("sleep", 0.5)
    py.window.call_station()
    yield from wait("Python receives the C++ auto-reply",
                    lambda: py.chat("N0CPP: Auto-reply from N0CPP to N0PYT, " + time.strftime("%Y-%m-%d")), 20)
    yield from wait("the C++ station hangs up after its auto-reply",
                    lambda: not py.in_session() and cpp.state.get("session") == "0", 20)

    # ---- settings written by C++, read by Python's configparser
    cpp.send("set macro_f1 100% sure, de {MYCALL}")
    yield ("sleep", 0.5)
    parser = configparser.ConfigParser()
    parser.read(dirs["cpp_settings"], encoding="utf-8")
    check("Python reads the C++ settings file (percent sign)",
          parser.get("CRYPTARA", "macro_f1") == "100% sure, de {MYCALL}")
    check("Python reads the C++ settings file (multi-line value)",
          parser.get("CRYPTARA", "auto_message").startswith("Auto-reply from"))
    check("C++ read the Python-written multi-line value",
          dirs["multiline_checked"]())


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    module_path, headless = sys.argv[1], sys.argv[2]
    app = QApplication(sys.argv[:1])
    random.seed(73)

    root = Path(tempfile.mkdtemp(prefix="cryptara-interop-"))
    dirs = {name: root / name for name in ("py_home", "py_rx", "py_bbs", "cpp_home", "cpp_save")}
    for d in dirs.values():
        d.mkdir(parents=True)
    dirs["bbs_content"] = ("Notes partagées\n" * 500).encode("utf-8")
    (dirs["py_bbs"] / "shared notes.txt").write_bytes(dirs["bbs_content"])

    modem_py, modem_cpp = make_pair()

    # The C++ settings, written by Python's configparser: the C++ version
    # must read what the Python version writes.
    cpp_settings = dirs["cpp_home"] / "cryptara_settings.conf"
    config = configparser.ConfigParser()
    config["CRYPTARA"] = {
        "modem_type": "VARA HF", "host": "127.0.0.1",
        "cmd_port": str(modem_cpp.cmd_port), "data_port": str(modem_cpp.data_port),
        "mycall": "N0CPP", "auto_start_vara": "False", "language": "en", "timestamp": "True",
        "auto_message": "line one\nline two, with a comma",
        "macro_f2": "rapport 100%% ok",
    }
    with open(cpp_settings, "w", encoding="utf-8") as f:
        config.write(f)
    dirs["cpp_settings"] = str(cpp_settings)

    log_file = open(root / "python-stdout.log", "w", encoding="utf-8")
    sys.stdout = log_file   # the Python version prints its [DEBUG] lines

    cpp = CppStation(headless)
    if not cpp.proc.waitForStarted(5000):
        say("FAILED: cannot start", headless)
        sys.exit(2)
    cpp.send("start " + str(cpp_settings))
    cpp.send("password " + PASSWORD)
    cpp.send("savedir " + str(dirs["cpp_save"]))

    # What the C++ side read from the Python-written file, checked before
    # the scenario changes it.
    multiline = {}
    def ask_settings():
        multiline["ok"] = True
    dirs["multiline_checked"] = lambda: multiline.get("ok", False)

    py = PythonStation(module_path, dirs["py_home"], modem_py, dirs["py_rx"], dirs["py_bbs"])
    cpp.send("connect")

    results = []
    steps = scenario(py, cpp, dirs, results)
    state = {"pending": None, "deadline": 0.0, "done": False}

    def advance():
        if state["done"]:
            return
        pending = state["pending"]
        if pending is not None:
            kind = pending[0]
            if kind == "wait":
                _, name, condition, _ = pending
                try:
                    satisfied = condition()
                except Exception as exc:  # noqa: BLE001
                    satisfied = False
                    pending = ("wait", name + " (%r)" % exc, condition, 0)
                if satisfied:
                    results.append((name, True))
                    say("  ok   " + name)
                elif time.monotonic() > state["deadline"]:
                    results.append((name, False))
                    say("  FAIL " + name + " (timed out)")
                    say("  ---- last C++ lines:")
                    for line in cpp.lines[-25:]:
                        say("     ", line[:200])
                    say("  ---- Python chat:")
                    for line in py.window.chat_display.toPlainText().splitlines()[-8:]:
                        say("     ", line[:200])
                    say("  ---- Python log:")
                    for line in py.window.log_display.toPlainText().splitlines()[-12:]:
                        say("     ", line[:200])
                    finish()
                    return
                else:
                    return
            elif kind == "sleep":
                if time.monotonic() < state["deadline"]:
                    return
        try:
            step = next(steps)
        except StopIteration:
            finish()
            return
        state["pending"] = step
        state["deadline"] = time.monotonic() + (step[3] if step[0] == "wait" else step[1])

    def finish():
        state["done"] = True
        cpp.send("quit")
        cpp.proc.waitForFinished(5000)
        failed = [name for name, ok in results if not ok]
        say("%s: %d checks, %d failed" % ("PASSED" if not failed else "FAILED", len(results), len(failed)))
        app.exit(1 if failed else 0)

    # The C++ side must have read the multi-line value as configparser does:
    # it writes the file back on the next settings change.
    def check_multiline():
        text = open(cpp_settings, encoding="utf-8").read()
        if "auto_message = line one\n\tline two, with a comma\n" in text and "macro_f2 = rapport 100%% ok\n" in text:
            ask_settings()
        else:
            say("  C++ wrote back:", repr(text[:400]))
    QTimer.singleShot(800, lambda: cpp.send("set auto_delay 3"))
    QTimer.singleShot(1800, check_multiline)

    timer = QTimer()
    timer.timeout.connect(advance)
    timer.start(50)
    code = app.exec()
    sys.stdout = OUT
    shutil.rmtree(root, ignore_errors=True)
    sys.exit(code)


if __name__ == "__main__":
    main()
