// Main.qml - CRYPTARA's main window.  Same layout as the Python version:
// toolbar, chat (left) and received CQs (right) over the system log,
// status bar; File, Tools and Help menus; F1-F12 send the macros.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import CryptaraBackend

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    minimumWidth: 1000
    minimumHeight: 560
    visible: true
    title: Controller.windowTitle

    readonly property var s: I18n.s
    readonly property string monoFamily: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"

    // ---- themes -----------------------------------------------------------
    // AX25Chat's four looks, same Material palettes and status colours:
    // light and dark for daytime, red for the night (every colour a shade of
    // red, so the eyes keep their dark adaptation), amber for dim light.
    // surface, field, button, line, selection and lit are what the desktop
    // panels, lists and raised buttons need on top.
    readonly property var themes: ({
        "light": { "material": Material.Light, "primary": "#3f51b5", "accent": "#e91e63",
                   "background": "#fafafa", "foreground": "#212121",
                   "surface": "#ffffff", "field": "#fbfbfc", "button": "#e8eaf0", "buttonText": "#212121",
                   "line": "#d0d7de", "selection": "#c5cae9", "lit": "#1a7f37", "onLit": "#ffffff",
                   "tip": "#323232", "tipText": "#ffffff", "strong": "#3f51b5", "onStrong": "#ffffff",
                   "colours": { "tx": "#1a7f37", "rx": "#0969da", "beacon": "#9a6700", "error": "#b32626",
                                "system": "#57606a", "monitor": "#7c3aed", "dim": "#6e7681", "clear": "#1a7f37",
                                "busy": "#9a6700", "transmitting": "#b32626", "offline": "#6e7681",
                                "heading": "#3f51b5" } },
        "dark":  { "material": Material.Dark, "primary": "#263238", "accent": "#26c6da",
                   "background": "#121212", "foreground": "#e6edf3",
                   "surface": "#1c1f23", "field": "#16181b", "button": "#2d333b", "buttonText": "#e6edf3",
                   "line": "#30363d", "selection": "#1f4f58", "lit": "#2ea043", "onLit": "#ffffff",
                   "tip": "#3d444d", "tipText": "#e6edf3", "strong": "#1f6f7a", "onStrong": "#ffffff",
                   "colours": { "tx": "#3fb950", "rx": "#58a6ff", "beacon": "#d29922", "error": "#f85149",
                                "system": "#9198a1", "monitor": "#a371f7", "dim": "#8b949e", "clear": "#3fb950",
                                "busy": "#d29922", "transmitting": "#f85149", "offline": "#8b949e",
                                "heading": "#26c6da" } },
        "red":   { "material": Material.Dark, "primary": "#2b0000", "accent": "#ff2a2a",
                   "background": "#0a0000", "foreground": "#ff3b3b",
                   "surface": "#140000", "field": "#0f0000", "button": "#2b0000", "buttonText": "#ff4a4a",
                   "line": "#3a0a0a", "selection": "#4a0c0c", "lit": "#c41414", "onLit": "#0a0000",
                   "tip": "#2b0000", "tipText": "#ff4a4a", "strong": "#5c0000", "onStrong": "#ff7070",
                   "colours": { "tx": "#ff7070", "rx": "#ff4d4d", "beacon": "#d63c3c", "error": "#ff1a1a",
                                "system": "#b03636", "monitor": "#8f2d2d", "dim": "#7a2424", "clear": "#ff5252",
                                "busy": "#c92e2e", "transmitting": "#ff1a1a", "offline": "#5c1f1f",
                                "heading": "#ff5a5a" } },
        "amber": { "material": Material.Dark, "primary": "#3d2a00", "accent": "#ffb000",
                   "background": "#100c00", "foreground": "#ffb84d",
                   "surface": "#1a1400", "field": "#140f00", "button": "#3d2a00", "buttonText": "#ffc266",
                   "line": "#3d2f10", "selection": "#4d3a10", "lit": "#ffb000", "onLit": "#100c00",
                   "tip": "#3d2a00", "tipText": "#ffc266", "strong": "#6b4a00", "onStrong": "#ffd27a",
                   "colours": { "tx": "#ffd166", "rx": "#ffb000", "beacon": "#e69500", "error": "#ff6a00",
                                "system": "#b88a2e", "monitor": "#a07a30", "dim": "#8a6a2a", "clear": "#ffd166",
                                "busy": "#e69500", "transmitting": "#ff6a00", "offline": "#5c4a1f",
                                "heading": "#ffb000" } }
    })
    readonly property string themeName: themes[Controller.theme] ? Controller.theme : "light"
    readonly property var theme: themes[themeName]
    Material.theme: theme.material
    Material.primary: theme.primary
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    color: theme.background

    readonly property bool dark: theme.material === Material.Dark
    readonly property var colours: theme.colours
    // Line kinds of the chat (tx, rx, info) and log levels (debug, info,
    // success, warning, error) mapped onto the theme.
    function colourOf(key) {
        switch (key) {
        case "info": return theme.foreground
        case "success": return colours.clear
        case "warning": return colours.beacon
        case "debug": return colours.dim
        }
        return colours[key] !== undefined ? colours[key] : colours["system"]
    }

    // ---- requests from the controller ------------------------------------
    // One dialog at a time; the others wait their turn.
    property var queue: []
    property var current: null
    function enqueue(request) {
        queue.push(request)
        if (current === null)
            showNext()
    }
    function showNext() {
        current = queue.length > 0 ? queue.shift() : null
        if (current === null)
            return
        if (current.type === "message") {
            messageBox.title = current.title
            messageBox.icon = current.icon
            messageBox.text = current.text
            messageBox.informative = current.informative
            messageBox.question = current.question
            messageBox.defaultYes = current.defaultYes
            messageBox.open()
        } else if (current.type === "password") {
            passwordDialog.title = current.title
            passwordDialog.prompt = current.prompt
            passwordDialog.open()
        } else if (current.type === "save") {
            saveDialog.title = current.title
            saveDialog.currentFolder = current.folder
            saveDialog.selectedFile = current.folder + "/" + encodeURIComponent(current.fileName)
            saveDialog.open()
        }
    }
    function requestDone() {
        current = null
        Qt.callLater(showNext)
    }

    Connections {
        target: Controller
        function onMessageRequested(id, icon, title, text, informative, question, defaultYes) {
            window.enqueue({ "type": "message", "id": id, "icon": icon, "title": title, "text": text,
                             "informative": informative, "question": question, "defaultYes": defaultYes })
        }
        function onPasswordRequested(id, title, prompt) {
            window.enqueue({ "type": "password", "id": id, "title": title, "prompt": prompt })
        }
        function onSaveFileRequested(id, title, fileName, folder) {
            window.enqueue({ "type": "save", "id": id, "title": title, "fileName": fileName, "folder": folder })
        }
        function onBbsWindowRequested(title) {
            bbsDialog.title = title
            bbsDialog.open()
        }
        function onQsoLogRequested() { qsoDialog.open() }
        function onStationFieldsRequested(callsign, via1, via2) {
            stationField.text = callsign
            if (via1.length > 0) via1Field.text = via1
            if (via2.length > 0) via2Field.text = via2
        }
        function onStateChanged() {
            // Digipeaters: VARA FM only; the fields empty otherwise.
            if (!Controller.digipeatersEnabled && Controller.modemType.length > 0) {
                via1Field.text = ""
                via2Field.text = ""
            }
        }
    }

    MessageBox {
        id: messageBox
        onAnswered: (yes) => {
            if (window.current && window.current.type === "message")
                Controller.answer(window.current.id, yes)
            window.requestDone()
        }
    }
    PasswordDialog {
        id: passwordDialog
        onAnswered: (ok, text) => {
            if (window.current && window.current.type === "password")
                Controller.answerText(window.current.id, ok, text)
            window.requestDone()
        }
    }
    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: [window.s.file_all_files + " (*)"]
        onAccepted: { Controller.answerFile(window.current.id, selectedFile); window.requestDone() }
        onRejected: { Controller.answerFile(window.current.id, ""); window.requestDone() }
    }
    FileDialog {
        id: sendDialog
        title: window.s.file_select
        fileMode: FileDialog.OpenFile
        nameFilters: [window.s.file_all_files + " (*)"]
        onAccepted: Controller.sendFile(selectedFile)
    }
    SettingsDialog { id: settingsDialog }
    MacrosDialog { id: macrosDialog }
    AboutDialog { id: aboutDialog }
    TableDialog {
        id: qsoDialog
        title: window.s.dlg_qso_log
        model: Controller.qsoModel
        columns: [{ "title": window.s.col_callsign, "role": "callsign", "stretch": 1 },
                  { "title": window.s.col_start, "role": "start", "stretch": 1.3 },
                  { "title": window.s.col_end, "role": "end", "stretch": 1.3 },
                  { "title": window.s.col_duration, "role": "duration", "stretch": 0.8 }]
    }
    TableDialog {
        id: bbsDialog
        model: Controller.bbsModel
        info: window.s.bbs_double_click_info
        showRefresh: true
        columns: [{ "title": window.s.lbl_filename, "role": "name", "stretch": 3 },
                  { "title": window.s.lbl_size, "role": "size", "stretch": 1 }]
        onActivated: (row) => {
            Controller.bbsDownload(Controller.bbsModel.cell(row, 0))
            close()
        }
        onRefresh: {
            Controller.bbsRefresh()
            close()
        }
    }

    readonly property bool popupOpen: messageBox.opened || passwordDialog.opened || settingsDialog.opened
                                      || macrosDialog.opened || aboutDialog.opened || qsoDialog.opened
                                      || bbsDialog.opened

    // F1-F12: the macros.
    Shortcut { sequence: "F1"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(1) }
    Shortcut { sequence: "F2"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(2) }
    Shortcut { sequence: "F3"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(3) }
    Shortcut { sequence: "F4"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(4) }
    Shortcut { sequence: "F5"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(5) }
    Shortcut { sequence: "F6"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(6) }
    Shortcut { sequence: "F7"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(7) }
    Shortcut { sequence: "F8"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(8) }
    Shortcut { sequence: "F9"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(9) }
    Shortcut { sequence: "F10"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(10) }
    Shortcut { sequence: "F11"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(11) }
    Shortcut { sequence: "F12"; enabled: !window.popupOpen; onActivated: Controller.sendMacro(12) }

    onClosing: Controller.shutdown()

    // ---- menus --------------------------------------------------------------
    menuBar: MenuBar {
        Material.background: window.theme.surface
        Menu {
            title: window.s.menu_file
            MenuItem { text: window.s.btn_save_chat; onTriggered: Controller.saveChat() }
            MenuItem { text: window.s.menu_save_log; onTriggered: Controller.saveLog() }
            MenuSeparator {}
            MenuItem { text: window.s.btn_settings; onTriggered: { settingsDialog.load(); settingsDialog.open() } }
            MenuSeparator {}
            MenuItem { text: window.s.menu_quit; onTriggered: window.close() }
        }
        Menu {
            id: toolsMenu
            title: window.s.menu_tools
            MenuItem { text: window.s.menu_qso_log; onTriggered: Controller.showQsoLog() }
            MenuItem { text: window.s.menu_macros; onTriggered: { macrosDialog.load(); macrosDialog.open() } }
            MenuSeparator {}
            Menu {
                title: window.s.menu_theme
                Repeater {
                    model: [["light", window.s.theme_light], ["dark", window.s.theme_dark],
                            ["red", window.s.theme_red], ["amber", window.s.theme_amber]]
                    delegate: MenuItem {
                        text: modelData[1]
                        checkable: true
                        checked: window.themeName === modelData[0]
                        onTriggered: Controller.setTheme(modelData[0])
                    }
                }
            }
            MenuItem {
                text: Controller.tooltips ? window.s.tooltips_enabled : window.s.tooltips_disabled
                onTriggered: Controller.toggleTooltips()
            }
        }
        Menu {
            title: window.s.menu_help
            MenuItem { text: window.s.menu_about; onTriggered: aboutDialog.open() }
        }
    }

    // ---- status bar -----------------------------------------------------
    footer: Pane {
        padding: 4
        leftPadding: 10
        rightPadding: 10
        Material.elevation: 6
        Material.background: window.theme.surface
        RowLayout {
            anchors.fill: parent
            spacing: 18
            Label {
                text: Controller.modemConnected ? "🟢 " + window.s.status_connected
                      : Controller.modemConnecting ? "🟡 " + window.s.status_connecting
                      : "⚫ " + window.s.status_disconnected
                color: Controller.modemConnected ? window.colourOf("clear")
                       : Controller.modemConnecting ? window.colourOf("busy") : window.colourOf("error")
            }
            Item { Layout.fillWidth: true }
            Label {
                text: Controller.ptt ? "PTT: ON" : "PTT: OFF"
                color: Controller.ptt ? window.colourOf("transmitting") : window.colourOf("dim")
                font.bold: Controller.ptt
            }
            Label {
                text: Controller.sessionActive ? window.s.session + ": " + Controller.remoteStation
                                               : window.s.no_session
                color: Controller.sessionActive ? window.colourOf("clear") : window.colourOf("dim")
                font.bold: Controller.sessionActive
            }
        }
    }

    // ---- toolbar and panels ---------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            CButton {
                text: Controller.modemConnected ? window.s.btn_disconnect_modem
                      : Controller.modemConnecting ? window.s.btn_cancel_connect : window.s.btn_connect
                tip: window.s.tt_connect_btn
                onClicked: Controller.toggleModem()
            }
            Label { text: window.s.lbl_station_short; Layout.leftMargin: 6 }
            CField {
                id: stationField
                leftPadding: 8
                rightPadding: 8
                Layout.fillWidth: true
                Layout.maximumWidth: 150
                Layout.minimumWidth: 80
                hint: window.s.placeholder_station
                onTextEdited: { const p = cursorPosition; text = text.toUpperCase(); cursorPosition = p }
                onAccepted: if (callButton.enabled) callButton.clicked()
            }
            Label { text: "Via1:" }
            CField {
                id: via1Field
                leftPadding: 8
                rightPadding: 8
                Layout.fillWidth: true
                Layout.maximumWidth: 110
                Layout.minimumWidth: 56
                enabled: Controller.digipeatersEnabled
                hint: Controller.digipeatersEnabled || Controller.modemType.length === 0
                      ? window.s.placeholder_digi1 : window.s.via_na
                tip: Controller.digipeatersEnabled || Controller.modemType.length === 0
                     ? window.s.tt_via1 : window.s.tt_via_unsupported.replace("%1", Controller.modemType)
                onTextEdited: { const p = cursorPosition; text = text.toUpperCase(); cursorPosition = p }
            }
            Label { text: "Via2:" }
            CField {
                id: via2Field
                leftPadding: 8
                rightPadding: 8
                Layout.fillWidth: true
                Layout.maximumWidth: 110
                Layout.minimumWidth: 56
                enabled: Controller.digipeatersEnabled
                hint: Controller.digipeatersEnabled || Controller.modemType.length === 0
                      ? window.s.placeholder_digi2 : window.s.via_na
                tip: Controller.digipeatersEnabled || Controller.modemType.length === 0
                     ? window.s.tt_via2 : window.s.tt_via_unsupported.replace("%1", Controller.modemType)
                onTextEdited: { const p = cursorPosition; text = text.toUpperCase(); cursorPosition = p }
            }
            Label { text: "BW:" }
            CCombo {
                id: bandwidthCombo
                Layout.preferredWidth: 118
                model: Controller.bandwidths
                enabled: Controller.bandwidthEnabled
                tip: Controller.modemType === "VARA FM" ? window.s.tt_bw_fm
                     : Controller.modemType === "VARA SAT" ? window.s.tt_bw_sat
                     : Controller.bandwidthEnabled ? window.s.tt_bw_select : ""
                onModelChanged: currentIndex = Controller.defaultBandwidthIndex
            }
            CButton {
                id: callButton
                text: window.s.btn_call
                tip: window.s.tt_call_btn
                enabled: Controller.modemConnected && !Controller.sessionActive
                onClicked: Controller.callStation(stationField.text, via1Field.text, via2Field.text,
                                                  bandwidthCombo.currentIndex)
            }
            CButton {
                text: window.s.btn_disconnect
                tip: window.s.tt_disconnect_btn
                enabled: Controller.sessionActive
                onClicked: Controller.disconnectSession()
            }
            Item { Layout.fillWidth: true }
            CButton {
                text: "🔒 " + window.s.btn_encryption
                tip: window.s.tt_crypto_btn
                lit: Controller.cryptoOn
                enabled: Controller.sessionActive
                onClicked: Controller.toggleCrypto()
            }
            CButton {
                text: "📁 " + window.s.bbs_button
                tip: window.s.bbs_button_tooltip
                enabled: Controller.remoteBbs
                onClicked: Controller.openBbs()
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SplitView {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 220
                orientation: Qt.Horizontal

                // Chat, 70 %
                Panel {
                    title: window.s.group_chat
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 360

                    LineView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: Controller.chatModel
                        monospace: true
                        tip: window.s.tt_chat_display
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        CField {
                            id: messageField
                            Layout.fillWidth: true
                            hint: window.s.placeholder_message
                            enabled: Controller.sessionActive
                            tip: window.s.tt_message_input
                            onAccepted: if (Controller.sendMessage(text)) clear()
                        }
                        CButton {
                            text: window.s.btn_send
                            tip: window.s.tt_send_btn
                            enabled: Controller.sessionActive
                            onClicked: if (Controller.sendMessage(messageField.text)) messageField.clear()
                        }
                        CButton {
                            text: "📎 " + window.s.btn_file
                            tip: window.s.tt_file_btn
                            enabled: Controller.sessionActive && !Controller.transferBusy
                            onClicked: sendDialog.open()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: Controller.transferText
                        elide: Text.ElideRight
                        font.italic: Controller.transferState === "idle"
                        font.bold: Controller.transferState !== "idle"
                        color: Controller.transferState === "active" ? window.colourOf("rx")
                               : Controller.transferState === "done" ? window.colourOf("clear")
                               : window.colourOf("dim")
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: Controller.transferVisible
                        from: 0
                        to: Math.max(1, Controller.transferMaximum)
                        value: Controller.transferValue
                    }
                }

                // Received CQ, 30 %
                Panel {
                    title: window.s.group_cq
                    SplitView.preferredWidth: window.width * 0.3
                    SplitView.minimumWidth: 260

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        CButton {
                            text: "📢 " + window.s.btn_send_cq
                            tip: window.s.tt_cq_btn
                            enabled: Controller.modemConnected && !Controller.sessionActive
                            onClicked: Controller.sendCq(via1Field.text, via2Field.text, bandwidthCombo.currentIndex)
                        }
                        CButton {
                            text: "🗑️ " + window.s.btn_clear_cq
                            tip: window.s.tt_clear_cq
                            onClicked: Controller.clearCq()
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: window.theme.field
                        border.color: window.theme.line
                        radius: 4

                        HoverHandler { id: cqHover }
                        Tip { shown: cqHover.hovered; text: window.s.tt_cq_table; delay: 900 }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 1
                            spacing: 0
                            Rectangle {
                                Layout.fillWidth: true
                                height: 26
                                color: window.theme.button
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    Label { text: window.s.col_callsign; font.bold: true; Layout.fillWidth: true; color: window.theme.buttonText }
                                    Label { text: "BW"; font.bold: true; Layout.preferredWidth: 46; color: window.theme.buttonText }
                                    Label { text: "Via"; font.bold: true; Layout.preferredWidth: 70; color: window.theme.buttonText }
                                    Label { text: window.s.col_time; font.bold: true; Layout.preferredWidth: 62; color: window.theme.buttonText }
                                }
                            }
                            ListView {
                                id: cqList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                currentIndex: -1
                                model: Controller.cqModel
                                ScrollBar.vertical: ScrollBar {}
                                delegate: Rectangle {
                                    id: cqRow
                                    required property int index
                                    required property string callsign
                                    required property string bandwidth
                                    required property string via
                                    required property string time
                                    width: ListView.view.width
                                    height: 26
                                    color: ListView.isCurrentItem ? window.theme.selection : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        Label { text: cqRow.callsign; Layout.fillWidth: true; elide: Text.ElideRight; font.family: window.monoFamily }
                                        Label { text: cqRow.bandwidth; Layout.preferredWidth: 46 }
                                        Label { text: cqRow.via; Layout.preferredWidth: 70; elide: Text.ElideRight }
                                        Label { text: cqRow.time; Layout.preferredWidth: 62; font.family: window.monoFamily }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: cqList.currentIndex = cqRow.index
                                        onDoubleClicked: {
                                            cqList.currentIndex = cqRow.index
                                            Controller.connectToCq(cqRow.index, bandwidthCombo.currentIndex)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // System log
            Panel {
                title: window.s.group_system_log
                SplitView.preferredHeight: 170
                SplitView.minimumHeight: 80
                LineView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: Controller.logModel
                    tip: window.s.tt_log_display
                }
            }
        }
    }

    // ---- screenshots (--screenshot DIR) ------------------------------------
    function screenshotMode(dir) {
        Controller.loadDemo()
        shooter.dir = dir
        shooter.start()
    }
    Timer {
        id: shooter
        property string dir: ""
        property int step: 0
        readonly property var steps: ["light", "dark", "red", "amber", "settings", "question", "menu-red",
                                      "settings-station", "settings-auto", "settings-interface", "settings-bbs",
                                      "macros"]
        interval: 700
        repeat: true
        onTriggered: {
            if (step > 0)
                Ui.grabWindow(window, dir + "/cryptara-" + steps[step - 1] + ".png")
            settingsDialog.close()
            messageBox.close()
            toolsMenu.close()
            macrosDialog.close()
            if (step >= steps.length) {
                stop()
                Qt.quit()
                return
            }
            const name = steps[step]
            if (name === "settings") {
                Controller.setTheme("dark")
                settingsDialog.load()
                settingsDialog.open()
            } else if (name === "question") {
                Controller.setTheme("light")
                Controller.demoQuestion()
            } else if (name.startsWith("settings-")) {
                Controller.setTheme("light")
                settingsDialog.load()
                settingsDialog.open()
                const tab = ["modem", "station", "auto", "interface", "bbs"].indexOf(name.substring(9))
                settingsDialog.showTab(tab, name === "settings-auto")
            } else if (name === "macros") {
                Controller.setTheme("light")
                macrosDialog.load()
                macrosDialog.open()
            } else if (name === "menu-red") {
                Controller.setTheme("red")
                toolsMenu.open()
            } else {
                Controller.setTheme(name)
            }
            step++
        }
    }
}
