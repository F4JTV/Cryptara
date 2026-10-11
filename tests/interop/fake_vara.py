"""Two simulated VARA modems linked by a perfect radio path (PyQt6).

Same behaviour as tests/FakeVara.h: each modem listens on a command port and
a data port, answers MYCALL, VERSION, LISTEN, CHAT, COMPRESSION, BW, connects
two stations when one calls the other's MYCALL, relays CQFRAME, and moves
data to the other side a few kilobytes at a time while reporting BUFFER and
PTT.
"""
from PyQt6.QtCore import QObject, QTimer
from PyQt6.QtNetwork import QHostAddress, QTcpServer


class FakeModem(QObject):
    def __init__(self, name):
        super().__init__()
        self.name = name
        self.peer = None
        self.mycall = ""
        self.session = False
        self.ptt = False
        self.buffer = bytearray()
        self.lines = bytearray()
        self.commands = []
        self.cmd = None
        self.dat = None
        self.cmd_server = QTcpServer()
        self.dat_server = QTcpServer()
        self.cmd_server.listen(QHostAddress(QHostAddress.SpecialAddress.LocalHost), 0)
        self.dat_server.listen(QHostAddress(QHostAddress.SpecialAddress.LocalHost), 0)
        self.cmd_server.newConnection.connect(self._new_cmd)
        self.dat_server.newConnection.connect(self._new_dat)
        self.air = QTimer()
        self.air.setInterval(20)
        self.air.timeout.connect(self._transmit)

    @property
    def cmd_port(self):
        return self.cmd_server.serverPort()

    @property
    def data_port(self):
        return self.dat_server.serverPort()

    def say(self, line):
        if self.cmd is not None:
            self.cmd.write((line + "\r").encode())

    def deliver(self, data):
        if self.dat is not None:
            self.dat.write(bytes(data))

    def _new_cmd(self):
        self.cmd = self.cmd_server.nextPendingConnection()
        self.cmd.readyRead.connect(self._read_cmd)

    def _new_dat(self):
        self.dat = self.dat_server.nextPendingConnection()
        self.dat.readyRead.connect(self._read_dat)

    def _read_cmd(self):
        self.lines += bytes(self.cmd.readAll())
        while b"\r" in self.lines:
            line, _, rest = bytes(self.lines).partition(b"\r")
            self.lines = bytearray(rest)
            text = line.decode(errors="replace").strip()
            if text:
                self._handle(text)

    def _handle(self, line):
        self.commands.append(line)
        parts = line.split()
        verb = parts[0] if parts else ""
        if verb == "MYCALL" and len(parts) > 1:
            self.mycall = parts[1]
            self.say("OK")
        elif verb == "VERSION":
            self.say("VERSION 4.8.9-fake")
        elif verb == "CONNECT" and len(parts) >= 3:
            source, destination = parts[1], parts[2]
            self.say("OK")
            if self.peer and self.peer.mycall.upper() == destination.upper():
                def connected():
                    message = "CONNECTED %s %s 2300" % (source, destination)
                    self.session = self.peer.session = True
                    self.say(message)
                    self.peer.say(message)
                QTimer.singleShot(150, connected)
        elif verb in ("DISCONNECT", "ABORT"):
            self.say("OK")
            if self.session:
                def disconnected():
                    self.session = False
                    self.buffer.clear()
                    self.say("DISCONNECTED")
                    if self.peer:
                        self.peer.session = False
                        self.peer.say("DISCONNECTED")
                QTimer.singleShot(100 if not self.buffer else 600, disconnected)
        elif verb == "CQFRAME":
            self.say("OK")
            if self.peer and not self.peer.session:
                self.peer.say(line)
        else:
            self.say("OK")

    def _read_dat(self):
        data = bytes(self.dat.readAll())
        if not self.session:
            return
        self.buffer += data
        self.say("BUFFER %d" % len(self.buffer))
        if not self.air.isActive():
            self.air.start()

    def _transmit(self):
        if not self.buffer or not self.session:
            self.air.stop()
            if self.ptt:
                self.ptt = False
                self.say("PTT OFF")
            return
        if not self.ptt:
            self.ptt = True
            self.say("PTT ON")
        frame = self.buffer[:6000]
        del self.buffer[:6000]
        if self.peer:
            self.peer.deliver(frame)
        self.say("BUFFER %d" % len(self.buffer))


def make_pair():
    a, b = FakeModem("A"), FakeModem("B")
    a.peer, b.peer = b, a
    return a, b
