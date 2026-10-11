#!/usr/bin/env python3
"""Load the QML of CRYPTARA with the Qt Quick that ships with PyQt6 (another
Qt version than the system one), with a stand-in for the C++ backend, and
render the window in each theme.

Any QML warning (unknown property, type error, missing import) is reported
and makes the script fail, which catches what differs between Qt versions.

    xvfb-run -a python3 qml_check.py ../../src OUTPUT_DIR
"""
import os
import re
import sys
from pathlib import Path

from PyQt6.QtCore import (QAbstractListModel, QByteArray, QModelIndex, QObject, Qt, QTimer,
                          QUrl, pyqtProperty, pyqtSignal, pyqtSlot, qInstallMessageHandler,
                          qVersion)
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine, qmlRegisterSingletonInstance
from PyQt6.QtQuick import QQuickWindow

SRC = Path(sys.argv[1]).resolve()
OUT = Path(sys.argv[2]).resolve()
OUT.mkdir(parents=True, exist_ok=True)
problems = []


def handler(mode, context, message):
    if "XDG_RUNTIME_DIR" in message or "propagateSizeHints" in message:
        return
    print("QML:", message)
    problems.append(message)


def catalogue():
    text = (SRC / "app" / "I18n.cpp").read_text(encoding="utf-8")
    entries = {}
    for match in re.finditer(r'\{"([a-z0-9_]+)",\s*((?:"(?:[^"\\]|\\.)*"\s*)+),\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}', text):
        def join(chunk):
            parts = re.findall(r'"((?:[^"\\]|\\.)*)"', chunk)
            return "".join(parts).encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
        entries[match.group(1)] = join(match.group(2))
    return entries


class Lines(QAbstractListModel):
    def __init__(self, roles, rows):
        super().__init__()
        self._roles = roles
        self._rows = rows

    def rowCount(self, parent=QModelIndex()):
        return len(self._rows)

    def data(self, index, role):
        column = role - Qt.ItemDataRole.UserRole - 1
        if 0 <= column < len(self._roles):
            return self._rows[index.row()][column]
        return None

    def roleNames(self):
        return {Qt.ItemDataRole.UserRole + 1 + i: QByteArray(r.encode()) for i, r in enumerate(self._roles)}

    @pyqtSlot(result=str)
    def plainText(self):
        return "\n".join(r[0] for r in self._rows)

    @pyqtSlot(int, int, result=str)
    def cell(self, row, column):
        return self._rows[row][column]

    @pyqtProperty(int, constant=True)
    def count(self):
        return len(self._rows)


class I18n(QObject):
    changed = pyqtSignal()

    def __init__(self):
        super().__init__()
        self._s = catalogue()

    @pyqtProperty("QVariantMap", notify=changed)
    def s(self):
        return self._s

    @pyqtProperty(str, notify=changed)
    def lang(self):
        return "en"


class Controller(QObject):
    stateChanged = pyqtSignal()
    transferChanged = pyqtSignal()
    preferencesChanged = pyqtSignal()
    messageRequested = pyqtSignal(int, str, str, str, str, bool, bool)
    passwordRequested = pyqtSignal(int, str, str)
    saveFileRequested = pyqtSignal(int, str, str, str)
    bbsWindowRequested = pyqtSignal(str)
    qsoLogRequested = pyqtSignal()
    stationFieldsRequested = pyqtSignal(str, str, str)

    def __init__(self):
        super().__init__()
        self._theme = "light"
        self._chat = Lines(["text", "kind"], [
            ["[14:02:11] W1AW: Good afternoon, signals 59 here.", "rx"],
            ["[14:02:40] N0CALL: Thanks, all good.", "tx"],
            ["[14:03:05] W1AW \U0001F512: Secure session established.", "rx"]])
        self._log = Lines(["text", "kind"], [
            ["[14:01:00] Connected to VARA HF (localhost:8300)", "success"],
            ["[14:01:00] CMD: VERSION 4.8.9", "debug"],
            ["[14:01:10] VARA error: WRONG", "error"],
            ["[14:01:12] File send timeout", "warning"]])
        self._cq = Lines(["callsign", "bandwidth", "via", "time"], [
            ["W1AW", "2300", "-", "14:00:00"], ["F4XYZ", "500", "-", "13:58:00"]])
        self._bbs = Lines(["name", "size"], [["report.pdf", "47.1 KB"]])
        self._qso = Lines(["callsign", "start", "end", "duration"], [["W1AW", "a", "b", "1m 2s"]])

    windowTitle = pyqtProperty(str, lambda self: "CRYPTARA - N0CALL", notify=stateChanged)
    myCall = pyqtProperty(str, lambda self: "N0CALL", notify=stateChanged)
    modemConnected = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    modemConnecting = pyqtProperty(bool, lambda self: False, notify=stateChanged)
    modemType = pyqtProperty(str, lambda self: "VARA HF", notify=stateChanged)
    sessionActive = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    remoteStation = pyqtProperty(str, lambda self: "W1AW", notify=stateChanged)
    ptt = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    cryptoOn = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    remoteBbs = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    digipeatersEnabled = pyqtProperty(bool, lambda self: False, notify=stateChanged)
    bandwidths = pyqtProperty("QStringList", lambda self: ["500 Hz", "2300 Hz"], notify=stateChanged)
    defaultBandwidthIndex = pyqtProperty(int, lambda self: 1, notify=stateChanged)
    bandwidthEnabled = pyqtProperty(bool, lambda self: True, notify=stateChanged)
    transferVisible = pyqtProperty(bool, lambda self: True, notify=transferChanged)
    transferBusy = pyqtProperty(bool, lambda self: True, notify=transferChanged)
    transferText = pyqtProperty(str, lambda self: "Sending report.pdf: 62% (412 B/s)", notify=transferChanged)
    transferState = pyqtProperty(str, lambda self: "active", notify=transferChanged)
    transferValue = pyqtProperty(float, lambda self: 62.0, notify=transferChanged)
    transferMaximum = pyqtProperty(float, lambda self: 100.0, notify=transferChanged)
    tooltips = pyqtProperty(bool, lambda self: True, notify=preferencesChanged)
    version = pyqtProperty(str, lambda self: "2.0.0", constant=True)
    cryptoBackend = pyqtProperty(str, lambda self: "stand-in", constant=True)

    @pyqtProperty(str, notify=preferencesChanged)
    def theme(self):
        return self._theme

    chatModel = pyqtProperty(QObject, lambda self: self._chat, constant=True)
    logModel = pyqtProperty(QObject, lambda self: self._log, constant=True)
    cqModel = pyqtProperty(QObject, lambda self: self._cq, constant=True)
    bbsModel = pyqtProperty(QObject, lambda self: self._bbs, constant=True)
    qsoModel = pyqtProperty(QObject, lambda self: self._qso, constant=True)

    @pyqtSlot(str)
    def setTheme(self, theme):
        self._theme = theme
        self.preferencesChanged.emit()

    @pyqtSlot(result="QVariantMap")
    def settingsMap(self):
        # Every field filled, as on a configured station: that is when the
        # Material 3 placeholders used to float over the field above.
        return {"modem_type": "VARA FM", "vara_hf_path": "C:\\VARA\\VARA.exe",
                "vara_fm_path": "C:/VARA FM/VARAFM.exe", "vara_sat_path": "C:\\VARA SAT\\VARASAT.exe",
                "auto_start_vara": True, "host": "localhost", "cmd_port": 8300,
                "data_port": 8301, "listen": True, "compression": True, "mycall": "N0CALL",
                "operator_name": "Smith", "operator_firstname": "John", "qth": "Springfield",
                "grid_square": "FN31PR", "rig": "Yaesu FT-891", "antenna": "EFHW",
                "auto_enabled": False, "auto_message": "", "auto_disconnect": True,
                "auto_delay": 3, "timestamp": True, "sound": False, "save_log": True,
                "file_rx_path": "C:\\Radio\\Received", "theme": self._theme, "language": "auto",
                "bbs_enabled": True, "bbs_folder": "C:\\Radio\\BBS"}

    @pyqtSlot(result="QStringList")
    def macros(self):
        return ["73 de {MYCALL}", "QTH {QTH}, locator {GRID}", "Rig {RIG}, antenna {ANTENNA}"] + [""] * 9

    @pyqtSlot()
    def demoQuestion(self):
        self.messageRequested.emit(1, "question", "Encryption request",
                                   "W1AW wants to establish an encrypted session.", "", True, True)

    @pyqtSlot()
    def loadDemo(self):
        pass

    @pyqtSlot(str, result=str)
    def toLocalPath(self, url):
        return QUrl(url).toLocalFile()

    @pyqtSlot(str, result=str)
    def toFileUrl(self, path):
        return QUrl.fromLocalFile(path).toString()

    @pyqtSlot(str, result=str)
    def folderUrlOf(self, path):
        return QUrl.fromLocalFile(os.path.dirname(path)).toString()

    # Actions: accepted and ignored.
    @pyqtSlot()
    def toggleModem(self): pass
    @pyqtSlot()
    def disconnectSession(self): pass
    @pyqtSlot()
    def clearCq(self): pass
    @pyqtSlot()
    def toggleCrypto(self): pass
    @pyqtSlot()
    def openBbs(self): pass
    @pyqtSlot()
    def bbsRefresh(self): pass
    @pyqtSlot()
    def saveChat(self): pass
    @pyqtSlot()
    def saveLog(self): pass
    @pyqtSlot()
    def showQsoLog(self): pass
    @pyqtSlot()
    def toggleTooltips(self): pass
    @pyqtSlot()
    def shutdown(self): pass
    @pyqtSlot(int)
    def sendMacro(self, n): pass
    @pyqtSlot(str)
    def sendFile(self, p): pass
    @pyqtSlot(str)
    def bbsDownload(self, p): pass
    @pyqtSlot(str)
    def testMacro(self, p): pass
    @pyqtSlot("QStringList")
    def saveMacros(self, p): pass
    @pyqtSlot("QVariantMap")
    def applySettings(self, p): pass
    @pyqtSlot(int, bool)
    def answer(self, i, y): pass
    @pyqtSlot(int, bool, str)
    def answerText(self, i, o, t): pass
    @pyqtSlot(int, str)
    def answerFile(self, i, p): pass
    @pyqtSlot(str, str, str, int)
    def callStation(self, *a): pass
    @pyqtSlot(str, str, int)
    def sendCq(self, *a): pass
    @pyqtSlot(int, int)
    def connectToCq(self, *a): pass
    @pyqtSlot(str, result=bool)
    def sendMessage(self, t): return True


class Ui(QObject):
    @pyqtSlot(QObject, str, result=bool)
    def grabWindow(self, window, path):
        return window.grabWindow().save(path)


def main():
    qInstallMessageHandler(handler)
    # What src/qml/qtquickcontrols2.conf says for the real program.
    os.environ["QT_QUICK_CONTROLS_MATERIAL_VARIANT"] = "Dense"
    os.environ["QT_QUICK_CONTROLS_STYLE"] = "Material"
    app = QGuiApplication(sys.argv[:1])
    controller, i18n, ui = Controller(), I18n(), Ui()
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "Controller", controller)
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "I18n", i18n)
    qmlRegisterSingletonInstance("CryptaraBackend", 1, 0, "Ui", ui)
    engine = QQmlApplicationEngine()
    engine.load(QUrl.fromLocalFile(str(SRC / "qml" / "Main.qml")))
    if not engine.rootObjects():
        print("FAILED: Main.qml did not load")
        sys.exit(1)
    root = engine.rootObjects()[0]
    root.screenshotMode(str(OUT))
    app.exec()
    # The about box and the message box, not in the C++ screenshot run.
    real = [p for p in problems if "qrc:/assets" not in p and "Cannot open: qrc" not in p]
    print("Qt", qVersion(), "-", "PASSED" if not real else "FAILED (%d QML warnings)" % len(real))
    sys.exit(1 if real else 0)


if __name__ == "__main__":
    main()
